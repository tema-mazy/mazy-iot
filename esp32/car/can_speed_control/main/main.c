#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "nvs_flash.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "CAN_SPEED";

// ── GPIO / CAN constants ─────────────────────────────────────────────────────
#define RELAY_GPIO   GPIO_NUM_1
#define LED_GPIO     GPIO_NUM_8
#define LINK_GPIO    GPIO_NUM_15
#define CAN_TXD_GPIO GPIO_NUM_20
#define CAN_RXD_GPIO GPIO_NUM_14
#define CAN_MODE     TWAI_MODE_LISTEN_ONLY
// Switch to TWAI_MODE_NORMAL for bench testing with a generator (needs ACK)

#define SPEED_OFF_KMH      15
#define SPEED_ON_KMH       10
#define CAN_ID_SWIFT_SPEED 0x1B8   // ABS wheel speeds: 4x uint16 BE, 0.01 m/s
#define WHEEL_SPEED_INVALID 0x3FFF // "ABS not ready" sentinel
#define SPEED_STALE_US     (5000LL * 1000LL)

// ── Log ring buffer ──────────────────────────────────────────────────────────
#define LOG_RING_SIZE 128
#define LOG_LINE_MAX  160

static char           s_log_lines[LOG_RING_SIZE][LOG_LINE_MAX];
static volatile int   s_log_write = 0; // next write slot (wraps mod LOG_RING_SIZE)
static volatile int   s_log_total = 0; // total lines ever written
static portMUX_TYPE   s_log_mux   = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t s_orig_vprintf;

static int log_intercept(const char *fmt, va_list args) {
  va_list copy;
  va_copy(copy, args);
  int ret = s_orig_vprintf(fmt, args); // keep UART output

  char buf[LOG_LINE_MAX];
  vsnprintf(buf, sizeof(buf), fmt, copy);
  va_end(copy);

  int len = strnlen(buf, LOG_LINE_MAX);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
    buf[--len] = '\0';
  if (len == 0)
    return ret;

  portENTER_CRITICAL_SAFE(&s_log_mux);
  memcpy(s_log_lines[s_log_write], buf, len + 1);
  s_log_write = (s_log_write + 1) % LOG_RING_SIZE;
  s_log_total++;
  portEXIT_CRITICAL_SAFE(&s_log_mux);

  return ret;
}

// ── WS2812 LED ───────────────────────────────────────────────────────────────
// R=CAN error (blinks 2 Hz), G=CAN OK, B=relay active; channels mix as RGB
static led_strip_handle_t s_led;

static void led_init(void) {
  led_strip_config_t strip_cfg = {
      .strip_gpio_num   = LED_GPIO,
      .max_leds         = 1,
      .led_pixel_format = LED_PIXEL_FORMAT_GRB,
      .led_model        = LED_MODEL_WS2812,
  };
  led_strip_rmt_config_t rmt_cfg = {.resolution_hz = 10 * 1000 * 1000};
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led));
  led_strip_clear(s_led);
}

static void led_set(uint8_t r, uint8_t g, uint8_t b) {
  led_strip_set_pixel(s_led, 0, r, g, b);
  led_strip_refresh(s_led);
}

static void led_update(bool can_error, bool can_ok, bool relay_on,
                       int64_t now_us) {
  uint8_t r = 0, g = 0, b = 0;
  if (can_error) {
    bool blink_on = (now_us / 250000LL) % 2 == 0;
    r = blink_on ? 200 : 0;
  }
  if (can_ok)   g = 100;
  if (relay_on) b = 100;
  led_set(r, g, b);
}

// ── Relay ────────────────────────────────────────────────────────────────────
static bool relay_active = true;

static void relay_set(bool on) {
  if (on == relay_active)
    return;
  relay_active = on;
  gpio_set_level(RELAY_GPIO, on ? 1 : 0);
  ESP_LOGI(TAG, "Relay %s — parking sensors %s", on ? "ON " : "OFF",
           on ? "ENABLED" : "DISABLED");
}

static void relay_boot_blink(void) {
  for (int i = 0; i < 2; i++) {
    gpio_set_level(RELAY_GPIO, 0);
    led_set(50, 50, 50);
    vTaskDelay(pdMS_TO_TICKS(300));
    gpio_set_level(RELAY_GPIO, 1);
    led_set(0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

static void gpio_init(void) {
  gpio_config_t cfg = {
      .pin_bit_mask  = (1ULL << RELAY_GPIO) | (1ULL << LINK_GPIO),
      .mode          = GPIO_MODE_OUTPUT,
      .pull_up_en    = GPIO_PULLUP_DISABLE,
      .pull_down_en  = GPIO_PULLDOWN_DISABLE,
      .intr_type     = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&cfg));
  gpio_set_level(RELAY_GPIO, 1); // SSR ON = sensors enabled (safe default)
}

// ── WiFi AP ──────────────────────────────────────────────────────────────────
#define WIFI_SSID "Swift"

static void wifi_init_ap(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifi_cfg = {
      .ap = {
          .ssid           = WIFI_SSID,
          .ssid_len       = strlen(WIFI_SSID),
          .password       = CONFIG_AP_PASSWORD,
          .max_connection = 4,
          .authmode       = strlen(CONFIG_AP_PASSWORD) >= 8
                                ? WIFI_AUTH_WPA2_PSK
                                : WIFI_AUTH_OPEN,
      },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "WiFi AP: SSID=%s  IP=192.168.4.1", WIFI_SSID);
}

// ── HTTP / SSE ───────────────────────────────────────────────────────────────
static const char *INDEX_HTML =
    "<!DOCTYPE html><html><head><title>CAN Speed</title>"
    "<style>body{font-family:monospace;background:#111;color:#0f0;margin:8px}"
    "#log{white-space:pre-wrap;font-size:13px}</style></head>"
    "<body><div id='log'></div><script>"
    "var es=new EventSource('/logs');"
    "es.onmessage=function(e){"
    "var d=document.getElementById('log');"
    "d.textContent+=e.data+'\\n';"
    "window.scrollTo(0,document.body.scrollHeight);};"
    "</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req) {
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t sse_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/event-stream");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");
  httpd_resp_send_chunk(req, ": connected\n\n", 13);

  int client_pos = 0;
  char line[LOG_LINE_MAX];
  char chunk[LOG_LINE_MAX + 8];

  while (1) {
    int total;
    portENTER_CRITICAL_SAFE(&s_log_mux);
    total = s_log_total;
    portEXIT_CRITICAL_SAFE(&s_log_mux);

    if (client_pos >= total) {
      // send a keepalive comment every 5 s to detect dead connections
      static int64_t last_ka = 0;
      int64_t now = esp_timer_get_time();
      if (now - last_ka > 5000000LL) {
        last_ka = now;
        if (httpd_resp_send_chunk(req, ": ka\n\n", 6) != ESP_OK) break;
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    if (total - client_pos > LOG_RING_SIZE)
      client_pos = total - LOG_RING_SIZE;

    int idx = client_pos % LOG_RING_SIZE;
    portENTER_CRITICAL_SAFE(&s_log_mux);
    memcpy(line, s_log_lines[idx], LOG_LINE_MAX);
    portEXIT_CRITICAL_SAFE(&s_log_mux);

    int n = snprintf(chunk, sizeof(chunk), "data: %s\n\n", line);
    if (httpd_resp_send_chunk(req, chunk, n) != ESP_OK) break;
    client_pos++;
  }

  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

static void httpd_init(void) {
  httpd_config_t config    = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable  = true;
  config.recv_wait_timeout = 10;
  config.send_wait_timeout = 10;
  httpd_handle_t server    = NULL;
  ESP_ERROR_CHECK(httpd_start(&server, &config));

  httpd_uri_t uri_index = {.uri = "/",     .method = HTTP_GET, .handler = index_handler};
  httpd_uri_t uri_logs  = {.uri = "/logs", .method = HTTP_GET, .handler = sse_handler};
  httpd_register_uri_handler(server, &uri_index);
  httpd_register_uri_handler(server, &uri_logs);
  ESP_LOGI(TAG, "HTTP server ready at http://192.168.4.1/");
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
  for (int i = 0; i < msg->data_length_code && i < 8; i++)
    len += snprintf(data_str + len, sizeof(data_str) - len, "%02X ", msg->data[i]);
  if (len > 0) data_str[len - 1] = '\0';
  ESP_LOGI(TAG, "CAN Rx: ID=0x%03X DLC=%d data=[%s]",
           (unsigned int)msg->identifier, msg->data_length_code, data_str);

  if (msg->identifier == CAN_ID_SWIFT_SPEED && msg->data_length_code >= 2) {
    uint16_t raw = ((uint16_t)msg->data[0] << 8) | msg->data[1];
    if (raw == WHEEL_SPEED_INVALID) return -1;
    return (uint32_t)raw * 36 / 1000; // 0.01 m/s → km/h
  }
  return -1;
}

static void update_relay(int speed_kmh) {
  if (speed_kmh > SPEED_OFF_KMH)
    relay_set(false);
  else if (speed_kmh < SPEED_ON_KMH)
    relay_set(true);
  // SPEED_ON_KMH..SPEED_OFF_KMH: hysteresis dead-band, hold state
}

// ── app_main ─────────────────────────────────────────────────────────────────
void app_main(void) {
  gpio_init();
  led_init();
  relay_boot_blink();
  relay_set(true); // sensors on until speed is known

  s_orig_vprintf = esp_log_set_vprintf(log_intercept);

  wifi_init_ap();
  httpd_init();
  can_init();

  int64_t last_speed_us  = 0;
  int64_t last_status_us = 0;
  int64_t link_on_us     = 0;
  bool    can_error      = false;

  while (1) {
    int64_t now_us = esp_timer_get_time();

    if (link_on_us > 0 && (now_us - link_on_us) > 50000LL) {
      gpio_set_level(LINK_GPIO, 0);
      link_on_us = 0;
    }

    if (last_speed_us > 0 && (now_us - last_speed_us) > SPEED_STALE_US) {
      ESP_LOGW(TAG, "Speed data stale — enabling sensors (safe default)");
      relay_set(true);
      last_speed_us = 0;
    }

    if (now_us - last_status_us > 2000000LL) {
      last_status_us = now_us;
      twai_status_info_t status;
      if (twai_get_status_info(&status) == ESP_OK) {
        can_error = (status.bus_error_count > 0 ||
                     status.state == TWAI_STATE_BUS_OFF ||
                     status.state == TWAI_STATE_RECOVERING);
        if (can_error)
          ESP_LOGW(TAG,
                   "CAN error: state=%d rx_err=%ld tx_err=%ld "
                   "rx_miss=%ld bus_err=%ld",
                   (int)status.state, (long)status.rx_error_counter,
                   (long)status.tx_error_counter, (long)status.rx_missed_count,
                   (long)status.bus_error_count);
      }
    }

    led_update(can_error, last_speed_us > 0, relay_active, now_us);

    twai_message_t rx;
    esp_err_t err = twai_receive(&rx, pdMS_TO_TICKS(15));
    if (err == ESP_OK) {
      gpio_set_level(LINK_GPIO, 1);
      link_on_us = now_us;
      int s = parse_broadcast(&rx);
      if (s >= 0) {
        last_speed_us = now_us;
        ESP_LOGI(TAG, "Speed: %d km/h", s);
        update_relay(s);
        taskYIELD();
      }
    } else if (err != ESP_ERR_TIMEOUT) {
      ESP_LOGE(TAG, "twai_receive failed: 0x%X (%s)", err, esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}
