# led-ws

Arduino/PlatformIO firmware driving a WS2812B/WS2811 strip with FastLED, with
a web UI for picking effects.

## Pins and strip

From `src/main.cpp`:

| Setting    | Value                                        |
|------------|----------------------------------------------|
| Data pin   | `D2` (source comments it is physically D4)   |
| LED type   | `WS2812B` or `WS2811`, per build variant     |
| `NUM_LEDS` | build flag, default 50 if undefined          |
| Frame rate | `FRAMES_PER_SECOND` 25                       |
| MCU LED    | `GPIO16` or `GPIO2`, per build variant       |

## Effects

`gPatterns` in `src/main.cpp` holds 16 entries:

    plua, confetti, sinelon, rainbow, glitter, drops, bpm,
    Fire2012WithPalette, milis, colors, candles, blendwave,
    noise8_pal, matrix_pal, serendipitous_pal, snow

`nextPattern()` advances through them. `gradient_palettes.h` supplies the
fixed palettes used by the `_pal` effects, indexed by
`gCurrentPaletteNumber`.

## Web UI and storage

`ESP8266WebServer` serves a page built from `html.h`, substituting `{fps}` and
`{svv}` and listing the pattern names. `svv` is set from a form argument as
`FRAMES_PER_SECOND - server.arg("svv")`. Settings persist via `EEPROM`.

`WiFiManager` handles WiFi setup; `ArduinoOTA` handles updates.

## Build variants

`platformio.ini` defines these environments:

| Env           | Board flag   | `NUM_LEDS` | Upload   |
|---------------|--------------|-----------|----------|
| `local`       | `NODEMCU`    | 100       | esptool  |
| `leds`        | `WEMOSD1MINI`| 300       | esptool  |
| `ledsota100`  | -            | -         | OTA      |
| `ledsota300`  | -            | -         | OTA      |
| `strip`       | -            | -         | esptool  |
| `stripota`    | -            | -         | OTA      |

Base board is `nodemcuv2`. Common flags: `-D BE_MINIMAL`,
`-D PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY`,
`-D PIO_FRAMEWORK_ARDUINO_ESPRESSIF_SDK22x_191122`, `-D BEARSSL_SSL_BASIC`.

Libraries: `fastled/FastLED`, `WifiManager`.

## Security note

`OTAKEY` is a literal in `platformio.ini` build flags, so it is in git
history.
