#include <stdio.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "onewire_bus.h"
#include "ds18b20.h"

#define SENSOR_GPIO GPIO_NUM_4
#define MAX_SENSORS 1

static const char *TAG = "ds18b20";

void app_main(void)
{
    ESP_LOGI(TAG, "BOOTING");

    // 1. Initialize 1-Wire bus
    onewire_bus_handle_t bus = NULL;
    onewire_bus_config_t bus_cfg = { .bus_gpio_num = SENSOR_GPIO };
    onewire_bus_rmt_config_t rmt_cfg = { .max_rx_bytes = 10 };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_cfg, &rmt_cfg, &bus));
    ESP_LOGI(TAG, "1-Wire bus ready");

    // 2. Iterate and register DS18B20 devices
    onewire_device_iter_handle_t iter = NULL;

    onewire_device_t dev;
    ds18b20_device_handle_t sensors[MAX_SENSORS];
    int cnt = 0;


    while (cnt == 0) {
        ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
	ESP_LOGI(TAG, "Searching devices...");
	while (onewire_device_iter_get_next(iter, &dev) == ESP_OK && cnt < MAX_SENSORS) {
        ds18b20_config_t cfg = {}; // default config
        if (ds18b20_new_device(&dev, &cfg, &sensors[cnt]) == ESP_OK) {
            ESP_LOGI(TAG, "DS18B20[%d] found (ROM: %016llX)", cnt, dev.address);
            cnt++;
        }
	}
	onewire_del_device_iter(iter);

        vTaskDelay(pdMS_TO_TICKS(5000));

    }
    ESP_LOGI(TAG, "%d sensor(s) registered", cnt);

    // 3. Read loop
    while (true) {
        for (int i = 0; i < cnt; i++) {
            float t = 0;
            ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(sensors[i]));
            vTaskDelay(pdMS_TO_TICKS(750));  // max conversion time
            ESP_ERROR_CHECK(ds18b20_get_temperature(sensors[i], &t));
            ESP_LOGI(TAG, "Sensor %d: %.2f °C", i, t);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
