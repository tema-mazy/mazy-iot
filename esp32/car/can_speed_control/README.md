# can_speed_control

Speed-gated parking sensor controller for a **Suzuki Swift V**. An ESP32-C6
listens to the car's CAN bus (500 kbps, listen-only), decodes vehicle speed from
the ABS wheel-speed broadcast and drives a solid-state relay that enables the
parking sensors only at low speed:

- speed > 15 km/h → relay OFF → parking sensors disabled
- speed < 10 km/h → relay ON → parking sensors enabled
- 10-15 km/h → hysteresis dead-band, hold current state

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
- If no valid speed frame for 5 s, the relay is forced ON (fail-safe).
- Two slow boot blinks indicate startup.

## Build & flash

ESP-IDF project (target `esp32c6`), `IDF_PATH` must be set:

```sh
./build.sh           # idf.py build
./flash.sh [port]    # flash + monitor
```
