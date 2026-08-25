/* Screen rendering for the 250x122 landscape panel.
 *
 * Every screen uses the full panel: there is no persistent status bar, since
 * clock and link state live on their own screen at the end of the rotation.
 * Each screen is a headline value with supporting detail on a bottom row.
 */
#include "screens.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "dashdata.h"
#include "epaper_gfx.h"

#define MARGIN     6

/* Two-column skeleton shared by the room and air quality screens:
 *
 *   label .................  upper right
 *   HEADLINE unit
 *   qualifier .............  lower right
 *
 * The headline stack sits on the left, secondary values right-aligned. */
#define TITLE_Y     0
#define HEAD_Y      32
#define QUAL_Y      96
#define RIGHT_HI_Y  32
#define RIGHT_LO_Y  76

/* Weather detail line, sitting just above the bottom edge. */
#define DETAIL_Y    105

/* The title gets a full-width row of its own, so it only has to clear the
 * margins. Anything longer drops to a smaller face rather than being cut. */
#define TITLE_MAX_W (250 - 2 * MARGIN)

/* ------------------------------------------------------------------ */
/* Weather glyphs                                                      */
/* ------------------------------------------------------------------ */

typedef enum {
    ICON_SUN,
    ICON_PARTLY,
    ICON_CLOUD,
    ICON_RAIN,
    ICON_SNOW,
    ICON_FOG,
    ICON_STORM,
} icon_t;

static icon_t icon_for_code(int code)
{
    switch (code) {
    case 0:  return ICON_SUN;
    case 1:
    case 2:  return ICON_PARTLY;
    case 3:  return ICON_CLOUD;
    case 45:
    case 48: return ICON_FOG;
    case 71: case 73: case 75: case 77:
    case 85: case 86: return ICON_SNOW;
    case 95: case 96: case 99: return ICON_STORM;
    default: return ICON_RAIN;   /* drizzle, rain and showers */
    }
}

static void draw_sun(int cx, int cy, int r)
{
    epaper_fill_circle(cx, cy, r, EPD_COLOR_BLACK);
    for (int i = 0; i < 8; i++) {
        /* Eight rays on the diagonals and axes. */
        static const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
        static const int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        int x0 = cx + dx[i] * (r + 3);
        int y0 = cy + dy[i] * (r + 3);
        int x1 = cx + dx[i] * (r + 8);
        int y1 = cy + dy[i] * (r + 8);
        epaper_draw_thick_line(x0, y0, x1, y1, 3, EPD_COLOR_BLACK);
    }
}

/* Cloud as three overlapping discs on a slab; reads well as a silhouette. */
static void draw_cloud(int x, int y, int w)
{
    int r = w / 5;
    int base = y + r * 2;
    epaper_fill_circle(x + r * 3 / 2, base - r, r, EPD_COLOR_BLACK);
    epaper_fill_circle(x + w / 2, base - r * 3 / 2, r * 4 / 3, EPD_COLOR_BLACK);
    epaper_fill_circle(x + w - r * 3 / 2, base - r, r, EPD_COLOR_BLACK);
    epaper_fill_rect(x + r, base - r, w - r * 2, r, EPD_COLOR_BLACK);
}

static void draw_icon(icon_t icon, int x, int y, int size)
{
    switch (icon) {
    case ICON_SUN:
        draw_sun(x + size / 2, y + size / 2, size / 4);
        break;

    case ICON_PARTLY:
        draw_sun(x + size / 3, y + size / 3, size / 6);
        draw_cloud(x + size / 5, y + size / 2 - size / 8, size * 4 / 5);
        break;

    case ICON_CLOUD:
        draw_cloud(x, y + size / 5, size);
        break;

    case ICON_RAIN:
        draw_cloud(x, y, size);
        for (int i = 0; i < 3; i++) {
            int rx = x + size / 4 + i * size / 4;
            int ry = y + size * 3 / 5;
            epaper_draw_thick_line(rx, ry, rx - 4, ry + 12, 3, EPD_COLOR_BLACK);
        }
        break;

    case ICON_SNOW:
        draw_cloud(x, y, size);
        for (int i = 0; i < 3; i++) {
            epaper_fill_circle(x + size / 4 + i * size / 4, y + size * 3 / 4, 3,
                               EPD_COLOR_BLACK);
        }
        break;

    case ICON_FOG:
        draw_cloud(x, y - size / 8, size);
        for (int i = 0; i < 3; i++) {
            int fy = y + size * 3 / 5 + i * 8;
            epaper_fill_rect(x + (i % 2) * 6, fy, size - 8, 3, EPD_COLOR_BLACK);
        }
        break;

    case ICON_STORM:
        draw_cloud(x, y, size);
        /* Zigzag bolt below the cloud. */
        epaper_draw_thick_line(x + size / 2 + 4, y + size * 3 / 5,
                               x + size / 2 - 5, y + size * 4 / 5, 4,
                               EPD_COLOR_BLACK);
        epaper_draw_thick_line(x + size / 2 - 5, y + size * 4 / 5,
                               x + size / 2 + 3, y + size * 4 / 5, 4,
                               EPD_COLOR_BLACK);
        epaper_draw_thick_line(x + size / 2 + 3, y + size * 4 / 5,
                               x + size / 2 - 4, y + size + 2, 4,
                               EPD_COLOR_BLACK);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Text helpers                                                        */
/* ------------------------------------------------------------------ */

/* Degree sign as a ring. The generated fonts only cover ASCII, and drawing it
 * keeps the glyph tables from having to carry a Latin-1 range for one symbol.
 */
static void draw_degree(int x, int y, int radius, uint8_t color)
{
    uint8_t hole = (color == EPD_COLOR_WHITE) ? EPD_COLOR_BLACK : EPD_COLOR_WHITE;
    int inner = radius - (radius >= 5 ? 3 : 2);

    epaper_fill_circle(x + radius, y + radius, radius, color);
    epaper_fill_circle(x + radius, y + radius, inner, hole);
}

/* Draw "22.9" followed by a degree ring. Returns the x just past the ring. */
static int draw_temperature(int x, int y, float value, const gfx_font_t *font,
                            uint8_t color)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", value);

    int end = epaper_draw_text(x, y, buf, font, color);

    /* Deliberately small: a degree sign scaled with the font looks like a
     * letter O next to 56px digits. Capped at 4px radius. */
    int radius = font->ascent / 9;
    if (radius < 3) {
        radius = 3;
    } else if (radius > 4) {
        radius = 4;
    }
    draw_degree(end + 2, y + font->ascent / 7, radius, color);
    return end + 2 + radius * 2;
}

/* Greedy word wrap, used for room names in the narrow left column. */
static void draw_wrapped(int x, int y, int max_w, const char *text,
                         const gfx_font_t *font, uint8_t color)
{
    char line[80] = "";
    const char *p = text;

    while (*p) {
        char word[32];
        int n = 0;
        while (*p == ' ') {
            p++;
        }
        while (*p && *p != ' ' && n < (int)sizeof(word) - 1) {
            word[n++] = *p++;
        }
        word[n] = '\0';
        if (n == 0) {
            break;
        }

        char probe[sizeof(line) + sizeof(word) + 1];
        snprintf(probe, sizeof(probe), "%s%s%s", line, line[0] ? " " : "", word);

        if (line[0] && epaper_text_width(probe, font) > max_w) {
            epaper_draw_text(x, y, line, font, color);
            y += font->yadvance;
            strlcpy(line, word, sizeof(line));
        } else {
            strlcpy(line, probe, sizeof(line));
        }
    }
    if (line[0]) {
        epaper_draw_text(x, y, line, font, color);
    }
}

/* ------------------------------------------------------------------ */
/* Screens                                                             */
/* ------------------------------------------------------------------ */

static void draw_no_data(const char *title)
{
    epaper_draw_text(MARGIN, 0, title, &font_medium, EPD_COLOR_BLACK);
    epaper_draw_text(MARGIN, 40, "no data", &font_large, EPD_COLOR_BLACK);
}

/* Shared skeleton for the room and air quality screens: title across the top
 * left, a large headline with a small unit beside it, and a one-word verdict
 * underneath. Callers fill the right-hand column themselves. */
static void draw_skeleton(const char *title, const char *headline,
                          const char *unit, const char *qualifier)
{
    /* One fixed size for every screen title, so the rotation does not appear
     * to change typeface between rooms. The fallback only exists so an
     * unusually long room name degrades instead of running off the panel. */
    const gfx_font_t *title_font = &font_title;
    if (epaper_text_width(title, title_font) > TITLE_MAX_W) {
        title_font = &font_medium;
    }
    epaper_draw_text(MARGIN, TITLE_Y, title, title_font, EPD_COLOR_BLACK);

    int end = epaper_draw_text(MARGIN, HEAD_Y, headline, &font_large,
                               EPD_COLOR_BLACK);

    /* Unit sits on the digits' baseline, and stays small: at four digits a
     * medium unit runs into the right-hand column. Optional. */
    if (unit && *unit) {
        epaper_draw_text(end + 3, HEAD_Y + font_large.ascent - font_tiny.ascent,
                         unit, &font_tiny, EPD_COLOR_BLACK);
    }

    epaper_draw_text(MARGIN, QUAL_Y, qualifier, &font_medium, EPD_COLOR_BLACK);
}

/* Right-aligned humidity: number at full size with a half-height "%" tucked
 * straight against it, matching how the degree ring reads. */
static void draw_humidity(int right, int y, float value, uint8_t color)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f", value);

    int nw = epaper_text_width(buf, &font_xl);
    int pw = epaper_text_width("%", &font_medium);
    int x = right - nw - pw;

    epaper_draw_text(x, y, buf, &font_xl, color);
    epaper_draw_text(x + nw, y + font_xl.ascent - font_medium.ascent, "%",
                     &font_medium, color);
}

/* A right-aligned "label value" pair, with the label small and the value big,
 * their baselines lined up. */
static void draw_labeled_value(int right, int y, const char *label,
                               const char *value)
{
    /* font_xl so the PM figures match temperature and humidity on the room
     * screens; the label stays small to keep the value dominant. */
    int vw = epaper_text_width(value, &font_xl);
    epaper_draw_text(right - vw, y, value, &font_xl, EPD_COLOR_BLACK);

    int lw = epaper_text_width(label, &font_small);
    epaper_draw_text(right - vw - 5 - lw,
                     y + font_xl.ascent - font_small.ascent, label,
                     &font_small, EPD_COLOR_BLACK);
}

/* CO2 is the headline: it is the number that should make someone act. */
static void draw_room_screen(const dash_sensor_t *s)
{
    char buf[32];

    if (!s->valid) {
        draw_no_data(s->room);
        return;
    }

    snprintf(buf, sizeof(buf), "%d", s->co2);
    draw_skeleton(s->room, buf, "ppm",
                  s->alert ? "VENTILATE" : dash_airq_text(s->airq));

    const int right = epaper_width() - MARGIN;

    /* Right-aligning a temperature means measuring the ring as well as the
     * digits, since draw_temperature appends it. */
    char temp[16];
    snprintf(temp, sizeof(temp), "%.1f", s->temp);
    int w = epaper_text_width(temp, &font_xl) + 2 + 8;
    draw_temperature(right - w, RIGHT_HI_Y, s->temp, &font_xl,
                     EPD_COLOR_BLACK);

    draw_humidity(right, RIGHT_LO_Y, s->humidity, EPD_COLOR_BLACK);
}

static void draw_weather_screen(const dash_weather_t *w)
{
    char buf[40];

    if (!w->valid) {
        draw_no_data("Outdoor");
        return;
    }

    /* Same title row and headline position as the other screens. */
    epaper_draw_text(MARGIN, TITLE_Y, "Outdoor", &font_title,
                     EPD_COLOR_BLACK);

    draw_temperature(MARGIN, HEAD_Y - 2, w->temp, &font_large, EPD_COLOR_BLACK);

    /* Icon in the right column, its label wrapped underneath because
     * "Freezing drizzle" is wider than the icon. */
    const int icon_w = 58;
    const int icon_x = epaper_width() - icon_w - MARGIN;
    draw_icon(icon_for_code(w->code), icon_x, HEAD_Y, icon_w);
    draw_wrapped(icon_x, 90, icon_w + MARGIN, dash_weather_text(w->code),
                 &font_small, EPD_COLOR_BLACK);

    /* Kept short so it clears the icon column above it. */
    int x = epaper_draw_text(MARGIN, DETAIL_Y, "feels", &font_small,
                             EPD_COLOR_BLACK);
    x = draw_temperature(x + 4, DETAIL_Y, w->apparent, &font_small,
                         EPD_COLOR_BLACK);
    snprintf(buf, sizeof(buf), "  %.0f%%  %.0f km/h", w->humidity, w->wind_kph);
    epaper_draw_text(x, DETAIL_Y, buf, &font_small, EPD_COLOR_BLACK);
}

static void draw_air_screen(const dash_air_t *a)
{
    char buf[40];

    if (!a->valid) {
        draw_no_data("Air quality");
        return;
    }

    /* Identical skeleton to a room screen, with the two PM rows taking the
     * place of temperature and humidity. */
    snprintf(buf, sizeof(buf), "%d", a->eaqi);
    draw_skeleton("Air quality", buf, NULL, dash_aqi_text(a->eaqi));

    const int right = epaper_width() - MARGIN;

    snprintf(buf, sizeof(buf), "%.1f", a->pm2_5);
    draw_labeled_value(right, RIGHT_HI_Y, "PM2.5", buf);

    snprintf(buf, sizeof(buf), "%.1f", a->pm10);
    draw_labeled_value(right, RIGHT_LO_Y, "PM10", buf);
}

/* Clock, date and link state, which used to crowd every other screen. */
static void draw_status_screen(const dash_snapshot_t *s)
{
    static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char buf[48];

    if (s->time_valid) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);

        snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
        epaper_draw_text_center(0, epaper_width(), 2, buf, &font_large,
                                EPD_COLOR_BLACK);

        snprintf(buf, sizeof(buf), "%s %02d.%02d.%04d", days[tm.tm_wday],
                 tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
        epaper_draw_text_center(0, epaper_width(), 68, buf, &font_medium,
                                EPD_COLOR_BLACK);
    } else {
        epaper_draw_text_center(0, epaper_width(), 30, "--:--", &font_large,
                                EPD_COLOR_BLACK);
    }

    if (s->wifi_up && s->rssi != 0) {
        snprintf(buf, sizeof(buf), "online   %d dBm   %d sensors", s->rssi,
                 s->sensor_count);
    } else {
        snprintf(buf, sizeof(buf), "%s   %d sensors",
                 s->wifi_up ? "online" : "OFFLINE", s->sensor_count);
    }
    epaper_draw_text_center(0, epaper_width(), 104, buf, &font_small,
                            EPD_COLOR_BLACK);
}

int screens_count(const dash_snapshot_t *s)
{
    /* One per room, plus weather, air quality and status. */
    return s->sensor_count + 3;
}

void screens_draw(const dash_snapshot_t *s, int index)
{
    epaper_clear(EPD_COLOR_WHITE);

    if (index < s->sensor_count) {
        draw_room_screen(&s->sensors[index]);
    } else if (index == s->sensor_count) {
        draw_weather_screen(&s->weather);
    } else if (index == s->sensor_count + 1) {
        draw_air_screen(&s->air);
    } else {
        draw_status_screen(s);
    }
}
