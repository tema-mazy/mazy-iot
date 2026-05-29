#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "CAN_SPEED";

#define RELAY_GPIO      GPIO_NUM_0
#define LED_GPIO        GPIO_NUM_8
#define CAN_TXD_GPIO    GPIO_NUM_1
#define CAN_RXD_GPIO    GPIO_NUM_3

// Hysteresis band: relay turns OFF above SPEED_OFF, turns ON below SPEED_ON
#define SPEED_OFF_KMH   10
#define SPEED_ON_KMH    8

// Suzuki Swift AZ broadcast: CAN ID 0x0AA, bytes [1..2] = speed * 100 (km/h)
// Verify on your specific car: (data[1]<<8 | data[2]) / 100 must match speedometer
#define CAN_ID_SWIFT_SPEED  0x0AA

// If no broadcast seen for this long, assume stopped → enable sensors (safe default)
#define SPEED_STALE_US      (5000LL * 1000LL)   // 5 s in µs

static bool relay_active = false;

// relay ON  → SSR closed → brake signal forwarded → parking sensors ENABLED
// relay OFF → SSR open   → brake signal blocked   → parking sensors DISABLED
static void relay_set(bool on)
{
    if (on == relay_active) return;
    relay_active = on;
    gpio_set_level(RELAY_GPIO, on ? 0 : 1); // active-low SSR input
    gpio_set_level(LED_GPIO,   on ? 1 : 0);
    ESP_LOGI(TAG, "Relay %s — parking sensors %s",
             on ? "ON " : "OFF",
             on ? "ENABLED" : "DISABLED");
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
    gpio_set_level(RELAY_GPIO, 0); // SSR ON = sensors enabled (safe default)
    gpio_set_level(LED_GPIO,   1);
}

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

// Returns speed in km/h if the frame is a Swift AZ broadcast, else -1
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
    if (speed_kmh > SPEED_OFF_KMH) {
        relay_set(false);
    } else if (speed_kmh < SPEED_ON_KMH) {
        relay_set(true);
    }
    // SPEED_ON_KMH..SPEED_OFF_KMH: hold current state (hysteresis dead-band)
}

void app_main(void)
{
    gpio_init();
    relay_set(true); // sensors on until speed is known

    can_init();

    int64_t last_speed_us = 0;

    while (1) {
        int64_t now_us = esp_timer_get_time();

        // Safety fallback: if broadcast goes silent, assume stopped
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
