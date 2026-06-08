#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "esp_wifi.h"
// #include "esp_event.h"
// #include "esp_netif.h"
// #include "nvs_flash.h"
// #include "esp_http_server.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "CAN_SPEED";

// ── GPIO / CAN constants ─────────────────────────────────────────────────────
#define RELAY_GPIO GPIO_NUM_0
#define LED_GPIO GPIO_NUM_8
#define CAN_TXD_GPIO GPIO_NUM_20
#define CAN_RXD_GPIO GPIO_NUM_21
#define CAN_MODE TWAI_MODE_LISTEN_ONLY // Change to
// TWAI_MODE_NORMAL for bench testing with generator
// #define CAN_MODE TWAI_MODE_NORMAL // Change to TWAI_MODE_NORMAL for bench
// testing with
// generator

#define SPEED_OFF_KMH 10
#define SPEED_ON_KMH 8
#define CAN_ID_SWIFT_SPEED 0x180
#define SPEED_STALE_US (5000LL * 1000LL)

// ── Log ring buffer (disabled) ───────────────────────────────────────────────
// #define LOG_RING_SIZE   64
// #define LOG_LINE_MAX    160
//
// static char          s_log_lines[LOG_RING_SIZE][LOG_LINE_MAX];
// static int           s_log_head  = 0;
// static int           s_log_count = 0;
// static portMUX_TYPE  s_log_mux   = portMUX_INITIALIZER_UNLOCKED;
// static vprintf_like_t s_orig_vprintf;
//
// static int log_intercept(const char *fmt, va_list args) { ... }

// ── Relay ────────────────────────────────────────────────────────────────────
static bool relay_active = true;

static void relay_set(bool on) {
  if (on == relay_active)
    return;
  relay_active = on;
  gpio_set_level(RELAY_GPIO, on ? 1 : 0);
  gpio_set_level(LED_GPIO, on ? 0 : 1); // active-low: 0 = ON
  ESP_LOGI(TAG, "Relay %s — parking sensors %s", on ? "ON " : "OFF",
           on ? "ENABLED" : "DISABLED");
}

static void relay_boot_blink(void) {
  for (int i = 0; i < 2; i++) {
    gpio_set_level(RELAY_GPIO, 0);
    gpio_set_level(LED_GPIO, 1); // relay OFF, LED OFF
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(RELAY_GPIO, 1);
    gpio_set_level(LED_GPIO, 0); // relay ON, LED ON
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

static void gpio_init(void) {
  gpio_config_t cfg = {
      .pin_bit_mask = (1ULL << RELAY_GPIO) | (1ULL << LED_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&cfg));
  gpio_set_level(RELAY_GPIO, 1); // SSR ON = sensors enabled (safe default)
  gpio_set_level(LED_GPIO, 0);   // active-low: 0 = LED ON
}

// ── CAN ──────────────────────────────────────────────────────────────────────
static void can_init(void) {
  twai_general_config_t g_cfg =
      TWAI_GENERAL_CONFIG_DEFAULT(CAN_TXD_GPIO, CAN_RXD_GPIO, CAN_MODE);
  twai_timing_config_t t_cfg = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  ESP_ERROR_CHECK(twai_driver_install(&g_cfg, &t_cfg, &f_cfg));
  ESP_ERROR_CHECK(twai_start());
  ESP_LOGI(TAG, "TWAI/CAN started at 500 kbps (mode: %s)",
           CAN_MODE == TWAI_MODE_LISTEN_ONLY ? "listen-only" : "normal");
}

static int parse_broadcast(const twai_message_t *msg) {
  char data_str[32] = {0};
  int len = 0;
  for (int i = 0; i < msg->data_length_code && i < 8; i++) {
    len +=
        snprintf(data_str + len, sizeof(data_str) - len, "%02X ", msg->data[i]);
  }
  if (len > 0) {
    data_str[len - 1] = '\0'; // Remove trailing space
  }
  ESP_LOGI(TAG, "CAN Rx: ID=0x%03X DLC=%d data=[%s]",
           (unsigned int)msg->identifier, msg->data_length_code, data_str);

  if (msg->identifier == CAN_ID_SWIFT_SPEED && msg->data_length_code >= 2) {
    uint16_t raw = ((uint16_t)msg->data[0] << 8) | msg->data[1];
    return raw / 20;
  }

  return -1;
}

static void update_relay(int speed_kmh) {
  if (speed_kmh > SPEED_OFF_KMH)
    relay_set(false);
  else if (speed_kmh < SPEED_ON_KMH)
    relay_set(true);
  // SPEED_ON_KMH..SPEED_OFF_KMH: hold current state (hysteresis dead-band)
}

// ── WiFi AP (disabled) ───────────────────────────────────────────────────────
// static void wifi_init_ap(void) { ... }

// ── HTTP / SSE (disabled) ────────────────────────────────────────────────────
// static void httpd_init(void) { ... }

// ── app_main ─────────────────────────────────────────────────────────────────
void app_main(void) {
  // s_orig_vprintf = esp_log_set_vprintf(log_intercept);

  gpio_init();
  relay_boot_blink();
  relay_set(true); // sensors on until speed is known
  can_init();
  // wifi_init_ap();
  // httpd_init();

  int64_t last_speed_us = 0;
  int64_t last_status_us = 0;

  while (1) {
    int64_t now_us = esp_timer_get_time();

    if (last_speed_us > 0 && (now_us - last_speed_us) > SPEED_STALE_US) {
      ESP_LOGW(TAG, "Speed data stale — enabling sensors (safe default)");
      relay_set(true);
      last_speed_us = 0;
    }

    if (now_us - last_status_us > 2000000LL) { // Every 2 seconds
      last_status_us = now_us;
      twai_status_info_t status;
      if (twai_get_status_info(&status) == ESP_OK) {
        if (status.rx_error_counter > 0 || status.tx_error_counter > 0 ||
            status.bus_error_count > 0 || status.state == TWAI_STATE_BUS_OFF) {
          ESP_LOGW(TAG,
                   "CAN Status Error: state=%d, rx_err=%ld, tx_err=%ld, "
                   "rx_miss=%ld, "
                   "bus_err=%ld, rx_queued=%ld",
                   (int)status.state, (long)status.rx_error_counter,
                   (long)status.tx_error_counter, (long)status.rx_missed_count,
                   (long)status.bus_error_count, (long)status.msgs_to_rx);
        }
      }
    }

    twai_message_t rx;
    esp_err_t err = twai_receive(&rx, pdMS_TO_TICKS(15));
    if (err == ESP_OK) {
      int s = parse_broadcast(&rx);
      if (s >= 0) {
        last_speed_us = now_us;
        ESP_LOGI(TAG, "Speed: %d km/h", s);
        update_relay(s);
        taskYIELD();
      }
    } else if (err != ESP_ERR_TIMEOUT) {
      ESP_LOGE(TAG, "twai_receive failed: 0x%X (%s)", err,
               esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(
          100)); // prevent tight-loop spam if driver is in bad state
    }
  }
}
