/* Minimal SSD1680 / SSD1675B e-paper driver for the LilyGO T5 V2.3 2.13" board.
 *
 * The panel is 122 x 250 in its native (portrait) orientation. All drawing
 * calls below use logical coordinates that depend on the rotation set with
 * epaper_set_rotation(); the default is landscape, 250 x 122.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* Native panel geometry. */
#define EPD_PANEL_WIDTH   122
#define EPD_PANEL_HEIGHT  250
#define EPD_ROW_BYTES     ((EPD_PANEL_WIDTH + 7) / 8)   /* 16 */
#define EPD_BUF_SIZE      (EPD_ROW_BYTES * EPD_PANEL_HEIGHT)

/* Board pinout, taken from LilyGO boards.h, LILYGO_T5_V213 block. */
#define EPD_PIN_MOSI  23
#define EPD_PIN_SCLK  18
#define EPD_PIN_CS     5
#define EPD_PIN_DC    17
#define EPD_PIN_RST   16
#define EPD_PIN_BUSY   4

typedef enum {
    EPD_ROT_0   = 0,   /* portrait,  122 x 250, USB at the bottom */
    EPD_ROT_90  = 1,   /* landscape, 250 x 122 */
    EPD_ROT_180 = 2,
    EPD_ROT_270 = 3,
} epaper_rotation_t;

#define EPD_COLOR_WHITE 0
#define EPD_COLOR_BLACK 1

/* Bring up the SPI bus and reset/initialise the panel. Call once. */
esp_err_t epaper_init(void);

/* Release the panel and the SPI bus. */
void epaper_deinit(void);

void epaper_set_rotation(epaper_rotation_t rot);

/* Logical framebuffer size for the current rotation. */
int epaper_width(void);
int epaper_height(void);

/* Framebuffer drawing. Nothing reaches the panel until a refresh call. */
void epaper_clear(uint8_t color);
void epaper_draw_pixel(int x, int y, uint8_t color);
void epaper_draw_hline(int x, int y, int w, uint8_t color);
void epaper_draw_vline(int x, int y, int h, uint8_t color);
void epaper_draw_line(int x0, int y0, int x1, int y1, uint8_t color);
void epaper_draw_rect(int x, int y, int w, int h, uint8_t color);
void epaper_fill_rect(int x, int y, int w, int h, uint8_t color);
void epaper_draw_circle(int cx, int cy, int r, uint8_t color);

/* Text using the built-in 5x7 font. `scale` multiplies both axes; the glyph
 * advance is 6 * scale pixels. Returns the x coordinate just past the string. */
int epaper_draw_char(int x, int y, char c, int scale, uint8_t color);
int epaper_draw_string(int x, int y, const char *s, int scale, uint8_t color);

/* Push the framebuffer and run a full refresh (flashing, ~2 s, best quality).
 * Use this at least every few dozen partial updates to clear ghosting. */
void epaper_refresh_full(void);

/* Push the framebuffer and run a fast non-flashing update. Leaves some
 * ghosting behind; only supported by SSD1680-class panels (see README). */
void epaper_refresh_partial(void);

/* Put the panel into deep sleep. epaper_init() must be called again after. */
void epaper_sleep(void);
