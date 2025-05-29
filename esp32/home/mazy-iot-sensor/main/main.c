/**
   Basic HK temperature, Humidity & CO2 sensor

   Copyright 2025 Achim Pieters | StudioPieters®

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   For more information, visit https://www.studiopieters.nl
 **/

#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <math.h> // Include for fabs
#include <string.h>
#include <esp_mac.h>
#include <driver/i2c.h>
#include <driver/uart.h>
#include <driver/gpio.h>

#define UART_NUM UART_NUM_1
#define TXD_PIN CONFIG_UART_TX
#define RXD_PIN CONFIG_UART_RX

#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL           /*!< gpio number for I2C master clock IO9*/
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA           /*!< gpio number for I2C master data  IO8*/
#define I2C_MASTER_NUM              I2C_NUM_0            /*!< I2C port number for master bme280 */
#define I2C_MASTER_TX_BUF_DISABLE   0                    /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                    /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000               /*!< I2C master clock frequency */
#define SHT21_SENSOR_ADDR    0x40   

// Define device characteristics
#define ACCESSORY_NAME "MIOT THC Sensor"
#define ACCESSORY_SN "SN000001"
#define DEVICE_MANUFACTURER "Mazy's Wünderwafle"
#define DEVICE_SERIAL "THC000001"
#define DEVICE_MODEL "MIOT32/THC/v1"
#define FW_VERSION "0.0.1"

char ssid[40] = "MIoT32/THC " ;
char ssn[20] = "000000000000" ;
uint8_t mac[6] = {0};

const uint8_t co2_cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
uint8_t response[9];

const uint8_t cmd_h = 0xF5; // Humidity command
const uint8_t cmd_t = 0xF3; // Temp command

const int periodic_interval = 30;
const int ALERT = 2000;


#define CHECK_ERROR(x) do {                          \
                esp_err_t __err_rc = (x);            \
                if (__err_rc != ESP_OK) {            \
                        ESP_LOGE("INFORMATION", "Error: %s", esp_err_to_name(__err_rc)); \
                        handle_error(__err_rc);      \
                }                                    \
} while(0)

void handle_error(esp_err_t err) {
    switch (err) {
    case ESP_ERR_WIFI_NOT_STARTED:
    case ESP_ERR_WIFI_CONN:
        ESP_LOGI("INFORMATION", "Restarting WiFi...");
        esp_wifi_stop();
        esp_wifi_start();
        break;
    default:
        ESP_LOGE("ERROR", "Critical error, restarting device...");
        esp_restart();
        break;
    }
}

static void on_wifi_ready();

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI("WiFi", "Connecting ...");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI("WiFi", "WiFi connected, IP obtained");
        on_wifi_ready();
    }
}

static void wifi_init() {
    CHECK_ERROR(esp_netif_init());
    CHECK_ERROR(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();


    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, ssid));
    
    CHECK_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    CHECK_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    CHECK_ERROR(esp_wifi_init(&wifi_init_config));
    CHECK_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    CHECK_ERROR(esp_wifi_set_mode(WIFI_MODE_STA));
    CHECK_ERROR(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    CHECK_ERROR(esp_wifi_start());
    ESP_LOGI("WiFi", "Wünderwafle is started sn: %s, hostname: %s ", ssn, ssid);

}

#define LED_GPIO CONFIG_ESP_LED_GPIO
static bool led_on = false;

void led_write(bool on) {
    gpio_set_level(LED_GPIO, on ? 1 : 0);
}

void gpio_init() {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    led_write(led_on);
}
void co2_init() {
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
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
    xTaskCreate(accessory_identify_task, "Accessory identify", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
}
homekit_characteristic_t cha_name = HOMEKIT_CHARACTERISTIC_(NAME, ACCESSORY_NAME);
homekit_characteristic_t cha_sn = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, ACCESSORY_SN);
homekit_characteristic_t cha_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t cha_humidity  = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);
homekit_characteristic_t cha_co2  = HOMEKIT_CHARACTERISTIC_(CARBON_DIOXIDE_LEVEL, 0);
homekit_characteristic_t cha_co2_alert  = HOMEKIT_CHARACTERISTIC_(CARBON_DIOXIDE_DETECTED, false);
homekit_characteristic_t cha_air  = HOMEKIT_CHARACTERISTIC_(AIR_QUALITY, 0);

void i2c_init() {
    // Initialize I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    ESP_LOGI("I2C", "Initialized.");
}



void th_sensor_task(void *pvParameters) {
    uint8_t data[3];

    uint16_t raw_humidity = 0;
    uint16_t raw_temperature = 0;
    
    float temperature = 0 , humidity = 0 ;
    
    int elapsed_time = 0;

    while (1) {
            if (elapsed_time >= periodic_interval) {
               i2c_master_write_to_device(I2C_MASTER_NUM, SHT21_SENSOR_ADDR, &cmd_h, 1, pdMS_TO_TICKS(1000));
               vTaskDelay(pdMS_TO_TICKS(50));
               i2c_master_read_from_device(I2C_MASTER_NUM, SHT21_SENSOR_ADDR, data, 3, pdMS_TO_TICKS(1000));
               raw_humidity = (data[0] << 8) | (data[1] & 0xFC);
               humidity = -6.0 + 125.0 * (raw_humidity / 65536.0);

               i2c_master_write_to_device(I2C_MASTER_NUM, SHT21_SENSOR_ADDR, &cmd_t, 1, pdMS_TO_TICKS(1000));
               vTaskDelay(pdMS_TO_TICKS(50));
               i2c_master_read_from_device(I2C_MASTER_NUM, SHT21_SENSOR_ADDR, data, 3, pdMS_TO_TICKS(1000));

               raw_temperature = (data[0] << 8) | (data[1] & 0xFC);
               temperature = -2.2 + -46.85 + 175.72 * (raw_temperature / 65536.0);

               cha_temperature.value.float_value = temperature;
               cha_humidity.value.float_value = humidity;
                
               homekit_characteristic_notify(&cha_temperature, HOMEKIT_FLOAT(temperature));
               homekit_characteristic_notify(&cha_humidity, HOMEKIT_FLOAT(humidity));

               ESP_LOGI("INFORMATION", "Humidity: %.1f%%, Temp: %.1f°C", humidity, temperature);

               elapsed_time = 0;
            }

        vTaskDelay(pdMS_TO_TICKS(10000));
        elapsed_time += 10;
    }
}

void co_sensor_task(void *pvParameters) {
    int ppm = 111;
    int elapsed_time = 0;
    int airq = 0;
    bool alerted = false;

    while (1) {
            if (elapsed_time >= periodic_interval) {
    //    	portENTER_CRITICAL( &spinlock );
    //    	portDISABLE_INTERRUPTS();
                uart_write_bytes(UART_NUM, (const char *)co2_cmd, 9);
                vTaskDelay(pdMS_TO_TICKS(500));
   
                int len = uart_read_bytes(UART_NUM, response, 9, pdMS_TO_TICKS(1000));
                
    //          portENABLE_INTERRUPTS();
    //          portEXIT_CRITICAL( &spinlock );                
                if (len == 9 && response[0] == 0xFF && response[1] == 0x86) {
                    int ppm_raw = (response[2] << 8) | response[3];
                    ppm = (ppm_raw + ppm) / 2;
                } else {
            	    ESP_LOGE("CO2","UART Read error, resetting driver");
                    co2_init();
                }

		if (ppm > 1600) { airq = 5; } // Poor
		else 
		if (ppm > 1300) { airq = 4; } // Inferior
		else 
		if (ppm > 1000) { airq = 3; } // Fair
		else 
		if (ppm > 800) { airq = 2; } // Good
		else 
		if (ppm > 400) { airq = 1; } // Excellent
		else 
		{ airq = 0; } // Unknown
		
                cha_co2.value.float_value = ppm;
                homekit_characteristic_notify(&cha_co2, cha_co2.value);
                cha_air.value.int_value = airq;
                homekit_characteristic_notify(&cha_air, cha_air.value);
                alerted = ppm > ALERT;
                cha_co2_alert.value.bool_value = alerted;
                homekit_characteristic_notify(&cha_co2_alert, cha_co2_alert.value);


                ESP_LOGI("INFORMATION", "CO2 %dppm, Alert: %d, AirQ: %d ", ppm, alerted, airq);

                elapsed_time = 0;
            }

        vTaskDelay(pdMS_TO_TICKS(10000));
        elapsed_time += 10;
    }
}

void th_sensor_init_task() {
    xTaskCreate(th_sensor_task, "TH Sensor Task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
}
void co_sensor_init_task() {
    xTaskCreate(co_sensor_task, "CO2 Sensor Task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
}

homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, DEVICE_MANUFACTURER);
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, DEVICE_MODEL);
homekit_characteristic_t revision = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, FW_VERSION);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_sensors, .services = (homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
            &cha_name,
            &manufacturer,
            &cha_sn,
            &model,
            &revision,
            HOMEKIT_CHARACTERISTIC(IDENTIFY, accessory_identify),
            NULL
        }),
        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Temperature"),
            &cha_temperature,
            NULL
        }),
        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Humidity"),
            &cha_humidity,
            NULL
        }),
        HOMEKIT_SERVICE(AIR_QUALITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Air quality"),
            &cha_air,
            &cha_co2,
            &cha_co2_alert,
            NULL
        }),        
        NULL
    }),
    NULL
};
#pragma GCC diagnostic pop

homekit_server_config_t config = {
    .accessories = accessories,
    .password = CONFIG_ESP_SETUP_CODE,
    .setupId = CONFIG_ESP_SETUP_ID,
};

static void on_wifi_ready() {
    ESP_LOGI("INFORMATION", "Starting HomeKit server...");
    cha_name.value = HOMEKIT_STRING(ssid);
    cha_sn.value = HOMEKIT_STRING(ssn);
    homekit_server_init(&config);
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI("INFORMATION", "Initializing UART...");
    co2_init();
    vTaskDelay(pdMS_TO_TICKS(1000));


    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("WARNING", "NVS flash initialization failed, erasing...");
        CHECK_ERROR(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    CHECK_ERROR(ret);



    CHECK_ERROR(esp_efuse_mac_get_default(mac));
    sprintf(ssid,"MIOT32-THC-%02x%02x%02x", mac[3], mac[4], mac[5]);
    sprintf(ssn,"THC/%02x%02x%02x%02x%02x%02x",mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    vTaskDelay(pdMS_TO_TICKS(2000));
    wifi_init();
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_init();
    ESP_LOGI("INFORMATION", "Initializing sensors...");
    vTaskDelay(pdMS_TO_TICKS(500));
    i2c_init();
    vTaskDelay(pdMS_TO_TICKS(1000));
    // start tasks
    ESP_LOGI("INFORMATION", "Starting Tasks...");
    th_sensor_init_task();
    ESP_LOGI("INFORMATION", "TH Task created...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    co_sensor_init_task();
    ESP_LOGI("INFORMATION", "CO2 Task created...");
}
