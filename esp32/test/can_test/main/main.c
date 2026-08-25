#include "driver/twai.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "OBD2_CAN";

void app_main(void)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_1, GPIO_NUM_3, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    ESP_LOGI(TAG, "CAN driver started");

    // Prepare the OBD-II request to read speed (PID 0x0D)
    twai_message_t request_msg = {
        .identifier = 0x7DF,
        .data_length_code = 8,
        .data = {0x02, 0x01, 0x0D, 0, 0, 0, 0, 0},
        .flags = TWAI_MSG_FLAG_NONE
    };

    while (1) {
        // Send the OBD-II request frame
        esp_err_t ret = twai_transmit(&request_msg, pdMS_TO_TICKS(1000));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Transmit failed");
        }

        // Wait for the response (e.g., 0x7E8)
        twai_message_t rx_msg;
        ret = twai_receive(&rx_msg, pdMS_TO_TICKS(1000));
        if (ret == ESP_OK) {
            if ((rx_msg.identifier & 0x7F0) == 0x7E0 && // Response from ECU
                 rx_msg.data_length_code >= 4 &&
                 rx_msg.data[1] == 0x41 && rx_msg.data[2] == 0x0D) {
                uint8_t speed = rx_msg.data[3]; // Vehicle speed in km/h
                ESP_LOGI(TAG, "Vehicle Speed: %d km/h", speed);
            }
        } else {
            ESP_LOGI(TAG, "No CAN response");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Query every 1 second
    }
}
