# can_speed_control

ESP32-C3 firmware (ESP-IDF, target `esp32c3`): speed gate for Suzuki Swift V
parking sensors. Reads vehicle speed from the CAN bus and switches an SSR so
the sensors only work below ~15 km/h. See README.md for hardware and the CAN
protocol table.

## Build & flash

```sh
./build.sh           # sources $IDF_PATH/export.sh, runs idf.py build
./flash.sh [port]    # flash + monitor
```

`platformio.ini` targets an ESP8285 (legacy) — ignore it; the active build
system is ESP-IDF/CMake.

## Architecture

Single file, single task: everything is in `main/main.c`, running in
`app_main`'s loop (no extra RTOS tasks).

- **Speed source:** CAN ID `0x1B8` (ABS wheel speeds), bytes 0-1 big-endian,
  `km/h = raw * 36 / 1000` (raw unit 0.01 m/s). `0x3FFF` = "ABS not ready",
  treated as no-data. Found by log analysis in `../LCD-CAN-logger/`.
- **Relay logic** (`update_relay`): hysteresis — OFF above `SPEED_OFF_KMH`
  (15), ON below `SPEED_ON_KMH` (10), hold in between. GPIO0 high = SSR closed
  = sensors enabled; GPIO8 LED is active-low and mirrors the relay.
- **Fail-safe:** relay ON at boot and whenever no valid speed frame arrives
  for `SPEED_STALE_US` (5 s).
- WiFi AP + HTTP/SSE log streaming and the log ring buffer exist only as
  commented-out stubs.

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
