#include "epaper_gfx.h"

#include <stdlib.h>
#include <string.h>

/* 1 = white, 0 = black, matching what the panel expects in RAM. */
static uint8_t s_fb[EPD_BUF_SIZE];
static epaper_rotation_t s_rot = EPD_ROT_90;

const uint8_t *epaper_framebuffer(void)
{
    return s_fb;
}

void epaper_set_rotation(epaper_rotation_t rot)
{
    s_rot = rot;
}

int epaper_width(void)
{
    return (s_rot == EPD_ROT_90 || s_rot == EPD_ROT_270) ? EPD_PANEL_HEIGHT : EPD_PANEL_WIDTH;
}

int epaper_height(void)
{
    return (s_rot == EPD_ROT_90 || s_rot == EPD_ROT_270) ? EPD_PANEL_WIDTH : EPD_PANEL_HEIGHT;
}

void epaper_clear(uint8_t color)
{
    memset(s_fb, color == EPD_COLOR_BLACK ? 0x00 : 0xFF, sizeof(s_fb));
}

void epaper_draw_pixel(int x, int y, uint8_t color)
{
    if (x < 0 || y < 0 || x >= epaper_width() || y >= epaper_height()) {
        return;
    }

    int nx, ny;
    switch (s_rot) {
    case EPD_ROT_90:
        nx = y;
        ny = EPD_PANEL_HEIGHT - 1 - x;
        break;
    case EPD_ROT_180:
        nx = EPD_PANEL_WIDTH - 1 - x;
        ny = EPD_PANEL_HEIGHT - 1 - y;
        break;
    case EPD_ROT_270:
        nx = EPD_PANEL_WIDTH - 1 - y;
        ny = x;
        break;
    case EPD_ROT_0:
    default:
        nx = x;
        ny = y;
        break;
    }

    uint8_t *cell = &s_fb[ny * EPD_ROW_BYTES + (nx >> 3)];
    uint8_t mask = 0x80 >> (nx & 7);
    if (color == EPD_COLOR_BLACK) {
        *cell &= (uint8_t)~mask;
    } else {
        *cell |= mask;
    }
}

void epaper_draw_hline(int x, int y, int w, uint8_t color)
{
    for (int i = 0; i < w; i++) {
        epaper_draw_pixel(x + i, y, color);
    }
}

void epaper_draw_vline(int x, int y, int h, uint8_t color)
{
    for (int i = 0; i < h; i++) {
        epaper_draw_pixel(x, y + i, color);
    }
}

void epaper_draw_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;   /* negative magnitude */
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        epaper_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void epaper_draw_rect(int x, int y, int w, int h, uint8_t color)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    epaper_draw_hline(x, y, w, color);
    epaper_draw_hline(x, y + h - 1, w, color);
    epaper_draw_vline(x, y, h, color);
    epaper_draw_vline(x + w - 1, y, h, color);
}

void epaper_fill_rect(int x, int y, int w, int h, uint8_t color)
{
    for (int j = 0; j < h; j++) {
        epaper_draw_hline(x, y + j, w, color);
    }
}

void epaper_draw_circle(int cx, int cy, int r, uint8_t color)
{
    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;

    do {
        epaper_draw_pixel(cx - x, cy + y, color);
        epaper_draw_pixel(cx + x, cy + y, color);
        epaper_draw_pixel(cx + x, cy - y, color);
        epaper_draw_pixel(cx - x, cy - y, color);
        r = err;
        if (r <= y) {
            err += ++y * 2 + 1;
        }
        if (r > x || err > y) {
            err += ++x * 2 + 1;
        }
    } while (x < 0);
}

/* Glyph bitmaps are packed MSB first and run continuously across rows. */
static int draw_glyph(int x, int y, unsigned char c, const gfx_font_t *font,
                      uint8_t color)
{
    if (c < font->first || c > font->last) {
        c = ' ';
        if (c < font->first || c > font->last) {
            return x;
        }
    }

    const gfx_glyph_t *g = &font->glyphs[c - font->first];
    const uint8_t *bits = &font->bitmap[g->offset];

    for (int row = 0; row < g->height; row++) {
        for (int col = 0; col < g->width; col++) {
            int bit = row * g->width + col;
            if (bits[bit >> 3] & (0x80 >> (bit & 7))) {
                epaper_draw_pixel(x + g->xoffset + col, y + g->yoffset + row,
                                  color);
            }
        }
    }
    return x + g->advance;
}

void epaper_fill_circle(int cx, int cy, int r, uint8_t color)
{
    for (int dy = -r; dy <= r; dy++) {
        /* Half-width of the circle at this row. */
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= r * r) {
            dx++;
        }
        epaper_draw_hline(cx - dx, cy + dy, 2 * dx + 1, color);
    }
}

void epaper_draw_thick_line(int x0, int y0, int x1, int y1, int thickness,
                            uint8_t color)
{
    if (thickness < 1) {
        thickness = 1;
    }
    int r = thickness / 2;

    /* Walk the line and stamp a disc at each step. Crude, but these are
     * decorative glyphs at a handful of pixels. */
    int steps = abs(x1 - x0) > abs(y1 - y0) ? abs(x1 - x0) : abs(y1 - y0);
    if (steps == 0) {
        epaper_fill_circle(x0, y0, r, color);
        return;
    }
    for (int i = 0; i <= steps; i++) {
        int x = x0 + (x1 - x0) * i / steps;
        int y = y0 + (y1 - y0) * i / steps;
        if (r > 0) {
            epaper_fill_circle(x, y, r, color);
        } else {
            epaper_draw_pixel(x, y, color);
        }
    }
}

int epaper_draw_text(int x, int y, const char *s, const gfx_font_t *font,
                     uint8_t color)
{
    while (*s) {
        x = draw_glyph(x, y, (unsigned char)*s++, font, color);
    }
    return x;
}

int epaper_text_width(const char *s, const gfx_font_t *font)
{
    int w = 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < font->first || c > font->last) {
            c = ' ';
            if (c < font->first || c > font->last) {
                continue;
            }
        }
        w += font->glyphs[c - font->first].advance;
    }
    return w;
}

int epaper_draw_text_right(int x_right, int y, const char *s,
                           const gfx_font_t *font, uint8_t color)
{
    return epaper_draw_text(x_right - epaper_text_width(s, font), y, s, font,
                            color);
}

void epaper_draw_text_center(int x, int w, int y, const char *s,
                             const gfx_font_t *font, uint8_t color)
{
    int tw = epaper_text_width(s, font);
    epaper_draw_text(x + (w - tw) / 2, y, s, font, color);
}
