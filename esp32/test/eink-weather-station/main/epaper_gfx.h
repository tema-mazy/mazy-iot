/* Pure framebuffer drawing for the 2.13" panel: geometry, primitives and
 * text. No hardware here, so this compiles on the host as well, which is what
 * tools/preview uses to render screens to PNG without flashing a board.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "fonts.h"

/* Native panel geometry. */
#define EPD_PANEL_WIDTH   122
#define EPD_PANEL_HEIGHT  250
#define EPD_ROW_BYTES     ((EPD_PANEL_WIDTH + 7) / 8)   /* 16 */
#define EPD_BUF_SIZE      (EPD_ROW_BYTES * EPD_PANEL_HEIGHT)

typedef enum {
    EPD_ROT_0   = 0,   /* portrait,  122 x 250, USB at the bottom */
    EPD_ROT_90  = 1,   /* landscape, 250 x 122 */
    EPD_ROT_180 = 2,
    EPD_ROT_270 = 3,
} epaper_rotation_t;

#define EPD_COLOR_WHITE 0
#define EPD_COLOR_BLACK 1

void epaper_set_rotation(epaper_rotation_t rot);

/* Logical framebuffer size for the current rotation. */
int epaper_width(void);
int epaper_height(void);

/* 1 = white, 0 = black, matching what the panel expects in RAM. */
const uint8_t *epaper_framebuffer(void);

void epaper_clear(uint8_t color);
void epaper_draw_pixel(int x, int y, uint8_t color);
void epaper_draw_hline(int x, int y, int w, uint8_t color);
void epaper_draw_vline(int x, int y, int h, uint8_t color);
void epaper_draw_line(int x0, int y0, int x1, int y1, uint8_t color);
void epaper_draw_rect(int x, int y, int w, int h, uint8_t color);
void epaper_fill_rect(int x, int y, int w, int h, uint8_t color);
void epaper_draw_circle(int cx, int cy, int r, uint8_t color);
void epaper_fill_circle(int cx, int cy, int r, uint8_t color);

/* Line with square-ish thickness, used for the weather glyphs. */
void epaper_draw_thick_line(int x0, int y0, int x1, int y1, int thickness,
                            uint8_t color);

/* Proportional text. `y` is the top of the line, not the baseline. Returns the
 * x coordinate just past the string. */
int epaper_draw_text(int x, int y, const char *s, const gfx_font_t *font,
                     uint8_t color);

/* Pixel width the string would occupy, for centring and right-alignment. */
int epaper_text_width(const char *s, const gfx_font_t *font);

/* Draw ending at `x_right` instead of starting at x. */
int epaper_draw_text_right(int x_right, int y, const char *s,
                           const gfx_font_t *font, uint8_t color);

/* Draw centred within [x, x + w). */
void epaper_draw_text_center(int x, int w, int y, const char *s,
                             const gfx_font_t *font, uint8_t color);
