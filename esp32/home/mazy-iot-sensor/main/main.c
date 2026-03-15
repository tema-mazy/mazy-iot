/**
   Basic HK temperature, Humidity & CO2 sensor

   Copyright 2025 Achim Pieters | StudioPieters®

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

   For more information, visit https://www.studiopieters.nl
 **/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "esp_intr_alloc.h"
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/uart.h>

#include <homekit/characteristics.h>
#include <homekit/homekit.h>

#define UART_NUM UART_NUM_1
#define TXD_PIN CONFIG_UART_TX
#define RXD_PIN CONFIG_UART_RX

#define I2C_MASTER_SCL_IO                                                      \
  CONFIG_I2C_MASTER_SCL /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO                                                      \
  CONFIG_I2C_MASTER_SDA          /*!< gpio number for I2C master data */
#define I2C_MASTER_NUM I2C_NUM_0 /*!< I2C port number for master bme280 */
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_FREQ_HZ 100000 /*!< I2C master clock frequency */
#define SHT21_SENSOR_ADDR 0x40

// Thresholds for HomeKit delta notifications
#define TEMP_NOTIFY_THRESHOLD 0.5f
#define HUMIDITY_NOTIFY_THRESHOLD 1.0f

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t sht21_dev = NULL;
static bool homekit_initialized = false;
static int s_retry_num = 0;

// FIX #9: Separate spinlock for HomeKit characteristic writes
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

#define BUTTON_GPIO GPIO_NUM_9

// Define device characteristics
#define ACCESSORY_NAME "MIOT THC Sensor"
#define ACCESSORY_SN "SN000001"
#define DEVICE_MANUFACTURER "Mazy's Wünderwafle"
#define DEVICE_SERIAL "THC000001"
#define DEVICE_MODEL "MIOT32/THC/v1"
#define FW_VERSION "26.03.15.1"

char ssid[40] = "MIoT32/THC ";
char ssn[20] = "000000000000";
uint8_t mac[6] = {0};

const uint8_t co2_cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x79};
uint8_t response[9];

const int periodic_interval = 60;
const int ALERT = 2000;

// Queue to send button events from ISR to task
static QueueHandle_t gpio_evt_queue = NULL;

#define CHECK_ERROR(x)                                                         \
  do {                                                                         \
    esp_err_t __err_rc = (x);                                                  \
    if (__err_rc != ESP_OK) {                                                  \
      ESP_LOGE("INFORMATION", "Error: %s", esp_err_to_name(__err_rc));         \
      handle_error(__err_rc);                                                  \
    }                                                                          \
  } while (0)

void handle_error(esp_err_t err) {
  switch (err) {
  case ESP_ERR_WIFI_NOT_STARTED:
  case ESP_ERR_WIFI_CONN:
    ESP_LOGI("INFORMATION", "WiFi Error, attempting reconnect...");
    esp_wifi_connect();
    break;
  default:
    ESP_LOGE("ERROR", "Critical error, restarting device...");
    esp_restart();
    break;
  }
}

static void IRAM_ATTR gpio_isr_handler(void *arg) {
  uint32_t gpio_num = (uint32_t)arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void on_wifi_ready();

// FIX #4: WiFi reconnect handled without blocking the event loop
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      ESP_LOGI("WiFI", "WiFi started");
      esp_wifi_connect();
      break;
    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI("WiFi", "WiFi connected");
      break;
    case WIFI_EVENT_STA_DISCONNECTED: {
      wifi_event_sta_disconnected_t *disconnected =
          (wifi_event_sta_disconnected_t *)event_data;
      ESP_LOGE("WiFi", "WiFi disconnected, reason: %d", disconnected->reason);

      s_retry_num++;
      if (s_retry_num < 5) {
        // FIX #4: Removed vTaskDelay() — do NOT block the event loop.
        // The WiFi driver manages its own retry backoff internally.
        ESP_LOGI("WiFi", "Retrying to connect to the AP (Attempt %d)",
                 s_retry_num);
        esp_wifi_connect();
      } else {
        ESP_LOGE("WiFi", "Failed to connect to the AP, restarting WiFi driver");
        s_retry_num = 0;
        // FIX #4: Removed vTaskDelay() before esp_wifi_start().
        // esp_wifi_stop() posts an async stop event; esp_wifi_start()
        // is safe to call immediately after.
        esp_wifi_stop();
        esp_wifi_start();
      }
      break;
    }
    case WIFI_EVENT_STA_STOP:
      ESP_LOGI("WiFi", "WiFi stopped");
      esp_wifi_start();
      break;
    default:
      break;
    }

  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI("WiFi", "WiFi connected, IP obtained");
    s_retry_num = 0;
    on_wifi_ready();
  }
}

static void wifi_init() {
  CHECK_ERROR(esp_netif_init());
  CHECK_ERROR(esp_event_loop_create_default());
  esp_netif_t *netif = esp_netif_create_default_wifi_sta();

  ESP_ERROR_CHECK(esp_netif_set_hostname(netif, ssid));

  CHECK_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         &event_handler, NULL));
  CHECK_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         &event_handler, NULL));

  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  CHECK_ERROR(esp_wifi_init(&wifi_init_config));
  CHECK_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  ESP_LOGI("WiFi", "Connecting to AP: %s", CONFIG_ESP_WIFI_SSID);

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = CONFIG_ESP_WIFI_SSID,
              .password = CONFIG_ESP_WIFI_PASSWORD,
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          },
  };

  CHECK_ERROR(esp_wifi_set_mode(WIFI_MODE_STA));
  CHECK_ERROR(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  CHECK_ERROR(esp_wifi_start());
  ESP_LOGI("WiFi", "Wünderwafle started sn: %s, hostname: %s", ssn, ssid);
}

#define LED_GPIO CONFIG_ESP_LED_GPIO
static bool led_on = false;

void led_write(bool on) { gpio_set_level(LED_GPIO, on ? 1 : 0); }

void gpio_init() {
  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
  led_write(led_on);
}

// FIX #2: Safely re-initialize UART by deleting the driver first
void co2_init() {
  // Delete existing driver if already installed (safe to call even if not
  // installed)
  uart_driver_delete(UART_NUM);

  const uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };
  uart_param_config(UART_NUM, &uart_config);
  uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
  uart_driver_install(UART_NUM, 256, 0, 0, NULL, 0);

  ESP_LOGI("CO2", "UART Initialized.");
}

void accessory_identify_task(void *args) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 2; j++) {
      led_write(true);
      vTaskDelay(pdMS_TO_TICKS(100));
      led_write(false);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  led_write(led_on);
  vTaskDelete(NULL);
}

void accessory_identify(homekit_value_t _value) {
  ESP_LOGI("INFORMATION", "Accessory identify");
  // FIX #8: Increased stack from configMINIMAL_STACK_SIZE to *2
  xTaskCreate(accessory_identify_task, "Accessory identify",
              configMINIMAL_STACK_SIZE * 2, NULL, 2, NULL);
}

homekit_characteristic_t cha_name =
    HOMEKIT_CHARACTERISTIC_(NAME, ACCESSORY_NAME);
homekit_characteristic_t cha_sn =
    HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, ACCESSORY_SN);
homekit_characteristic_t cha_temperature =
    HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t cha_humidity =
    HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);
homekit_characteristic_t cha_co2 =
    HOMEKIT_CHARACTERISTIC_(CARBON_DIOXIDE_LEVEL, 0);
homekit_characteristic_t cha_co2_alert =
    HOMEKIT_CHARACTERISTIC_(CARBON_DIOXIDE_DETECTED, false);
homekit_characteristic_t cha_air = HOMEKIT_CHARACTERISTIC_(AIR_QUALITY, 0);

void i2c_init() {
  i2c_master_bus_config_t bus_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_NUM_0,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = SHT21_SENSOR_ADDR,
      .scl_speed_hz = I2C_MASTER_FREQ_HZ,
  };

  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &sht21_dev));

  ESP_LOGI("I2C", "Initialized.");
}

// FIX #3: Button task — consumes events from the ISR queue
static void gpio_task(void *arg) {
  uint32_t gpio_num;
  while (1) {
    if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY)) {
      ESP_LOGI("BUTTON", "Button pressed on GPIO %lu", gpio_num);
      // Trigger accessory identify on button press
      accessory_identify(HOMEKIT_BOOL(true));
    }
  }
}

void co_sensor_task(void *pvParameters) {
  // FIX #12: Start ppm at 0; first real reading is accepted directly
  int ppm = 0;
  bool first_reading = true;
  int airq = 0;
  bool alerted = false;

  while (1) {
    // FIX #5: Removed dead elapsed_time counter — just read every iteration
    uart_write_bytes(UART_NUM, (const char *)co2_cmd, 9);
    vTaskDelay(pdMS_TO_TICKS(500));

    int len = uart_read_bytes(UART_NUM, response, 9, pdMS_TO_TICKS(1000));

    if (len == 9 && response[0] == 0xFF && response[1] == 0x86) {
      int ppm_raw = (response[2] << 8) | response[3];

      // FIX #12: Accept first reading directly; average thereafter
      if (first_reading) {
        ppm = ppm_raw;
        first_reading = false;
      } else {
        ppm = (ppm_raw + ppm) / 2;
      }
    } else {
      ESP_LOGE("CO2", "UART Read error, resetting driver");
      uart_flush_input(UART_NUM);
      vTaskDelay(pdMS_TO_TICKS(500));
      // FIX #2: co2_init() now safely deletes the driver before reinstalling
      co2_init();
      // Skip HomeKit update this cycle; wait full interval before retrying
      vTaskDelay(pdMS_TO_TICKS((periodic_interval * 1000) - 1000));
      continue;
    }

    // Calculate air quality
    if (ppm > 1300) {
      airq = 5;
    } else if (ppm > 1000) {
      airq = 4;
    } else if (ppm > 800) {
      airq = 3;
    } else if (ppm > 600) {
      airq = 2;
    } else if (ppm > 400) {
      airq = 1;
    } else {
      airq = 0;
    }

    alerted = ppm > ALERT;

    // Batch HomeKit updates with critical section
    portENTER_CRITICAL(&spinlock);
    {
      cha_co2.value.float_value = ppm;
      cha_air.value.int_value = airq;
      cha_co2_alert.value.bool_value = alerted;
    }
    portEXIT_CRITICAL(&spinlock);

    homekit_characteristic_notify(&cha_co2, cha_co2.value);
    vTaskDelay(pdMS_TO_TICKS(25));
    homekit_characteristic_notify(&cha_air, cha_air.value);
    vTaskDelay(pdMS_TO_TICKS(25));
    homekit_characteristic_notify(&cha_co2_alert, cha_co2_alert.value);

    ESP_LOGI("INFORMATION", "CO2 %dppm, Alert: %d, AirQ: %d", ppm, alerted,
             airq);

    // Wait for the next polling interval
    vTaskDelay(pdMS_TO_TICKS(periodic_interval * 1000));
  }
}

void th_sensor_task(void *pvParameters) {
  uint8_t data[2];
  uint16_t raw_temp, raw_humidity;
  // FIX #10: Track previous values for delta-based notifications
  float temp = 0.0f, humidity = 0.0f;
  float prev_temp = -999.0f, prev_humidity = -999.0f;
  bool first_th_reading = true;

  while (1) {
    // --- Read temperature ---
    uint8_t cmd = 0xF3;
    esp_err_t err =
        i2c_master_transmit(sht21_dev, &cmd, 1, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
      ESP_LOGE("I2C", "Sensor T transmit error: %s", esp_err_to_name(err));
      // FIX #1: No 'continue' here — add a real delay before next attempt
      vTaskDelay(pdMS_TO_TICKS(periodic_interval * 1000));
      continue;
    }

    vTaskDelay(pdMS_TO_TICKS(85)); // SHT21 max measurement time for 14-bit temp

    err = i2c_master_receive(sht21_dev, data, 2, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
      ESP_LOGE("I2C", "Sensor T receive error: %s", esp_err_to_name(err));
      // FIX #1: Proper delay prevents busy-loop on persistent sensor fault
      vTaskDelay(pdMS_TO_TICKS(periodic_interval * 1000));
      continue;
    }

    raw_temp = (data[0] << 8) | (data[1] & 0xFC); // mask status bits
    temp = -46.85f + 175.72f * ((float)raw_temp / 65536.0f);

    // --- Read humidity ---
    cmd = 0xF5;
    err = i2c_master_transmit(sht21_dev, &cmd, 1, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
      ESP_LOGE("I2C", "Sensor H transmit error: %s", esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(periodic_interval * 1000));
      continue;
    }

    vTaskDelay(pdMS_TO_TICKS(29)); // SHT21 max measurement time for 12-bit RH

    err = i2c_master_receive(sht21_dev, data, 2, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
      ESP_LOGE("I2C", "Sensor H receive error: %s", esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(periodic_interval * 1000));
      continue;
    }

    raw_humidity = (data[0] << 8) | (data[1] & 0xFC); // mask status bits
    humidity = -6.0f + 125.0f * ((float)raw_humidity / 65536.0f);

    // Clamp to valid sensor range
    if (humidity < 0.0f)
      humidity = 0.0f;
    if (humidity > 100.0f)
      humidity = 100.0f;

    // FIX #9 + #10: Update characteristics in a critical section;
    // notify HomeKit only when values change beyond threshold (or first read)
    bool notify_temp =
        first_th_reading || (fabsf(temp - prev_temp) >= TEMP_NOTIFY_THRESHOLD);
    bool notify_hum = first_th_reading || (fabsf(humidity - prev_humidity) >=
                                           HUMIDITY_NOTIFY_THRESHOLD);

    portENTER_CRITICAL(&spinlock);
    {
      cha_temperature.value.float_value = temp;
      cha_humidity.value.float_value = humidity;
    }
    portEXIT_CRITICAL(&spinlock);

    if (notify_temp) {
      homekit_characteristic_notify(&cha_temperature, cha_temperature.value);
      prev_temp = temp;
      vTaskDelay(pdMS_TO_TICKS(25));
    }
    if (notify_hum) {
      homekit_characteristic_notify(&cha_humidity, cha_humidity.value);
      prev_humidity = humidity;
    }

    first_th_reading = false;

    ESP_LOGI("INFORMATION", "Temp: %.2f°C, Humidity: %.2f%%", temp, humidity);

    vTaskDelay(pdMS_TO_TICKS(periodic_interval * 1000));
  }
}

void th_sensor_init_task() {
  // FIX #7: Increased stack to 4096 bytes for floating-point + logging overhead
  xTaskCreate(th_sensor_task, "TH Sensor Task", 4096, NULL, 5, NULL);
}

void co_sensor_init_task() {
  // FIX #7: Increased stack to 4096 bytes for UART + logging overhead
  xTaskCreate(co_sensor_task, "CO2 Sensor Task", 4096, NULL, 5, NULL);
}

homekit_characteristic_t manufacturer =
    HOMEKIT_CHARACTERISTIC_(MANUFACTURER, DEVICE_MANUFACTURER);
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, DEVICE_MODEL);
homekit_characteristic_t revision =
    HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, FW_VERSION);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
            .id = 1, .category = homekit_accessory_category_sensors,
            .services =
                (homekit_service_t *[]){
                    HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                                    .characteristics =
                                        (homekit_characteristic_t *[]){
                                            &cha_name, &manufacturer, &cha_sn,
                                            &model, &revision,
                                            HOMEKIT_CHARACTERISTIC(
                                                IDENTIFY, accessory_identify),
                                            NULL}),
                    HOMEKIT_SERVICE(
                        TEMPERATURE_SENSOR, .primary = true,
                        .characteristics =
                            (homekit_characteristic_t *[]){
                                HOMEKIT_CHARACTERISTIC(NAME, "Temperature"),
                                &cha_temperature, NULL}),
                    HOMEKIT_SERVICE(
                        HUMIDITY_SENSOR,
                        .characteristics =
                            (homekit_characteristic_t *[]){
                                HOMEKIT_CHARACTERISTIC(NAME, "Humidity"),
                                &cha_humidity, NULL}),
                    HOMEKIT_SERVICE(
                        AIR_QUALITY_SENSOR,
                        .characteristics =
                            (homekit_characteristic_t *[]){
                                HOMEKIT_CHARACTERISTIC(NAME, "Air quality"),
                                &cha_air, &cha_co2, &cha_co2_alert, NULL}),
                    NULL}),
    NULL};
#pragma GCC diagnostic pop

homekit_server_config_t config = {
    .accessories = accessories,
    .password = CONFIG_ESP_SETUP_CODE,
    .setupId = CONFIG_ESP_SETUP_ID,
};

static void on_wifi_ready() {
  if (homekit_initialized) {
    ESP_LOGI("INFORMATION", "HomeKit already initialized, skipping...");
    return;
  }
  ESP_LOGI("INFORMATION", "Starting HomeKit server...");
  cha_name.value = HOMEKIT_STRING(ssid);
  cha_sn.value = HOMEKIT_STRING(ssn);
  homekit_server_init(&config);
  homekit_initialized = true;
}

void app_main(void) {
  // FIX #6: Reduced initial boot delay from 10s → 3s (power rail stabilization)
  vTaskDelay(pdMS_TO_TICKS(3000));

  ESP_LOGI("INFORMATION", "Initializing UART...");
  co2_init();

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW("WARNING", "NVS flash initialization failed, erasing...");
    CHECK_ERROR(nvs_flash_erase());
    ret = nvs_flash_init();
    CHECK_ERROR(ret);
  }

  CHECK_ERROR(esp_efuse_mac_get_default(mac));
  sprintf(ssid, "MIOT32-THC-%02x%02x%02x", mac[3], mac[4], mac[5]);
  sprintf(ssn, "THC/%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);

  wifi_init();
  gpio_init();

  // FIX #3: Create queue and button ISR handler, then spawn the consumer task
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << BUTTON_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .intr_type = GPIO_INTR_NEGEDGE,
  };
  gpio_config(&io_conf);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);
  xTaskCreate(gpio_task, "GPIO Task", configMINIMAL_STACK_SIZE * 2, NULL, 3,
              NULL);

  ESP_LOGI("INFORMATION", "Initializing sensors...");
  i2c_init();

  // FIX #6: Removed excessive inter-task delays; tasks start immediately
  ESP_LOGI("INFORMATION", "Starting Tasks...");
  th_sensor_init_task();
  ESP_LOGI("INFORMATION", "TH Task created.");
  co_sensor_init_task();
  ESP_LOGI("INFORMATION", "CO2 Task created.");
}
