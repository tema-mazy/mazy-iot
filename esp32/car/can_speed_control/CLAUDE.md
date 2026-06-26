# can_speed_control

ESP32-C6 firmware (ESP-IDF, target `esp32c6`): speed gate for Suzuki Swift V
parking sensors. Reads vehicle speed from the CAN bus and switches an SSR so
the sensors only work at low speed. See README.md for hardware and the CAN
protocol table.

## Build & flash

```sh
./build.sh           # sources $IDF_PATH/export.sh, runs idf.py build
./flash.sh [port]    # flash + monitor
```

`platformio.ini` targets an ESP8285 (legacy) — ignore it; the active build
system is ESP-IDF/CMake.

After changing `partitions.csv`, the first flash must be over cable (run
`idf.py erase-flash` once, then `./flash.sh`); later updates can go over OTA.

## Architecture

Single file, single task: everything is in `main/main.c`, running in
`app_main`'s loop (no extra RTOS tasks).

- **Speed source:** CAN ID `0x1B8` (ABS wheel speeds), bytes 0-1 big-endian,
  `km/h = raw * 36 / 1000` (raw unit 0.01 m/s). `0x3FFF` = "ABS not ready",
  treated as no-data. Found by log analysis in `../LCD-CAN-logger/`.
- **Relay logic** (`update_relay`): hysteresis — OFF above `SPEED_OFF_KMH`
  (20), ON below `SPEED_ON_KMH` (17), hold in between. GPIO1 high = SSR closed
  = sensors enabled. Thresholds are raw ABS wheel speed, which reads ~3-4 km/h
  above the dashboard.
- **Standstill:** valid speed of 0 km/h for `SPEED_STOP_US` (3 s) forces the
  relay OFF (at a traffic light, show the map not the camera). Any non-zero
  frame resets the timer and resumes hysteresis, so slow rolling = parking =
  sensors on.
- **LED:** WS2812 on GPIO8 (via RMT/`led_strip`): red blink = CAN error,
  green = CAN OK (valid speed frames arriving), blue = relay active. Colors mix
  as RGB. Separate link LED on GPIO15 pulses 50 ms on every received CAN frame.
- **Fail-safe:** relay ON at boot and whenever no valid speed frame arrives
  for `SPEED_STALE_US` (5 s). Note 0 km/h is a *valid* frame, so standstill
  does not trip the stale fail-safe — only a truly silent bus does.
- **WiFi AP + log stream:** SoftAP (SSID `Swift`, password `CONFIG_AP_PASSWORD`
  from Kconfig — open if <8 chars) at 192.168.4.1, serving an HTML page (`/`)
  and an SSE feed (`/logs`). `esp_log_set_vprintf` mirrors every `ESP_LOG*`
  line into a 128-entry ring buffer that the SSE handler streams to clients.
- **OTA** (`ota_handler`, `POST /update`): raw `.bin` in the body →
  `esp_ota_*` into the inactive slot → `esp_restart`. The web page's OTA button
  closes the SSE stream first — the httpd runs one task and the SSE handler
  loops forever, so it must be released before another request can be served.
  Two app slots live in `partitions.csv` (`ota_0`/`ota_1`, 1.5 MB each).

## Constraints / gotchas

- **Keep `CAN_MODE = TWAI_MODE_LISTEN_ONLY` for the car.** Normal mode sends
  ACKs, which on the car bus leads to TX-error accumulation → bus-off → reset
  loop. Switch to `TWAI_MODE_NORMAL` only for bench tests with a generator
  (the second node must ACK).
- `0x180` was previously misidentified as the speed ID — it is throttle/load
  related (discrete steps). Do not revert to it.
- CAN tap is at the OBD-II port; do not add 120 Ω termination unless the bus
  measures unterminated (~60 Ω CANH–CANL with ignition off = already OK).
- SSR goes in series with the parking-sensor controller's brake input wire
  only — never cut the main brake light wire.
- Power from an ignition-switched fuse-box tap, not OBD-II pin 16 (always-on,
  would drain the battery).
