# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-C3 firmware (ESP-IDF) for a Suzuki Swift AZ parking sensor speed gate. Reads vehicle speed from the CAN bus and controls a solid-state relay that enables/disables parking sensors based on speed thresholds.

- **Target chip:** ESP32-C3 SuperMini
- **Framework:** ESP-IDF ≥ 5.0 (CMake build, not Arduino/PlatformIO)
- **CAN transceiver:** SN65HVD230 via TWAI driver

## Build & Flash

```sh
# Source ESP-IDF environment first (required every terminal session)
. $IDF_PATH/export.sh

# Configure (only needed once or when changing sdkconfig)
idf.py menuconfig

# Build
idf.py build

# Flash (adjust PORT as needed)
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor

# Flash + monitor combined
idf.py -p /dev/ttyUSB0 flash monitor
```

Manual flash command (from file `1`):
```sh
python -m esptool --chip esp32c3 -b 460800 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/canspeed.bin
```

## Architecture

All firmware is in `main/main.c` — single-file, single-task (no RTOS tasks spawned beyond `app_main`).

### Speed acquisition — dual-mode with automatic fallback

1. **Broadcast mode (primary):** Listens for CAN ID `0x0AA` (Swift AZ native broadcast). Bytes `[1..2]` encode speed as `(b[1] << 8 | b[2]) / 100` km/h at ~10 ms cadence.
2. **OBD-II fallback:** If no broadcast seen for `BROADCAST_TIMEOUT_US` (2 s), sends an OBD-II Mode 01 PID 0x0D request to `0x7DF` and reads the response from `0x7E0–0x7EF`.

### Relay / hysteresis logic

The SSR controls the +12 V brake signal line to the parking sensor controller:
- `relay ON` (GPIO0 LOW, SSR closed) → brake signal forwarded → sensors **enabled**
- `relay OFF` (GPIO0 HIGH, SSR open) → brake signal blocked → sensors **disabled**

Hysteresis band prevents relay chatter:
- Speed > `SPEED_OFF_KMH` (10) → relay OFF
- Speed < `SPEED_ON_KMH` (8)  → relay ON
- 8–10 km/h → hold current state

### Safety defaults

- On power-up: relay ON (sensors enabled) until a valid speed is received.
- If no valid speed data for `SPEED_STALE_US` (5 s): relay forced ON (fail-safe).
- After 5 consecutive CAN TX failures or OBD RX timeouts: `can_recover()` restarts the TWAI driver.

## GPIO Pin Map

| GPIO | Function |
|------|----------|
| 0    | SSR relay control (active-low) |
| 1    | CAN TXD → SN65HVD230 |
| 3    | CAN RXD ← SN65HVD230 |
| 8    | Built-in LED (mirrors relay state) |

## Key Constants to Tune

All in `main/main.c` at the top:

| Constant | Default | Purpose |
|----------|---------|---------|
| `SPEED_OFF_KMH` | 10 | Upper hysteresis threshold |
| `SPEED_ON_KMH` | 8 | Lower hysteresis threshold |
| `CAN_ID_SWIFT_SPEED` | `0x0AA` | Native broadcast CAN ID |
| `BROADCAST_TIMEOUT_US` | 2 000 000 | µs before OBD fallback |
| `SPEED_STALE_US` | 5 000 000 | µs before fail-safe relay-ON |

## Hardware Notes

See `hardware/schematic.md` for full wiring. Key points:
- CAN is tapped at OBD-II port (pin 6 = CAN-H, pin 14 = CAN-L) — do **not** add 120 Ω termination resistor unless bus is unterminated (check: ~60 Ω between CANH/CANL with ignition off = OK).
- SSR must be wired **in series** on the parking sensor controller's brake input wire only — never cut the main brake light wire.
- `platformio.ini` targets an ESP8285 (legacy/unrelated) — ignore it; the active build system is ESP-IDF/CMake.
