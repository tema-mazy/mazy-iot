#include "Counter.h"

uint16_t CPS          = 0;  // packet counter (display)
bool     can_connected = false;

static const char *TAG = "CAN";

#define CAN_TXD_GPIO   GPIO_NUM_20
#define CAN_RXD_GPIO   GPIO_NUM_19
#define STALE_US       (5000LL * 1000LL)

static int64_t s_last_rx_us      = 0;
static int64_t s_connect_deadline = 0;

static void sniffer_task(void *arg)
{
    while (1) {
        int64_t now_us = esp_timer_get_time();

        if (!can_connected && s_last_rx_us == 0 &&
            s_connect_deadline > 0 && now_us > s_connect_deadline) {
            ESP_LOGW(TAG, "CAN not connected");
            s_connect_deadline = 0;
            Set_RGB(0, 0, 0);
        }

        if (s_last_rx_us > 0 && (now_us - s_last_rx_us) > STALE_US) {
            ESP_LOGW(TAG, "CAN signal lost");
            can_connected = false;
            s_last_rx_us  = 0;
            Set_RGB(0, 0, 0);
        }

        twai_message_t rx;
        if (twai_receive(&rx, pdMS_TO_TICKS(15)) != ESP_OK) continue;

        int32_t interval_ms = s_last_rx_us > 0
            ? (int32_t)((now_us - s_last_rx_us) / 1000) : 0;
        s_last_rx_us  = now_us;
        can_connected = true;
        CPS++;

        uint8_t n = rx.data_length_code;
        ESP_LOGI(TAG, "ID:%03X DLC:%d  %02X %02X %02X %02X %02X %02X %02X %02X  [%d ms]",
                 rx.identifier, n,
                 n > 0 ? rx.data[0] : 0, n > 1 ? rx.data[1] : 0,
                 n > 2 ? rx.data[2] : 0, n > 3 ? rx.data[3] : 0,
                 n > 4 ? rx.data[4] : 0, n > 5 ? rx.data[5] : 0,
                 n > 6 ? rx.data[6] : 0, n > 7 ? rx.data[7] : 0,
                 interval_ms);

        Set_RGB(0, 32, 0);  // brief green pulse on any frame
    }
}

void Counter_Init(void)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TXD_GPIO, CAN_RXD_GPIO, TWAI_MODE_LISTEN_ONLY);
    twai_timing_config_t  t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());
    s_connect_deadline = esp_timer_get_time() + STALE_US;
    ESP_LOGI(TAG, "CAN sniffer started (listen-only, 500 kbps)");

    xTaskCreatePinnedToCore(sniffer_task, "CAN sniffer", 2048, NULL, 1, NULL, 0);
}
