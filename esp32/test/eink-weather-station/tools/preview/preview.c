/* Renders the dashboard screens on the host and writes them as PGM images.
 *
 * Links the real screens.c and epaper_gfx.c, so what comes out is exactly what
 * the panel would show - no reimplementation to drift out of sync. Only the
 * network half is stubbed, since the preview supplies its own data.
 *
 * See tools/preview/render.sh.
 */
#include <stdio.h>
#include <string.h>

#include "dashdata.h"
#include "epaper_gfx.h"
#include "screens.h"

/* Normally in dashdata.c, which cannot build on the host. */
const char *dash_weather_text(int code)
{
    switch (code) {
    case 0:  return "Clear";
    case 1:
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45:
    case 48: return "Fog";
    case 51: return "Light drizzle";
    case 53: return "Drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 71: return "Light snow";
    case 95: return "Thunderstorm";
    default: return "Unknown";
    }
}

const char *dash_aqi_text(int eaqi)
{
    if (eaqi <= 20)  return "Good";
    if (eaqi <= 40)  return "Fair";
    if (eaqi <= 60)  return "Moderate";
    if (eaqi <= 80)  return "Poor";
    if (eaqi <= 100) return "Very poor";
    return "Extremely poor";
}

const char *dash_airq_text(int level)
{
    switch (level) {
    case 1:  return "Excellent";
    case 2:  return "Good";
    case 3:  return "Fair";
    case 4:  return "Inferior";
    case 5:  return "Poor";
    default: return "Unknown";
    }
}

esp_err_t dash_net_start(void) { return ESP_OK; }
void dash_refresh(void) {}
void dash_get(dash_snapshot_t *out) { (void)out; }

/* Unpack the 1-bit framebuffer into a PGM, honouring the rotation so the
 * output is oriented the way the panel is read. */
static void write_pgm(const char *path)
{
    int w = epaper_width();
    int h = epaper_height();

    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        return;
    }
    fprintf(f, "P5\n%d %d\n255\n", w, h);

    const uint8_t *fb = epaper_framebuffer();
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            /* Mirror the rotation mapping used by epaper_draw_pixel. */
            int nx = y;
            int ny = EPD_PANEL_HEIGHT - 1 - x;
            int bit = fb[ny * EPD_ROW_BYTES + (nx >> 3)] & (0x80 >> (nx & 7));
            fputc(bit ? 0xFF : 0x00, f);
        }
    }
    fclose(f);
}

int main(void)
{
    dash_snapshot_t s = {0};

    s.wifi_up = true;
    s.time_valid = true;
    s.rssi = -57;
    s.sensor_count = 3;
    strcpy(s.ip, "192.168.77.142");

    struct { const char *room; float t, h; int co2, airq; bool alert; } rooms[] = {
        {"Son's room",      22.4f, 63.0f,  726, 1, false},
        {"Daughter's room", 22.9f, 68.0f, 1834, 4, true},
        {"Main bedroom",    22.5f, 62.0f,  659, 2, false},
    };
    for (int i = 0; i < 3; i++) {
        strcpy(s.sensors[i].room, rooms[i].room);
        s.sensors[i].temp = rooms[i].t;
        s.sensors[i].humidity = rooms[i].h;
        s.sensors[i].co2 = rooms[i].co2;
        s.sensors[i].airq = rooms[i].airq;
        s.sensors[i].alert = rooms[i].alert;
        s.sensors[i].valid = true;
    }

    s.weather.valid = true;
    s.weather.temp = 14.0f;
    s.weather.apparent = 13.8f;
    s.weather.humidity = 92.0f;
    s.weather.wind_kph = 8.3f;
    s.weather.code = 3;

    s.air.valid = true;
    s.air.eaqi = 24;
    s.air.pm2_5 = 6.3f;
    s.air.pm10 = 10.1f;

    epaper_set_rotation(EPD_ROT_90);

    static const char *names[] = {
        "01-son", "02-daughter-alert", "03-bedroom", "04-weather", "05-air",
        "06-status",
    };
    int count = screens_count(&s);
    for (int i = 0; i < count; i++) {
        char path[64];
        snprintf(path, sizeof(path), "out/%s.pgm", names[i]);
        screens_draw(&s, i);
        write_pgm(path);
        printf("wrote %s\n", path);
    }
    return 0;
}
