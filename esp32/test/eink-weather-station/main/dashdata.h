/* Network side of the dashboard: WiFi, time, sensor discovery, and the two
 * Open-Meteo feeds. Everything here fills in the shared snapshot below, which
 * the render side reads.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define DASH_MAX_SENSORS 6
#define DASH_ROOM_LEN    32
#define DASH_ID_LEN      24

typedef struct {
    char room[DASH_ROOM_LEN];
    char id[DASH_ID_LEN];
    float temp;
    float humidity;
    int co2;
    int airq;        /* 0 unknown, 1 excellent .. 5 poor */
    bool alert;
    bool valid;      /* false until a fetch has succeeded */
    int64_t fetched_us;
} dash_sensor_t;

typedef struct {
    float temp;
    float apparent;
    float humidity;
    float wind_kph;
    int code;        /* WMO weather code */
    bool valid;
    int64_t fetched_us;
} dash_weather_t;

typedef struct {
    int eaqi;        /* European AQI */
    float pm2_5;
    float pm10;
    bool valid;
    int64_t fetched_us;
} dash_air_t;

typedef struct {
    dash_sensor_t sensors[DASH_MAX_SENSORS];
    int sensor_count;
    dash_weather_t weather;
    dash_air_t air;
    bool wifi_up;
    bool time_valid;
    char ip[16];
    int rssi;        /* dBm, 0 when unknown */
} dash_snapshot_t;

/* Join WiFi and start SNTP. Returns once an IP is up, or ESP_ERR_TIMEOUT. */
esp_err_t dash_net_start(void);

/* Browse _mazyiot._tcp and fetch /api/values from everything found. Also
 * refreshes weather and air quality when they are older than their interval.
 * Safe to call repeatedly; failures leave the previous values in place and
 * only clear the matching `valid` flag once the data goes properly stale. */
void dash_refresh(void);

/* Copy the current snapshot. */
void dash_get(dash_snapshot_t *out);

/* Human-readable text for a WMO weather code, e.g. "Light drizzle". */
const char *dash_weather_text(int wmo_code);

/* European AQI band, e.g. "Good". */
const char *dash_aqi_text(int eaqi);

/* Air quality level 0..5 as reported by the sensors. */
const char *dash_airq_text(int level);
