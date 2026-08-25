/* Panel side of the SSD1680 / SSD1675B driver for the LilyGO T5 V2.3 2.13".
 *
 * Drawing lives in epaper_gfx.h, which has no hardware dependencies. This
 * header only adds bring-up and the refresh operations.
 */
#pragma once

#include "epaper_gfx.h"
#include "esp_err.h"

/* Board pinout, taken from LilyGO boards.h, LILYGO_T5_V213 block. */
#define EPD_PIN_MOSI  23
#define EPD_PIN_SCLK  18
#define EPD_PIN_CS     5
#define EPD_PIN_DC    17
#define EPD_PIN_RST   16
#define EPD_PIN_BUSY   4

/* Bring up the SPI bus and reset/initialise the panel. Call once. */
esp_err_t epaper_init(void);

/* Release the panel and the SPI bus. */
void epaper_deinit(void);

/* Push the framebuffer and run a full refresh (flashing, ~2 s, best quality).
 * Use this at least every few dozen partial updates to clear ghosting. */
void epaper_refresh_full(void);

/* Push the framebuffer and run a fast non-flashing update. Leaves some
 * ghosting behind; only supported by SSD1680-class panels (see README). */
void epaper_refresh_partial(void);

/* Put the panel into deep sleep. epaper_init() must be called again after. */
void epaper_sleep(void);
