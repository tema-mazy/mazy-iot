#include "dashdata.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs_flash.h"

static const char *TAG = "dashdata";

/* Open-Meteo only recomputes hourly, so polling it hard buys nothing. */
#define WEATHER_INTERVAL_US (15 * 60 * 1000000LL)
#define AIR_INTERVAL_US     (30 * 60 * 1000000LL)

/* Drop a sensor's values once nothing has been heard for this long, rather
 * than showing a number that quietly stopped updating. */
#define SENSOR_STALE_US     (10 * 60 * 1000000LL)

#define WIFI_CONNECTED_BIT BIT0
#define HTTP_BUF_SIZE      1024

static dash_snapshot_t s_snap;
static SemaphoreHandle_t s_lock;
static EventGroupHandle_t s_wifi_events;

/* ------------------------------------------------------------------ */
/* WiFi and time                                                       */
/* ------------------------------------------------------------------ */

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id,
                               void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        s_snap.wifi_up = false;
        ESP_LOGW(TAG, "WiFi dropped, reconnecting");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(s_snap.ip, sizeof(s_snap.ip), IPSTR, IP2STR(&event->ip_info.ip));
        s_snap.wifi_up = true;
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void time_start(void)
{
    /* Poland: CET/CEST, last Sunday of March to last Sunday of October. */
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&cfg);

    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) == ESP_OK) {
        s_snap.time_valid = true;
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        ESP_LOGI(TAG, "time synced: %04d-%02d-%02d %02d:%02d", tm.tm_year + 1900,
                 tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    } else {
        ESP_LOGW(TAG, "SNTP did not sync in time, will keep trying");
    }
}

esp_err_t dash_net_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_wifi_events = xEventGroupCreate();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    wifi_config_t sta = {0};
    strlcpy((char *)sta.sta.ssid, CONFIG_DASH_WIFI_SSID, sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, CONFIG_DASH_WIFI_PASSWORD,
            sizeof(sta.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(30000));
    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "no IP after 30 s");
        return ESP_ERR_TIMEOUT;
    }

    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set("eink-dash");

    time_start();
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* HTTP                                                                */
/* ------------------------------------------------------------------ */

static esp_err_t http_get(const char *url, char *buf, size_t buflen)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGW(TAG, "%s -> HTTP %d", url, status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int total = 0;
    while (total < (int)buflen - 1) {
        int n = esp_http_client_read(client, buf + total, buflen - 1 - total);
        if (n <= 0) {
            break;
        }
        total += n;
    }
    buf[total] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return total > 0 ? ESP_OK : ESP_FAIL;
}

/* ------------------------------------------------------------------ */
/* Feeds                                                               */
/* ------------------------------------------------------------------ */

static void parse_sensor(const char *json, dash_sensor_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return;
    }

    const cJSON *room = cJSON_GetObjectItem(root, "room");
    const cJSON *id = cJSON_GetObjectItem(root, "id");
    const cJSON *temp = cJSON_GetObjectItem(root, "temp");
    const cJSON *hum = cJSON_GetObjectItem(root, "humidity");
    const cJSON *co2 = cJSON_GetObjectItem(root, "co2");
    const cJSON *airq = cJSON_GetObjectItem(root, "airq");
    const cJSON *alert = cJSON_GetObjectItem(root, "alert");

    if (cJSON_IsString(room)) {
        strlcpy(out->room, room->valuestring, sizeof(out->room));
    }
    if (cJSON_IsString(id)) {
        strlcpy(out->id, id->valuestring, sizeof(out->id));
    }
    if (cJSON_IsNumber(temp)) {
        out->temp = (float)temp->valuedouble;
    }
    if (cJSON_IsNumber(hum)) {
        out->humidity = (float)hum->valuedouble;
    }
    if (cJSON_IsNumber(co2)) {
        out->co2 = co2->valueint;
    }
    if (cJSON_IsNumber(airq)) {
        out->airq = airq->valueint;
    }
    out->alert = cJSON_IsTrue(alert);
    out->valid = true;
    out->fetched_us = esp_timer_get_time();

    cJSON_Delete(root);
}

/* Fetch one sensor by address. Returns true when `out` was filled in. */
static bool fetch_sensor(esp_ip4_addr_t ip, uint16_t port, char *buf,
                         dash_sensor_t *out)
{
    char url[96];
    snprintf(url, sizeof(url), "http://" IPSTR ":%u/api/values", IP2STR(&ip),
             port);

    if (http_get(url, buf, HTTP_BUF_SIZE) != ESP_OK) {
        ESP_LOGW(TAG, "fetch failed: %s", url);
        return false;
    }

    parse_sensor(buf, out);
    if (!out->valid) {
        return false;
    }

    ESP_LOGI(TAG, "%s: %.1fC %.0f%% %dppm", out->room, out->temp,
             out->humidity, out->co2);
    return true;
}

/* Resolve the configured hostnames directly. Service browsing needs multicast
 * that some access points drop, while plain .local resolution still works. */
static int fetch_configured_hosts(char *buf, dash_sensor_t *found)
{
    const char *hosts = CONFIG_DASH_SENSOR_HOSTS;
    if (!hosts || !*hosts) {
        return 0;
    }

    char list[192];
    strlcpy(list, hosts, sizeof(list));

    int count = 0;
    char *save = NULL;
    for (char *name = strtok_r(list, ",", &save);
         name && count < DASH_MAX_SENSORS;
         name = strtok_r(NULL, ",", &save)) {

        while (*name == ' ') {
            name++;
        }
        if (!*name) {
            continue;
        }

        esp_ip4_addr_t ip = {0};
        if (mdns_query_a(name, 3000, &ip) != ESP_OK) {
            ESP_LOGW(TAG, "cannot resolve %s.local", name);
            continue;
        }
        if (fetch_sensor(ip, 8080, buf, &found[count])) {
            count++;
        }
    }
    return count;
}

static void refresh_sensors(char *buf)
{
    /* One attempt only. Where browsing works this finds everything first try;
     * where the AP drops multicast it never succeeds, and retrying just burns
     * seconds on every refresh. The configured-hosts path picks up the slack.
     */
    mdns_result_t *results = NULL;
    if (mdns_query_ptr("_mazyiot", "_tcp", 3000, DASH_MAX_SENSORS,
                       &results) != ESP_OK) {
        results = NULL;
    }

    dash_sensor_t found[DASH_MAX_SENSORS] = {0};
    int count = 0;

    if (!results) {
        ESP_LOGW(TAG, "_mazyiot._tcp browse empty, trying configured hosts");
        count = fetch_configured_hosts(buf, found);
        if (count == 0) {
            ESP_LOGW(TAG, "no sensors reachable");
            return;
        }
        goto publish;
    }

    for (mdns_result_t *r = results; r && count < DASH_MAX_SENSORS; r = r->next) {
        /* Only IPv4 here; v4 is enough and keeps the URL simple. */
        esp_ip4_addr_t ip = {0};
        for (mdns_ip_addr_t *a = r->addr; a; a = a->next) {
            if (a->addr.type == ESP_IPADDR_TYPE_V4) {
                ip = a->addr.u_addr.ip4;
                break;
            }
        }

        /* A PTR answer does not always carry the A record, in which case the
         * hostname has to be resolved separately. */
        if (ip.addr == 0 && r->hostname) {
            if (mdns_query_a(r->hostname, 3000, &ip) != ESP_OK) {
                ESP_LOGW(TAG, "cannot resolve %s.local", r->hostname);
                continue;
            }
        }
        if (ip.addr == 0) {
            continue;
        }

        if (fetch_sensor(ip, r->port, buf, &found[count])) {
            count++;
        }
    }
    mdns_query_results_free(results);

    if (count == 0) {
        return;   /* keep whatever we had rather than blanking the screens */
    }

publish:

    /* Stable ordering, so screens do not shuffle between refreshes. */
    for (int i = 1; i < count; i++) {
        dash_sensor_t key = found[i];
        int j = i - 1;
        while (j >= 0 && strcmp(found[j].room, key.room) > 0) {
            found[j + 1] = found[j];
            j--;
        }
        found[j + 1] = key;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(s_snap.sensors, found, sizeof(found));
    s_snap.sensor_count = count;
    xSemaphoreGive(s_lock);
}

static void refresh_weather(char *buf)
{
    int64_t now = esp_timer_get_time();
    if (s_snap.weather.valid && now - s_snap.weather.fetched_us < WEATHER_INTERVAL_US) {
        return;
    }

    char url[256];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
             "weather_code,wind_speed_10m&timezone=Europe%%2FWarsaw",
             CONFIG_DASH_WEATHER_LAT, CONFIG_DASH_WEATHER_LON);

    if (http_get(url, buf, HTTP_BUF_SIZE) != ESP_OK) {
        ESP_LOGW(TAG, "weather fetch failed");
        return;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return;
    }
    const cJSON *cur = cJSON_GetObjectItem(root, "current");
    if (cJSON_IsObject(cur)) {
        dash_weather_t w = {0};
        const cJSON *t = cJSON_GetObjectItem(cur, "temperature_2m");
        const cJSON *a = cJSON_GetObjectItem(cur, "apparent_temperature");
        const cJSON *h = cJSON_GetObjectItem(cur, "relative_humidity_2m");
        const cJSON *c = cJSON_GetObjectItem(cur, "weather_code");
        const cJSON *s = cJSON_GetObjectItem(cur, "wind_speed_10m");

        if (cJSON_IsNumber(t)) w.temp = (float)t->valuedouble;
        if (cJSON_IsNumber(a)) w.apparent = (float)a->valuedouble;
        if (cJSON_IsNumber(h)) w.humidity = (float)h->valuedouble;
        if (cJSON_IsNumber(c)) w.code = c->valueint;
        if (cJSON_IsNumber(s)) w.wind_kph = (float)s->valuedouble;
        w.valid = true;
        w.fetched_us = esp_timer_get_time();

        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_snap.weather = w;
        xSemaphoreGive(s_lock);

        ESP_LOGI(TAG, "weather: %.1fC code %d wind %.1f", w.temp, w.code,
                 w.wind_kph);
    }
    cJSON_Delete(root);
}

static void refresh_air(char *buf)
{
    int64_t now = esp_timer_get_time();
    if (s_snap.air.valid && now - s_snap.air.fetched_us < AIR_INTERVAL_US) {
        return;
    }

    char url[256];
    snprintf(url, sizeof(url),
             "https://air-quality-api.open-meteo.com/v1/air-quality"
             "?latitude=%s&longitude=%s&current=european_aqi,pm10,pm2_5"
             "&timezone=Europe%%2FWarsaw",
             CONFIG_DASH_WEATHER_LAT, CONFIG_DASH_WEATHER_LON);

    if (http_get(url, buf, HTTP_BUF_SIZE) != ESP_OK) {
        ESP_LOGW(TAG, "air quality fetch failed");
        return;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return;
    }
    const cJSON *cur = cJSON_GetObjectItem(root, "current");
    if (cJSON_IsObject(cur)) {
        dash_air_t a = {0};
        const cJSON *aqi = cJSON_GetObjectItem(cur, "european_aqi");
        const cJSON *pm25 = cJSON_GetObjectItem(cur, "pm2_5");
        const cJSON *pm10 = cJSON_GetObjectItem(cur, "pm10");

        if (cJSON_IsNumber(aqi)) a.eaqi = aqi->valueint;
        if (cJSON_IsNumber(pm25)) a.pm2_5 = (float)pm25->valuedouble;
        if (cJSON_IsNumber(pm10)) a.pm10 = (float)pm10->valuedouble;
        a.valid = true;
        a.fetched_us = esp_timer_get_time();

        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_snap.air = a;
        xSemaphoreGive(s_lock);

        ESP_LOGI(TAG, "air: EAQI %d PM2.5 %.1f PM10 %.1f", a.eaqi, a.pm2_5,
                 a.pm10);
    }
    cJSON_Delete(root);
}

void dash_refresh(void)
{
    /* One shared buffer: these run in sequence on the same task. */
    static char buf[HTTP_BUF_SIZE];

    refresh_sensors(buf);
    refresh_weather(buf);
    refresh_air(buf);

    wifi_ap_record_t ap;
    int rssi = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;

    /* Expire anything that has gone quiet for too long. */
    int64_t now = esp_timer_get_time();
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_snap.rssi = rssi;
    for (int i = 0; i < s_snap.sensor_count; i++) {
        if (s_snap.sensors[i].valid &&
            now - s_snap.sensors[i].fetched_us > SENSOR_STALE_US) {
            s_snap.sensors[i].valid = false;
        }
    }
    if (!s_snap.time_valid) {
        time_t t = time(NULL);
        s_snap.time_valid = t > 1700000000;   /* clock has left 1970 */
    }
    xSemaphoreGive(s_lock);
}

void dash_get(dash_snapshot_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_snap;
    xSemaphoreGive(s_lock);
}

/* ------------------------------------------------------------------ */
/* Text helpers                                                        */
/* ------------------------------------------------------------------ */

const char *dash_weather_text(int code)
{
    switch (code) {
    case 0:  return "Clear";
    case 1:  return "Mostly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45:
    case 48: return "Fog";
    case 51: return "Light drizzle";
    case 53: return "Drizzle";
    case 55: return "Heavy drizzle";
    case 56:
    case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66:
    case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: return "Light showers";
    case 81: return "Showers";
    case 82: return "Heavy showers";
    case 85:
    case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96:
    case 99: return "Thunder, hail";
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
