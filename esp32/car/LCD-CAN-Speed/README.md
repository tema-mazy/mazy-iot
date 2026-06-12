# LCD-CAN-Speed

Real-time speedometer for a **Suzuki Swift V**. An ESP32-C6 listens to the car's
CAN bus (500 kbps, listen-only), decodes the ABS wheel-speed broadcast and shows
the current speed in km/h on the built-in 1.47" LCD.

## Hardware

- **Board:** Waveshare ESP32-C6-LCD-1.47 (ST7789 172×320 LCD, onboard WS2812 RGB LED)
- **CAN transceiver:** external (e.g. SN65HVD230) wired to the TWAI controller
  - TX → GPIO20, RX → GPIO19
- **Car:** Suzuki Swift V, body CAN @ 500 kbps

## CAN decoding (Suzuki Swift V)

Findings from analyzing a real drive log (`../LCD-CAN-logger/SuzukiSwiftV-CAN-Log.txt`):

| ID | Content |
|----|---------|
| `0x1B8` | **ABS wheel speeds** — four big-endian `uint16` (bytes 0-1, 2-3, 4-5, 6-7), one per wheel. Unit: 0.01 m/s per count, so `km/h = raw * 0.036`. Value `0x3FFF` means "invalid / ABS not ready" (seen briefly at startup) and must be ignored. |
| `0x1E8` | Vehicle speed (bytes 0-1, big-endian) ≈ `km/h * 100`. Alternative single-value source, slightly noisier near zero. |
| `0x180` | **Not speed** — byte 3 correlates with engine load/throttle state (jumps discretely, e.g. ~13 accelerating vs ~39 coasting). |

The firmware uses `0x1B8` (first wheel) — it is smooth, drops to exactly 0 when
stopped, and disappears when the ignition is off.

## Behavior

- Speed displayed as a large number, updated ~3×/s
- RGB LED: **green** = receiving speed frames, **red** = no CAN traffic for 3 s
  (display shows `NO CAN`)

## Build & flash

Requires ESP-IDF (target `esp32c6`) with `IDF_PATH` set:

```sh
./build.sh                       # idf.py build
./flash.sh [port]                # flash + monitor, default /dev/cu.usbmodem2101
```

## Project structure

```
main/
  main.c            init sequence (SD, LCD, LVGL, RGB, display, CAN)
  Counter/          TWAI (CAN) driver setup + receive task, decodes 0x1B8 into CPS
  Display/          LVGL label showing CPS, "NO CAN" fallback
  LCD_Driver/       ST7789 panel driver + backlight PWM
  LVGL_Driver/      LVGL port (flush callbacks, tick)
  RGB/              onboard WS2812 status LED
  SD_Card/          SD over SPI (unused by the speed display itself)
```
