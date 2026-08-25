/* LilyGO T5 V2.3 2.13" e-paper sample.
 *
 * Draws a static demo screen with a full refresh, then updates a counter and
 * a moving marker with fast partial refreshes. Every PARTIALS_PER_FULL
 * updates it does another full refresh to clear accumulated ghosting.
 */
#include <stdio.h>

#include "epaper.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "eink-test";

#define PARTIALS_PER_FULL   20
#define UPDATE_PERIOD_MS    2000

/* Region reserved for the partially refreshed counter text. */
#define COUNTER_X   8
#define COUNTER_Y   58
#define COUNTER_W   150
#define COUNTER_H   24

/* Track that sweeps left to right under the counter. */
#define TRACK_X     8
#define TRACK_Y     94
#define TRACK_W     234
#define TRACK_H     14

static void draw_static_screen(void)
{
    const int w = epaper_width();
    const int h = epaper_height();

    epaper_clear(EPD_COLOR_WHITE);

    /* Border and title bar. */
    epaper_draw_rect(0, 0, w, h, EPD_COLOR_BLACK);
    epaper_fill_rect(0, 0, w, 22, EPD_COLOR_BLACK);
    epaper_draw_string(8, 4, "LilyGO T5 V2.3  2.13in", 2, EPD_COLOR_WHITE);

    epaper_draw_string(8, 30, "ESP-IDF SSD1680 e-paper sample", 1, EPD_COLOR_BLACK);
    epaper_draw_string(8, 42, "SPI3  SCK 18  MOSI 23  CS 5", 1, EPD_COLOR_BLACK);

    epaper_draw_rect(TRACK_X - 2, TRACK_Y - 2, TRACK_W + 4, TRACK_H + 4, EPD_COLOR_BLACK);

    /* A couple of shapes to show the primitives work. */
    epaper_draw_circle(w - 34, 52, 16, EPD_COLOR_BLACK);
    epaper_draw_line(w - 50, 36, w - 18, 68, EPD_COLOR_BLACK);
    epaper_draw_line(w - 18, 36, w - 50, 68, EPD_COLOR_BLACK);
}

static void draw_counter(uint32_t n)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "count %lu", (unsigned long)n);

    epaper_fill_rect(COUNTER_X, COUNTER_Y, COUNTER_W, COUNTER_H, EPD_COLOR_WHITE);
    epaper_draw_string(COUNTER_X, COUNTER_Y, buf, 3, EPD_COLOR_BLACK);
}

static void draw_track(uint32_t n)
{
    const int marker_w = 16;
    const int steps = PARTIALS_PER_FULL;
    int pos = (int)((n % steps) * (TRACK_W - marker_w) / (steps - 1));

    epaper_fill_rect(TRACK_X, TRACK_Y, TRACK_W, TRACK_H, EPD_COLOR_WHITE);
    epaper_fill_rect(TRACK_X + pos, TRACK_Y, marker_w, TRACK_H, EPD_COLOR_BLACK);
}

void app_main(void)
{
    ESP_ERROR_CHECK(epaper_init());
    epaper_set_rotation(EPD_ROT_90);
    ESP_LOGI(TAG, "logical size %dx%d", epaper_width(), epaper_height());

    uint32_t n = 0;
    for (;;) {
        bool full = (n % PARTIALS_PER_FULL) == 0;

        if (full) {
            draw_static_screen();
        }
        draw_counter(n);
        draw_track(n);

        if (full) {
            ESP_LOGI(TAG, "full refresh, n=%lu", (unsigned long)n);
            epaper_refresh_full();
        } else {
            ESP_LOGI(TAG, "partial refresh, n=%lu", (unsigned long)n);
            epaper_refresh_partial();
        }

        n++;
        vTaskDelay(pdMS_TO_TICKS(UPDATE_PERIOD_MS));
    }
}
