#include "epaper.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "font5x7.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "epaper";

/* SSD1680 / SSD1675B commands used here. */
#define CMD_DRIVER_OUTPUT      0x01
#define CMD_DEEP_SLEEP         0x10
#define CMD_DATA_ENTRY_MODE    0x11
#define CMD_SW_RESET           0x12
#define CMD_TEMP_SENSOR        0x18
#define CMD_MASTER_ACTIVATE    0x20
#define CMD_UPDATE_CTRL1       0x21
#define CMD_UPDATE_CTRL2       0x22
#define CMD_WRITE_RAM_BW       0x24
#define CMD_WRITE_RAM_RED      0x26   /* doubles as the "previous image" RAM */
#define CMD_BORDER_WAVEFORM    0x3C
#define CMD_SET_RAM_X_RANGE    0x44
#define CMD_SET_RAM_Y_RANGE    0x45
#define CMD_SET_RAM_X_COUNTER  0x4E
#define CMD_SET_RAM_Y_COUNTER  0x4F

/* Display update sequences selected via CMD_UPDATE_CTRL2. */
#define UPDATE_SEQ_FULL        0xF7   /* load OTP waveform mode 1, flashing */
#define UPDATE_SEQ_PARTIAL     0xFF   /* load OTP waveform mode 2, no flash */

#define BUSY_TIMEOUT_MS        10000

/* 1 = white, 0 = black, matching what the panel expects in RAM. */
static uint8_t s_fb[EPD_BUF_SIZE];
static spi_device_handle_t s_spi;
static epaper_rotation_t s_rot = EPD_ROT_90;

/* ------------------------------------------------------------------ */
/* Low level                                                           */
/* ------------------------------------------------------------------ */

static void spi_write(const uint8_t *data, size_t len)
{
    if (len == 0) {
        return;
    }
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void write_cmd(uint8_t cmd)
{
    gpio_set_level(EPD_PIN_DC, 0);
    spi_write(&cmd, 1);
    gpio_set_level(EPD_PIN_DC, 1);
}

static void write_data(const uint8_t *data, size_t len)
{
    gpio_set_level(EPD_PIN_DC, 1);
    spi_write(data, len);
}

static void write_cmd_data(uint8_t cmd, const uint8_t *data, size_t len)
{
    write_cmd(cmd);
    write_data(data, len);
}

static void wait_busy(void)
{
    /* BUSY is driven high while the panel is working. */
    int waited_ms = 0;
    while (gpio_get_level(EPD_PIN_BUSY)) {
        vTaskDelay(pdMS_TO_TICKS(10));
        waited_ms += 10;
        if (waited_ms >= BUSY_TIMEOUT_MS) {
            ESP_LOGE(TAG, "BUSY stuck high for %d ms, giving up", waited_ms);
            return;
        }
    }
}

static void hw_reset(void)
{
    gpio_set_level(EPD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(EPD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    wait_busy();
}

/* Point the RAM window at the whole panel and rewind the address counters. */
static void set_ram_window(void)
{
    const uint8_t x_range[] = {0x00, EPD_ROW_BYTES - 1};
    const uint8_t y_range[] = {
        0x00, 0x00, (EPD_PANEL_HEIGHT - 1) & 0xFF, (EPD_PANEL_HEIGHT - 1) >> 8
    };
    const uint8_t x_start = 0x00;
    const uint8_t y_start[] = {0x00, 0x00};

    write_cmd_data(CMD_SET_RAM_X_RANGE, x_range, sizeof(x_range));
    write_cmd_data(CMD_SET_RAM_Y_RANGE, y_range, sizeof(y_range));
    write_cmd_data(CMD_SET_RAM_X_COUNTER, &x_start, 1);
    write_cmd_data(CMD_SET_RAM_Y_COUNTER, y_start, sizeof(y_start));
}

static void panel_init(void)
{
    hw_reset();

    write_cmd(CMD_SW_RESET);
    wait_busy();

    /* MUX = 250 gate lines, scan from G0, no interlacing. */
    const uint8_t driver_output[] = {
        (EPD_PANEL_HEIGHT - 1) & 0xFF, (EPD_PANEL_HEIGHT - 1) >> 8, 0x00
    };
    write_cmd_data(CMD_DRIVER_OUTPUT, driver_output, sizeof(driver_output));

    /* X and Y both increment, so framebuffer row 0 lands on gate 0. */
    const uint8_t entry_mode = 0x03;
    write_cmd_data(CMD_DATA_ENTRY_MODE, &entry_mode, 1);

    set_ram_window();

    const uint8_t border = 0x05;    /* follow LUT1, keeps the frame white */
    write_cmd_data(CMD_BORDER_WAVEFORM, &border, 1);

    const uint8_t temp_sensor = 0x80;   /* use the internal sensor */
    write_cmd_data(CMD_TEMP_SENSOR, &temp_sensor, 1);

    const uint8_t update_ctrl1[] = {0x00, 0x80};
    write_cmd_data(CMD_UPDATE_CTRL1, update_ctrl1, sizeof(update_ctrl1));

    wait_busy();
}

/* Send the framebuffer to one of the two RAM banks. */
static void write_ram(uint8_t cmd)
{
    set_ram_window();
    write_cmd_data(cmd, s_fb, sizeof(s_fb));
}

static void run_update(uint8_t sequence)
{
    write_cmd_data(CMD_UPDATE_CTRL2, &sequence, 1);
    write_cmd(CMD_MASTER_ACTIVATE);
    wait_busy();
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t epaper_init(void)
{
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << EPD_PIN_DC) | (1ULL << EPD_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&out_cfg));

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << EPD_PIN_BUSY),
        .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&in_cfg));

    gpio_set_level(EPD_PIN_RST, 1);
    gpio_set_level(EPD_PIN_DC, 1);

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = EPD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = EPD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EPD_BUF_SIZE + 8,
    };
    esp_err_t err = spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return err;
    }

    spi_device_interface_config_t dev_cfg = {
        .mode = 0,
        .clock_speed_hz = 10 * 1000 * 1000,
        .spics_io_num = EPD_PIN_CS,
        .queue_size = 1,
    };
    err = spi_bus_add_device(SPI3_HOST, &dev_cfg, &s_spi);
    if (err != ESP_OK) {
        spi_bus_free(SPI3_HOST);
        return err;
    }

    panel_init();
    epaper_clear(EPD_COLOR_WHITE);
    ESP_LOGI(TAG, "panel ready, %dx%d native", EPD_PANEL_WIDTH, EPD_PANEL_HEIGHT);
    return ESP_OK;
}

void epaper_deinit(void)
{
    spi_bus_remove_device(s_spi);
    s_spi = NULL;
    spi_bus_free(SPI3_HOST);
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

int epaper_draw_char(int x, int y, char c, int scale, uint8_t color)
{
    if (scale < 1) {
        scale = 1;
    }
    if ((unsigned char)c < FONT_FIRST_CHAR || (unsigned char)c > FONT_LAST_CHAR) {
        c = '?';
    }

    const uint8_t *glyph = &font5x7[((unsigned char)c - FONT_FIRST_CHAR) * FONT_WIDTH];
    for (int col = 0; col < FONT_WIDTH; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < FONT_HEIGHT; row++) {
            if (bits & (1 << row)) {
                epaper_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
    return x + (FONT_WIDTH + 1) * scale;
}

int epaper_draw_string(int x, int y, const char *s, int scale, uint8_t color)
{
    while (*s) {
        x = epaper_draw_char(x, y, *s++, scale, color);
    }
    return x;
}

void epaper_refresh_full(void)
{
    write_ram(CMD_WRITE_RAM_BW);
    run_update(UPDATE_SEQ_FULL);
    /* Seed the previous-image RAM so the next partial update has a reference. */
    write_ram(CMD_WRITE_RAM_RED);
}

void epaper_refresh_partial(void)
{
    const uint8_t border = 0x80;    /* hold the border during a partial update */
    write_cmd_data(CMD_BORDER_WAVEFORM, &border, 1);

    write_ram(CMD_WRITE_RAM_BW);
    run_update(UPDATE_SEQ_PARTIAL);
    write_ram(CMD_WRITE_RAM_RED);
}

void epaper_sleep(void)
{
    const uint8_t mode = 0x01;   /* deep sleep mode 1, RAM retained */
    write_cmd_data(CMD_DEEP_SLEEP, &mode, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}
