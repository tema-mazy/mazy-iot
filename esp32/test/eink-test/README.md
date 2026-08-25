# Mazy's IoT Dashboard - LilyGO T5 V2.3 2.13" e-paper

ESP-IDF firmware for the LilyGO/TTGO T5 V2.3 (ESP32-WROVER + 2.13" e-paper).
It reads the mazy-iot sensors over the LAN, pulls Krakow weather and air
quality from Open-Meteo, and rotates through one screen per source.

No external components beyond cJSON and mDNS.

## Screens

The rotation is derived from the data, not hardcoded: `sensor_count + 3`. Add a
fourth sensor and it becomes seven screens with no code change.

    Son's room                 Outdoor                    Air quality
    726 ppm        22.4 deg    14.0 deg    [icon]         24            PM2.5 6.3
    Excellent          63%     feels 13.8  Overcast       Fair          PM10 10.1

- **Room** (one per sensor) - CO2 is the headline, since it is the number that
  should make someone act. Air quality word underneath, temperature and
  humidity right. Over the CO2 threshold the qualifier becomes `VENTILATE`.
- **Outdoor** - temperature, a drawn weather glyph with its condition
  underneath, and a feels-like/humidity/wind line.
- **Air quality** - European AQI headline with its band, PM2.5 and PM10 right.
- **Status** - clock, date, link state, RSSI and sensor count. This exists so
  the other screens do not need a permanent status bar eating a row.

Room and air quality share one `draw_skeleton()`, so they cannot drift apart.
All titles use one fixed size; a very long room name falls back a size rather
than running off the panel.

## Layout preview - use this instead of flashing

    tools/preview/render.sh

Renders every screen to `tools/preview/out/*.png` on the host, in seconds. It
compiles the real `screens.c`, `epaper_gfx.c` and `fonts.c`, so the output is
pixel-identical to the panel - there is no second implementation to drift.
Only the network half is stubbed, and the preview supplies its own data,
including a CO2 alert case that is otherwise awkward to reproduce.

This is why `epaper.c` is split in two:

- `epaper_gfx.c` - framebuffer, primitives, text. No hardware, builds anywhere.
- `epaper.c` - SPI and panel bring-up only.

Check a layout change here before it goes near the board.

## Fonts

Arial Bold rendered from the system TTF into packed 1-bit bitmaps, not a 5x7
grid scaled up. Regenerate with:

    python3 tools/genfont.py main/fonts.c main/fonts.h

| Font          | px | Coverage    | Used for                        |
|---------------|----|-------------|---------------------------------|
| `font_tiny`   | 10 | ASCII       | unit labels beside a headline   |
| `font_small`  | 13 | ASCII       | detail lines, value labels      |
| `font_medium` | 21 | ASCII       | qualifiers, date                |
| `font_title`  | 26 | ASCII       | screen titles                   |
| `font_xl`     | 36 | digits      | temperature, humidity, PM       |
| `font_large`  | 56 | digits      | headline numbers, clock         |

The digit-only faces carry `0123456789.:%-` and a space, which keeps the tables
small. Total font data is about 8 KB.

The degree sign and the half-height `%` are drawn rather than set: the fonts
are ASCII-only, and a degree scaled to a 56px face reads as a letter O.

## Pinout

From LilyGO's `boards.h`, `LILYGO_T5_V213` block. The panel is on SPI3 (VSPI),
where 18 and 23 are IOMUX pins.

| Signal    | GPIO |
|-----------|------|
| EPD MOSI  | 23   |
| EPD SCLK  | 18   |
| EPD CS    | 5    |
| EPD DC    | 17   |
| EPD RST   | 16   |
| EPD BUSY  | 4    |
| Green LED | 19   |

The V2.2 board uses a different mapping (DC 19, RST 12) - check the silkscreen.

### LEDs

The green LED on GPIO19 blinks with the boot progress bar and goes dark once
data is on screen. It is **active high**, despite `boards.h` declaring
`LED_ON` as LOW for this board.

The red LED beside the battery connector is the TP4054 charger's status output
(schematic sheet 1, via a UMH3N transistor). It is not wired to any GPIO, so
firmware cannot turn it off - only lifting the LED or its resistor will.

## Configuration

`idf.py menuconfig`, under "E-paper dashboard":

| Option                      | Default            | Meaning                          |
|-----------------------------|--------------------|----------------------------------|
| `DASH_WIFI_SSID` / `_PASSWORD` | -               | must be the sensors' LAN         |
| `DASH_SENSOR_HOSTS`         | the three units    | discovery fallback, see below    |
| `DASH_WEATHER_LAT` / `_LON` | 50.0006 / 19.9167  | Borek Falecki, Krakow            |
| `DASH_WEATHER_PLACE`        | `Krakow, Borek`    | unused since the title is Outdoor|
| `DASH_SCREEN_SECONDS`       | 12                 | dwell per screen                 |
| `DASH_REFRESH_SECONDS`      | 120                | sensor poll interval             |
| `DASH_FULL_REFRESH_EVERY`   | 6                  | full refresh every N screens     |

Credentials live in `sdkconfig`, which is gitignored. Do not delete that file
to force a Kconfig regeneration - use `idf.py reconfigure`, which picks up new
symbols while keeping existing values.

## Data sources

Sensors are found by browsing `_mazyiot._tcp` and read from
`http://<host>:8080/api/values`. See `esp32/home/mazy-iot-sensor`.

**Service browsing does not work on every network.** On this LAN the ESP32's
PTR queries come back empty while plain `.local` resolution works fine - the
access point drops the multicast that browsing depends on. When the browse
finds nothing, the hostnames in `DASH_SENSOR_HOSTS` are resolved instead. The
cost is that a new sensor has to be added to that list by hand. If you want
real discovery back, look for an IGMP snooping or multicast setting on the AP.

Weather and air quality come from Open-Meteo over HTTPS using the IDF
certificate bundle. No API key. Both are polled far less often than the
sensors (15 and 30 minutes) because Open-Meteo only recomputes hourly.

A failed fetch keeps the previous values rather than blanking a screen; values
are only dropped after ten minutes of silence.

## Build and flash

    . ~/esp/5.5.1/export.sh
    export PATH="$HOME/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin:$PATH"
    idf.py set-target esp32
    idf.py -p /dev/cu.SLAB_USBtoUART flash monitor

The manual PATH line is only needed because this machine has ESP-IDF installed
for RISC-V targets; the xtensa toolchain is present but `export.sh` does not
put it on PATH. `idf.py install esp32` fixes that permanently.

The T5 V2.3 has an onboard CP2102, so it enumerates as `/dev/cu.SLAB_USBtoUART`.

## Partitions and OTA

Dual-slot from the start, so this board never needs the serial conversion the
sensors did:

    nvs       0x9000   0x6000
    otadata   0xf000   0x2000
    phy_init  0x11000  0x1000
    ota_0     0x20000  0x180000
    ota_1     0x1a0000 0x180000

The app is about 1.05 MB in a 1.5 MB slot. Rollback is enabled and the image
marks itself valid once it has booted, joined WiFi and fetched data.

There is no upload endpoint yet - the slots exist, but updates are still over
USB. Adding a `/api/update` handler like the sensors' would finish it.

## Panel notes

The framebuffer is the panel's native 122 x 250 layout, 16 bytes per row, 4000
bytes total, with `1 = white` to match the controller's RAM.
`epaper_set_rotation()` remaps logical coordinates; the dashboard uses
`EPD_ROT_90` for a 250 x 122 landscape screen.

- `epaper_refresh_full()` - sequence `0xF7`, OTP waveform 1. Flashes, ~5.7 s
  measured, leaves no ghosting.
- `epaper_refresh_partial()` - sequence `0xFF`, OTP waveform 2, border held at
  `0x80`. ~0.67 s, no flashing, ghosting accumulates.

Both push to the B/W RAM (`0x24`) then copy into the previous-image RAM
(`0x26`) so the next partial update has a reference frame. Neither writes a
custom LUT, relying on the panel's OTP waveforms, which is what makes the same
code work across the variants LilyGO ships under this board name
(DEPG0213BN, GDEM0213B74, GDEH0213B72/B73).

Verified on hardware: full and partial refresh both render correctly at
`EPD_ROT_90`. If you use this on another T5 and partial refresh ghosts badly,
it is likely a B72/B73, which needs an explicit partial LUT - use
`epaper_refresh_full()` only on that variant.

## TODO

- **OTA upload endpoint.** The partition layout is ready; only the HTTP
  handler is missing.
- **WiFi manager.** Same argument as the sensors: credentials in NVS with a
  setup AP, so a wipe of `sdkconfig` cannot strand the device.
