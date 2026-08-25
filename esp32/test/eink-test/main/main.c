/* E-paper room dashboard on a LilyGO T5 V2.3.
 *
 * Discovers the mazy-iot sensors over mDNS, pulls their readings plus Krakow
 * weather and air quality from Open-Meteo, and rotates through one screen per
 * source on the 2.13" panel.
 */
#include <stdio.h>
#include <string.h>

#include "dashdata.h"
#include "driver/gpio.h"
#include "epaper.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "screens.h"

static const char *TAG = "dashboard";

/* Status LED on GPIO19. It blinks while the dashboard is coming up and stays
 * dark once data is on screen - a display in a bedroom should not glow.
 *
 * Active high, despite LilyGO's boards.h declaring LED_ON as LOW for this
 * board: driving it low here left the LED permanently lit. */
#define LED_PIN  GPIO_NUM_19

static void led_set(bool on)
{
    gpio_set_level(LED_PIN, on ? 1 : 0);
}

static void led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << LED_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    led_set(false);
}

#define SCREEN_MS   (CONFIG_DASH_SCREEN_SECONDS * 1000)
#define REFRESH_US  (CONFIG_DASH_REFRESH_SECONDS * 1000000LL)

/* ------------------------------------------------------------------ */
/* Boot splash with a sliding progress bar                             */
/* ------------------------------------------------------------------ */

#define BAR_X       20
#define BAR_Y       86
#define BAR_H       10
#define BAR_BLOCK   56

static TaskHandle_t s_splash_task;
static volatile bool s_splash_run;
static char s_splash_text[48];

static void splash_frame(int offset)
{
    const int w = epaper_width();
    const int bar_w = w - 2 * BAR_X;

    epaper_clear(EPD_COLOR_WHITE);
    epaper_draw_text_center(0, w, 18, "Mazy's IoT Dashboard", &font_medium,
                            EPD_COLOR_BLACK);
    epaper_draw_text_center(0, w, 56, s_splash_text, &font_small,
                            EPD_COLOR_BLACK);

    epaper_draw_rect(BAR_X, BAR_Y, bar_w, BAR_H, EPD_COLOR_BLACK);

    /* The block slides right and wraps, drawn in two pieces when it straddles
     * the end so the motion stays continuous. */
    int span = bar_w - 4;
    int pos = offset % span;
    for (int i = 0; i < BAR_BLOCK; i++) {
        int x = BAR_X + 2 + (pos + i) % span;
        epaper_draw_vline(x, BAR_Y + 2, BAR_H - 4, EPD_COLOR_BLACK);
    }
}

static void splash_task(void *arg)
{
    int offset = 0;
    while (s_splash_run) {
        /* One blink per frame, so the LED tracks the bar's motion. */
        led_set((offset / 18) % 2 == 0);
        splash_frame(offset);
        epaper_refresh_partial();
        offset += 18;
    }
    led_set(false);
    s_splash_task = NULL;
    vTaskDelete(NULL);
}

static void splash_start(const char *text)
{
    strlcpy(s_splash_text, text, sizeof(s_splash_text));
    if (s_splash_task) {
        return;
    }
    s_splash_run = true;
    xTaskCreate(splash_task, "splash", 4096, NULL, 4, &s_splash_task);
}

static void splash_status(const char *text)
{
    strlcpy(s_splash_text, text, sizeof(s_splash_text));
}

/* Stops the animation and waits for the task to exit, so nothing else touches
 * the panel while a refresh is still in flight. */
static void splash_stop(void)
{
    s_splash_run = false;
    while (s_splash_task) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ------------------------------------------------------------------ */

void app_main(void)
{
    led_init();

    ESP_ERROR_CHECK(epaper_init());
    epaper_set_rotation(EPD_ROT_90);

    /* One full refresh first: the animation below is all partial updates and
     * needs a clean panel to start from. */
    epaper_clear(EPD_COLOR_WHITE);
    epaper_refresh_full();

    splash_start("connecting to wifi");
    esp_err_t err = dash_net_start();

    if (err != ESP_OK) {
        splash_stop();
        epaper_clear(EPD_COLOR_WHITE);
        epaper_draw_text_center(0, epaper_width(), 30, "WiFi failed",
                                &font_medium, EPD_COLOR_BLACK);
        epaper_draw_text_center(0, epaper_width(), 64, "check credentials",
                                &font_small, EPD_COLOR_BLACK);
        epaper_refresh_full();
        return;
    }

    splash_status("looking for sensors");
    dash_refresh();
    splash_stop();

    /* We booted, joined WiFi and fetched data, so this image is good. */
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "image on %s marked valid", running->label);
    }

    dash_snapshot_t snap;
    int64_t last_refresh = esp_timer_get_time();
    int index = 0;
    int since_full = 0;

    for (;;) {
        if (esp_timer_get_time() - last_refresh >= REFRESH_US) {
            dash_refresh();
            last_refresh = esp_timer_get_time();
        }

        dash_get(&snap);
        int count = screens_count(&snap);
        if (index >= count) {
            index = 0;
        }

        screens_draw(&snap, index);

        /* Partial refresh is ~0.7 s and silent; a periodic full refresh
         * clears the ghosting it leaves behind. */
        if (since_full >= CONFIG_DASH_FULL_REFRESH_EVERY) {
            epaper_refresh_full();
            since_full = 0;
        } else {
            epaper_refresh_partial();
            since_full++;
        }

        ESP_LOGI(TAG, "screen %d/%d", index + 1, count);
        index++;

        vTaskDelay(pdMS_TO_TICKS(SCREEN_MS));
    }
}
