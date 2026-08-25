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
#include <driver/gpio.h>
#include <onewire_bus.h>
#include <ds18b20.h>
#include <mqtt_client.h>

#define SENSOR_GPIO 4  // GPIO connected to DS18B20 data pin
#define MAX_SENSORS 1

// Define device characteristics
#define ACCESSORY_NAME "MH Temp Sensor"
#define ACCESSORY_SN "SN000001"
#define DEVICE_MANUFACTURER "Mazy's Wünderwafle"
#define DEVICE_SERIAL "MHTEMP000001"
#define DEVICE_MODEL "MIOT32/T/v1"
#define FW_VERSION "0.0.1"

char ssid[40] = "MIoT32/MH/T " ;
char ssn[20] = "000000000000" ;
uint8_t mac[6] = {0};

const int periodic_interval = 30;

int sensors_count = 0;
onewire_bus_handle_t bus = NULL;
onewire_bus_config_t bus_cfg = { .bus_gpio_num = SENSOR_GPIO };
onewire_bus_rmt_config_t rmt_cfg = { .max_rx_bytes = 10 };
ds18b20_device_handle_t sensors[MAX_SENSORS];
static esp_mqtt_client_handle_t mqtt_client = NULL;
esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = CONFIG_ESP_MQTT_URI,
};


#define CHECK_ERROR(x) do {                          \
                esp_err_t __err_rc = (x);            \
                if (__err_rc != ESP_OK) {            \
                        ESP_LOGE("ERROR", "Error: %s", esp_err_to_name(__err_rc)); \
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
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);


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

void mqtt_publish_temperature(float temperature) {
    if (mqtt_client) {
        char payload[32];
        snprintf(payload, sizeof(payload), "%.2f", temperature);
        esp_mqtt_client_publish(mqtt_client, "sensors/beczka_water/temperature", payload, 0, 1, 0);
    }
}

void th_sensor_task(void *pvParameters) {
    float temperature=-127 ;
    
    int elapsed_time = 0;

    while (1) {
            if (elapsed_time >= periodic_interval) {

        for (int i = 0; i < sensors_count; i++) {
            ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(sensors[i]));
            vTaskDelay(pdMS_TO_TICKS(750));  // max conversion time
            ESP_ERROR_CHECK(ds18b20_get_temperature(sensors[i], &temperature));
            ESP_LOGI("DS18", "Sensor %d: %.2f °C", i, temperature);

        }
        
        if ( temperature != -127 ) {
               mqtt_publish_temperature(temperature);
               homekit_characteristic_notify(&cha_temperature, HOMEKIT_FLOAT(temperature));
        }
               elapsed_time = 0;
            }

        vTaskDelay(pdMS_TO_TICKS(10000));
        elapsed_time += 10;
    }
}

void th_sensor_init_task() {
    xTaskCreate(th_sensor_task, "T Sensor Task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
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
            HOMEKIT_CHARACTERISTIC(NAME, "Water Temperature"),
            &cha_temperature,
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
    ESP_LOGI("WiFiReady", "Starting HomeKit server...");
    cha_name.value = HOMEKIT_STRING(ssid);
    cha_sn.value = HOMEKIT_STRING(ssn);
    homekit_server_init(&config);
    ESP_LOGI("WiFiReady", "Starting MQTT client %s", CONFIG_ESP_MQTT_URI);
    CHECK_ERROR(esp_mqtt_client_start(mqtt_client));

}

void init_onewire() {
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_cfg, &rmt_cfg, &bus));
    ESP_LOGI("ONEWIRE", "1-Wire bus ready");

    onewire_device_iter_handle_t iter = NULL;

    onewire_device_t dev;
    int cnt = 0;

    while (sensors_count == 0 || cnt > 60) {
        ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
        ESP_LOGI("ONEWIRE", "Searching devices...");
        while (onewire_device_iter_get_next(iter, &dev) == ESP_OK && cnt < MAX_SENSORS) {
        ds18b20_config_t cfg = {}; // default config
        if (ds18b20_new_device(&dev, &cfg, &sensors[sensors_count]) == ESP_OK) {
            ESP_LOGI("ONEWIRE", "DS18B20[%d] found (ROM: %016llX)", sensors_count, dev.address);
            cnt++;
            sensors_count++;
        }
        }
        onewire_del_device_iter(iter);
        vTaskDelay(pdMS_TO_TICKS(1000));

    }
    ESP_LOGI("ONEWIRE", "%d sensor(s) registered", cnt);
}

void app_main(void) {

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("WARNING", "NVS flash initialization failed, erasing...");
        CHECK_ERROR(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    CHECK_ERROR(ret);



    CHECK_ERROR(esp_efuse_mac_get_default(mac));
    sprintf(ssid,"MIOT32-MH-T-%02x%02x%02x", mac[3], mac[4], mac[5]);
    sprintf(ssn,"MHT/%02x%02x%02x%02x%02x%02x",mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    vTaskDelay(pdMS_TO_TICKS(2000));
    wifi_init();
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_init();
    ESP_LOGI("INFORMATION", "Initializing sensors...");
    vTaskDelay(pdMS_TO_TICKS(500));
    init_onewire();
    vTaskDelay(pdMS_TO_TICKS(1000));
    // start tasks
    ESP_LOGI("INFORMATION", "Starting Tasks...");
    th_sensor_init_task();
    ESP_LOGI("INFORMATION", "T Task created...");
    vTaskDelay(pdMS_TO_TICKS(5000));
}
