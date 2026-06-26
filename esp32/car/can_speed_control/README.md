# can_speed_control

Speed-gated parking sensor controller for a **Suzuki Swift V**. An ESP32-C6
listens to the car's CAN bus (500 kbps, listen-only), decodes vehicle speed from
the ABS wheel-speed broadcast and drives a solid-state relay that enables the
parking sensors only at low speed:

- speed > 20 km/h → relay OFF → parking sensors disabled
- speed < 17 km/h → relay ON → parking sensors enabled
- 17-20 km/h → hysteresis dead-band, hold current state
- stopped (0 km/h) for 3 s → relay OFF → show the map, not the camera

Thresholds are raw ABS wheel speed, which reads ~3-4 km/h above the dashboard.

## Hardware

- **MCU:** ESP32-C6
- **CAN transceiver:** SN65HVD230 — TX GPIO20, RX GPIO14, tapped at the OBD-II
  port (pin 6 = CAN-H, pin 14 = CAN-L)
- **SSR:** DC-DC opto-isolated relay on GPIO1, in series with the parking
  sensor controller's brake-signal input (never the main brake light wire)
- **Status LED:** WS2812 on GPIO8 — red blink = CAN error, green = CAN OK,
  blue = relay active (colors mix as RGB)
- **Link LED:** GPIO15 — pulses 50 ms on every received CAN frame
- Full wiring in [`hardware/schematic.md`](hardware/schematic.md)

## CAN decoding (Suzuki Swift V)

Derived from a real drive log (`../LCD-CAN-logger/SuzukiSwiftV-CAN-Log.txt`):

| ID | Content |
|----|---------|
| `0x1B8` | **ABS wheel speeds** — four big-endian `uint16` (bytes 0-1, 2-3, 4-5, 6-7), one per wheel. Unit: 0.01 m/s per count → `km/h = raw * 0.036`. Value `0x3FFF` = "ABS not ready" (briefly at startup), ignored. |
| `0x1E8` | Vehicle speed (bytes 0-1 big-endian) ≈ `km/h * 100` — alternative source. |
| `0x180` | Not speed — byte 3 follows engine load/throttle, changes discretely. |

The firmware uses `0x1B8`, first wheel (bytes 0-1).

## Safety defaults

- On power-up the relay is ON (sensors enabled) until a valid speed arrives.
- If no valid speed frame for 5 s, the relay is forced ON (fail-safe). A 0 km/h
  reading is a valid frame, so standing still does not trip this — only a silent
  bus does.
- Two slow boot blinks indicate startup.

## Web UI & OTA

The device runs a SoftAP (SSID `Swift`) and HTTP server at `http://192.168.4.1/`:

- live log stream (SSE) of every `ESP_LOG*` line
- **OTA update:** pick a `build/canspeed.bin` and hit *OTA update* — the firmware
  writes it to the inactive slot and reboots into it. No cable needed.

Two 1.5 MB OTA app slots (`partitions.csv`). The **first** flash after changing
the partition table must be over cable (`./flash.sh`, ideally after
`idf.py erase-flash`); subsequent updates can go over WiFi.

## Build & flash

ESP-IDF project (target `esp32c6`), `IDF_PATH` must be set:

```sh
./build.sh           # idf.py build
./flash.sh [port]    # flash + monitor (cable; needed once after partition change)
```
