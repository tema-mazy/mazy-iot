#include "Counter.h"

volatile uint16_t CPS = 0; // current speed km/h (displayed on LCD)
volatile bool can_connected = true;

static const char *TAG = "CAN";

#define CAN_TXD_GPIO GPIO_NUM_20
#define CAN_RXD_GPIO GPIO_NUM_19
#define CAN_ID_SWIFT_SPEED 0x1B8   // ABS wheel speeds: 4x uint16 BE, 0.01 m/s per count
#define WHEEL_SPEED_INVALID 0x3FFF // value broadcast before ABS is ready

static void receiver_task(void *arg) {
  twai_message_t rx;
  int64_t last_rx_time = esp_timer_get_time();

  while (1) {
    esp_err_t err = twai_receive(&rx, pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
      if (rx.identifier == CAN_ID_SWIFT_SPEED && rx.data_length_code >= 2) {
        uint16_t raw = ((uint16_t)rx.data[0] << 8) | rx.data[1];
        if (raw == WHEEL_SPEED_INVALID)
          continue;
        CPS = (uint32_t)raw * 36 / 1000; // 0.01 m/s -> km/h
        can_connected = true;
        last_rx_time = esp_timer_get_time();
        Set_RGB(0, 32, 0); // Green indicating active speed reception
        ESP_LOGI(TAG, "RX 0x%03X raw=%d speed=%d km/h", (unsigned int)rx.identifier, raw, CPS);
      }
    } else if (err == ESP_ERR_TIMEOUT) {
      // Check if we haven't received anything for 3 seconds
      if (esp_timer_get_time() - last_rx_time > 3000000LL) {
        can_connected = false;
        Set_RGB(32, 0, 0); // Red indicating connection lost
      }
    } else {
      // Prevent tight loop on other driver errors
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

void Counter_Init(void) {
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(CAN_TXD_GPIO, CAN_RXD_GPIO, TWAI_MODE_LISTEN_ONLY);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
  ESP_ERROR_CHECK(twai_start());
  ESP_LOGI(TAG, "CAN initialized (500 kbps)");

  xTaskCreatePinnedToCore(receiver_task, "CAN rx", 4096, NULL, 5, NULL, 0);
}
