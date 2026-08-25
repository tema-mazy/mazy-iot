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
- The LED is used only for the HomeKit identify blink.

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

- **idf** `>=6.0`
- **achimpieters/esp32-homekit** `>=1.2.4`
- **espressif/onewire_bus** `>=1.0.0`
- **espressif/ds18b20** `>=0.1.2`

## HomeKit

One accessory, one functional service:

- Accessory Information
- Temperature Sensor ("Water Temperature")

## Known issues

- **Does not build against ESP-IDF 5.5.1**, which is what is installed on this
  machine. `idf.py build` fails with "Missing required kconfig option after
  retry", preceded by:

      WARNING: The following Kconfig variables were used in "if" clauses, but
      not found in any Kconfig file:
          DS18B20_SENSOR_HUB, introduced by espressif/sensor_hub

  Dependency resolution is pulling in `espressif/sensor_hub`, whose Kconfig
  references a symbol that does not exist. The manifest asks for `idf >=6.0`,
  so this most likely needs IDF 6.x, or the `ds18b20` dependency pinned to a
  version that does not drag in `sensor_hub`.

- **`partitions.csv` is not the table in use.** `sdkconfig` selects
  `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE`, so
  `partitions_singleapp_large.csv` is built instead. The local file is also
  malformed - `factory` starts at `0x10000` while `phy_init` sits at
  `0x11000`, inside it - and it declares an `otadata` partition with no
  `ota_0`/`ota_1` to switch between, so OTA would not work as written.

- **`init_onewire()` retries forever** if no sensor is found. The loop
  condition is `while (sensors_count == 0 || cnt > 60)`; the `cnt > 60` guard
  is inverted and never fires, so a missing or miswired probe hangs boot
  before WiFi-dependent tasks are useful.

- Readings are published unconditionally every 30 s. The sibling project
  filters on change thresholds; this one does not.
