#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include <string.h>
#include <stdarg.h>

static const char *TAG = "CAN_SPEED";

// ── GPIO / CAN constants ─────────────────────────────────────────────────────
#define RELAY_GPIO          GPIO_NUM_0
#define LED_GPIO            GPIO_NUM_8
#define CAN_TXD_GPIO        GPIO_NUM_1
#define CAN_RXD_GPIO        GPIO_NUM_3

#define SPEED_OFF_KMH       10
#define SPEED_ON_KMH        8
#define CAN_ID_SWIFT_SPEED  0x0AA
#define SPEED_STALE_US      (5000LL * 1000LL)

// ── Log ring buffer ──────────────────────────────────────────────────────────
#define LOG_RING_SIZE   64
#define LOG_LINE_MAX    160

static char          s_log_lines[LOG_RING_SIZE][LOG_LINE_MAX];
static int           s_log_head  = 0;
static int           s_log_count = 0;  // monotonically increasing
static portMUX_TYPE  s_log_mux   = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t s_orig_vprintf;

static int log_intercept(const char *fmt, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    int ret = s_orig_vprintf(fmt, args);  // UART output unchanged

    char line[LOG_LINE_MAX];
    vsnprintf(line, sizeof(line), fmt, copy);
    va_end(copy);

    int len = (int)strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

    taskENTER_CRITICAL(&s_log_mux);
    memcpy(s_log_lines[s_log_head], line, (size_t)len + 1);
    s_log_head  = (s_log_head + 1) % LOG_RING_SIZE;
    s_log_count++;
    taskEXIT_CRITICAL(&s_log_mux);

    return ret;
}

// ── Relay ────────────────────────────────────────────────────────────────────
static bool relay_active = false;

static void relay_set(bool on)
{
    if (on == relay_active) return;
    relay_active = on;
    gpio_set_level(RELAY_GPIO, on ? 0 : 1);
    gpio_set_level(LED_GPIO,   on ? 1 : 0);
    ESP_LOGI(TAG, "Relay %s — parking sensors %s",
             on ? "ON " : "OFF", on ? "ENABLED" : "DISABLED");
}

static void gpio_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask  = (1ULL << RELAY_GPIO) | (1ULL << LED_GPIO),
        .mode          = GPIO_MODE_OUTPUT,
        .pull_up_en    = GPIO_PULLUP_DISABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(RELAY_GPIO, 0);  // SSR ON = sensors enabled (safe default)
    gpio_set_level(LED_GPIO,   1);
}

// ── CAN ──────────────────────────────────────────────────────────────────────
static void can_init(void)
{
    // LISTEN_ONLY: no ACK bits, no transmissions — invisible to the ECU
    twai_general_config_t g_cfg = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TXD_GPIO, CAN_RXD_GPIO, TWAI_MODE_LISTEN_ONLY);
    twai_timing_config_t  t_cfg = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t  f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_ERROR_CHECK(twai_driver_install(&g_cfg, &t_cfg, &f_cfg));
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "TWAI/CAN started at 500 kbps (listen-only)");
}

static int parse_broadcast(const twai_message_t *msg)
{
    if (msg->identifier == CAN_ID_SWIFT_SPEED && msg->data_length_code >= 3) {
        uint16_t raw = ((uint16_t)msg->data[1] << 8) | msg->data[2];
        return (int)(raw / 100);
    }
    return -1;
}

static void update_relay(int speed_kmh)
{
    if (speed_kmh > SPEED_OFF_KMH)     relay_set(false);
    else if (speed_kmh < SPEED_ON_KMH) relay_set(true);
    // SPEED_ON_KMH..SPEED_OFF_KMH: hold current state (hysteresis dead-band)
}

// ── WiFi AP ──────────────────────────────────────────────────────────────────
static void wifi_init_ap(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t cfg = { 0 };
    strlcpy((char *)cfg.ap.ssid,     "Swift",            sizeof(cfg.ap.ssid));
    strlcpy((char *)cfg.ap.password, CONFIG_AP_PASSWORD, sizeof(cfg.ap.password));
    cfg.ap.ssid_len         = 5;
    cfg.ap.channel          = 6;
    cfg.ap.max_connection   = 4;
    cfg.ap.authmode         = strlen(CONFIG_AP_PASSWORD) >= 8
                              ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    cfg.ap.pmf_cfg.capable  = true;   // iOS/macOS require PMF-capable AP for WPA2
    cfg.ap.pmf_cfg.required = false;  // don't mandate PMF so non-Apple clients still work

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Force 11b/g — prevents AMPDU/BA negotiation that causes ADDBA/DELBA storms */
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G));
    /* Suppress noisy driver logs — keep only errors */
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("esp_netif_lwip", ESP_LOG_WARN);
    ESP_LOGI(TAG, "AP started — SSID: Swift  IP: 192.168.4.1");
}

// ── HTTP / SSE ───────────────────────────────────────────────────────────────
static const char HTML[] =
    "<!DOCTYPE html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Swift CAN</title><style>"
    "body{background:#111;color:#ccc;font:13px/1.5 monospace;margin:0;padding:8px}"
    "h2{color:#4f4;margin:0 0 6px}#log{height:calc(100vh - 44px);overflow-y:auto}"
    ".W{color:#fa0}.E{color:#f55}.I{color:#8f8}.D{color:#88f}"
    "</style></head><body><h2>Swift CAN Speed Monitor</h2>"
    "<div id=log></div><script>"
    "const d=document.getElementById('log'),"
    "es=new EventSource('/events');"
    "es.onmessage=e=>{"
    "const r=document.createElement('div'),t=e.data;"
    "r.className=t.includes('(W)')?'W':t.includes('(E)')?'E':t.includes('(I)')?'I':'D';"
    "r.textContent=t;d.appendChild(r);d.scrollTop=d.scrollHeight;"
    "while(d.children.length>500)d.removeChild(d.firstChild)"
    "};"
    "es.onerror=()=>setTimeout(()=>location.reload(),3000);"
    "</script></body></html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    return httpd_resp_send(req, HTML, HTTPD_RESP_USE_STRLEN);
}

// Runs in its own FreeRTOS task so the httpd worker stays free for page loads.
static void sse_task(void *arg)
{
    httpd_req_t *req = arg;

    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // Start from the oldest entry still in the ring
    int last_sent;
    taskENTER_CRITICAL(&s_log_mux);
    last_sent = (s_log_count > LOG_RING_SIZE) ? s_log_count - LOG_RING_SIZE : 0;
    taskEXIT_CRITICAL(&s_log_mux);

    char ev[LOG_LINE_MAX + 16];
    while (1) {
        int count;
        taskENTER_CRITICAL(&s_log_mux);
        count = s_log_count;
        taskEXIT_CRITICAL(&s_log_mux);

        while (last_sent < count) {
            char line[LOG_LINE_MAX];
            taskENTER_CRITICAL(&s_log_mux);
            int oldest = (s_log_count > LOG_RING_SIZE) ? s_log_count - LOG_RING_SIZE : 0;
            if (last_sent < oldest) last_sent = oldest;  // skip overwritten entries
            strlcpy(line, s_log_lines[last_sent % LOG_RING_SIZE], LOG_LINE_MAX);
            taskEXIT_CRITICAL(&s_log_mux);

            int n = snprintf(ev, sizeof(ev), "data: %s\n\n", line);
            if (httpd_resp_send_chunk(req, ev, n) != ESP_OK) goto done;
            last_sent++;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
        if (httpd_resp_send_chunk(req, ": ping\n\n", 8) != ESP_OK) goto done;
    }
done:
    httpd_req_async_handler_complete(req);
    vTaskDelete(NULL);
}

static esp_err_t events_handler(httpd_req_t *req)
{
    httpd_req_t *req_copy;
    ESP_ERROR_CHECK(httpd_req_async_handler_begin(req, &req_copy));
    xTaskCreate(sse_task, "sse", 4096, req_copy, 5, NULL);
    return ESP_OK;
}

static void httpd_init(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_open_sockets = 5;

    httpd_handle_t server;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));

    static const httpd_uri_t root   = { .uri="/",       .method=HTTP_GET, .handler=root_handler   };
    static const httpd_uri_t events = { .uri="/events", .method=HTTP_GET, .handler=events_handler };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &events);
    ESP_LOGI(TAG, "HTTP server ready at http://192.168.4.1");
}

// ── app_main ─────────────────────────────────────────────────────────────────
void app_main(void)
{
    s_orig_vprintf = esp_log_set_vprintf(log_intercept);

    gpio_init();
    relay_set(true);  // sensors on until speed is known
    can_init();
    wifi_init_ap();
    httpd_init();

    int64_t last_speed_us = 0;

    while (1) {
        int64_t now_us = esp_timer_get_time();

        if (last_speed_us > 0 && (now_us - last_speed_us) > SPEED_STALE_US) {
            ESP_LOGW(TAG, "Speed data stale — enabling sensors (safe default)");
            relay_set(true);
            last_speed_us = 0;
        }

        twai_message_t rx;
        if (twai_receive(&rx, pdMS_TO_TICKS(15)) == ESP_OK) {
            int s = parse_broadcast(&rx);
            if (s >= 0) {
                last_speed_us = now_us;
                ESP_LOGI(TAG, "Speed: %d km/h", s);
                update_relay(s);
                taskYIELD();
            }
        }
    }
}
