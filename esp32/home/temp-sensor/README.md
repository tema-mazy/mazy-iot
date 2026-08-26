# HomeKit Water Temperature Sensor

## What it does

Reads a DS18B20 over 1-Wire and publishes the temperature two ways:

- to Apple HomeKit, as a single Temperature Sensor accessory
- to MQTT, on topic `sensors/beczka_water/temperature`

The HomeKit service is named "Water Temperature" - this is a probe in a water
barrel, not a room sensor. For room temperature, humidity and CO2 see
`esp32/home/mazy-iot-sensor`.

Board: ESP32-C3 Super Mini.

## Wiring

| Signal        | Description               | Default GPIO |
|---------------|---------------------------|--------------|
| `SENSOR_GPIO` | DS18B20 data (1-Wire)     | `GPIO4`      |
| `LED_GPIO`    | Onboard / external LED    | `GPIO8`      |

`SENSOR_GPIO` is a compile-time constant in `main/main.c`, not a Kconfig
option. The 1-Wire bus needs the usual pull-up to 3V3 on the data line.

`MAX_SENSORS` is 1: the bus is scanned, but only the first DS18B20 found is
registered and read.

## Behaviour

- The sensor task wakes every 10 s and takes a reading every **30 s**
  (`periodic_interval` in `main/main.c`), allowing 750 ms for the DS18B20
  conversion.
- Every reading is pushed to both HomeKit and MQTT. There is no
  change-threshold filtering.
- `-127` is treated as the "no reading" sentinel and is not published.

### Probe detection and recovery

The bus is brought up at boot but **not** scanned there, so a missing probe
cannot stop WiFi, HomeKit and MQTT from starting.

Scanning happens in the sensor task and never gives up. If no device is
registered it rescans every cycle; if a read fails, the probe is released with
`ds18b20_del_device()` and rediscovered on the next cycle. A probe that was
never connected and one unplugged later are the same case, so pulling the
sensor and plugging it back in recovers on its own.

A failed read does not restart the device. It previously did, via
`ESP_ERROR_CHECK`.

### Health signalling

This board runs headless, so probe state is reported where it can actually be
seen rather than in the log:

| Channel | Healthy         | No probe             |
|---------|-----------------|----------------------|
| LED     | off             | lit                  |
| MQTT    | `status` = `ok` | `status` = `no_sensor` |

The status topic is `sensors/beczka_water/status`, published **retained** and
only on change, so a dashboard or broker subscriber sees the current state
immediately on connect.

The LED is otherwise used for the HomeKit identify blink.

## Identity

Derived from the MAC at boot, so several units do not collide:

- accessory name: `MIOT32-MH-T-<last 3 MAC bytes>`
- serial: `MHT/<full MAC>`
- manufacturer `Mazy's Wunderwafle`, model `MIOT32/T/v1`, firmware `0.0.1`

## Configuration

`idf.py menuconfig`, under "Mazy's IOT Config":

| Option              | Default                 | Meaning                     |
|---------------------|-------------------------|-----------------------------|
| `ESP_WIFI_SSID`     | -                       | network to join             |
| `ESP_WIFI_PASSWORD` | -                       | WPA2 password               |
| `ESP_LED_GPIO`      | 8                       | identify LED                |
| `ESP_SETUP_CODE`    | `338-77-883`            | HomeKit pairing code        |
| `ESP_SETUP_ID`      | `1QJ8`                  | HomeKit setup ID            |
| `ESP_MQTT_URI`      | `mqtt://192.168.88.14`  | broker                      |

Changing the setup code or setup ID invalidates `qrcode.png` - regenerate it.

Credentials live in `sdkconfig`, which is gitignored. Do not delete that file
to force a Kconfig regeneration; use `idf.py reconfigure`, which picks up new
symbols while keeping existing values.

## Requirements

From `main/idf_component.yml`:

- **idf** `>=5.3`
- **achimpieters/esp32-homekit** `*`
- **espressif/ds18b20** `~0.1.2`
- **espressif/onewire_bus** `>=1.0.0`

`ds18b20` is pinned to the 0.1.x line deliberately: later releases pull in
`espressif/sensor_hub`, whose Kconfig references a `DS18B20_SENSOR_HUB` symbol
that does not exist, and the build fails with "Missing required kconfig option
after retry".

Builds on ESP-IDF 5.5.1. App is about 1.11 MB in the 1.46 MB single-app-large
partition, 26% free.

## HomeKit

One accessory, one functional service:

- Accessory Information
- Temperature Sensor ("Water Temperature")

## Partition table

The built-in `partitions_singleapp_large.csv` (single `factory` app, no OTA).
A local `partitions.csv` used to sit here but was never selected and was
malformed - `factory` at `0x10000` overlapped `phy_init` at `0x11000`, and it
declared `otadata` with no `ota_0`/`ota_1` to switch between. It has been
removed rather than left as a trap.

## Notes

- Readings are published unconditionally every 30 s. `mazy-iot-sensor` filters
  on change thresholds; this one does not.
- With no probe on the bus the firmware still comes up; the sensor task keeps
  scanning and publishes nothing until one appears.

## TODO

- **OTA.** The other sensors moved to dual-slot OTA and no longer need USB.
  This one cannot yet: the app is 1.11 MB and `sdkconfig` declares 2 MB of
  flash, which will not hold two slots. The C3 Super Mini usually ships 4 MB -
  confirm with `esptool flash_id` while the board is connected, and if it is
  4 MB, copy the layout and `/api/update` handler from
  `esp32/home/mazy-iot-sensor`. Note the conversion needs one serial flash and
  must keep `nvs` at `0x9000`/`0x6000` to preserve HomeKit pairing.
- **WiFi manager**, same as the other sensors: credentials into NVS instead of
  `sdkconfig`.
