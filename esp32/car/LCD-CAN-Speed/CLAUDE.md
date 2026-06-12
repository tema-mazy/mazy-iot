# LCD-CAN-Speed

ESP32-C6 speedometer for a Suzuki Swift V: reads ABS wheel speed from the car's
CAN bus and displays km/h on the onboard 1.47" LCD. See README.md for hardware
and CAN protocol details.

## Build & flash

ESP-IDF project, target `esp32c6`. `IDF_PATH` must point at an ESP-IDF checkout.

```sh
./build.sh           # sources $IDF_PATH/export.sh, runs idf.py build
./flash.sh [port]    # flash + monitor, default port /dev/cu.usbmodem2101
```

`platformio.ini` is leftover and not the supported build path.

## Architecture

- `main/main.c` — init order matters: SD → LCD → backlight → LVGL → RGB →
  Display → Counter (CAN last, so the label exists before frames arrive).
- `main/Counter/` — TWAI driver in **listen-only** mode @ 500 kbps
  (TX GPIO20, RX GPIO19). A receive task decodes ID `0x1B8` (first wheel speed,
  big-endian bytes 0-1, `km/h = raw * 36 / 1000`, ignore `0x3FFF` = invalid)
  into the global `CPS`, and maintains `can_connected` (false after 3 s of
  silence).
- `main/Display/` — LVGL task renders `CPS` every 300 ms, or `NO CAN`.

## Constraints / gotchas

- **Keep `TWAI_MODE_LISTEN_ONLY`.** Normal mode sends ACKs; on the car bus this
  caused TX-error accumulation → bus-off → reset loops (commit f37d7f9).
- `CPS` and `can_connected` are `volatile`, written by the CAN task and read by
  the display task — keep `volatile` on both declaration and definition.
- CAN ID facts come from real log analysis (`../LCD-CAN-logger/`). `0x180` was
  previously misidentified as speed — it is throttle/load related; do not
  revert to it.
