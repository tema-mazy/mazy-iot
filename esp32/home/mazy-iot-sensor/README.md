# `HomeKit Temperature & Humidity & CO2 Sensor`

## What it does

This project reads temperature and humidity data from a SHT21 and MH-Z19b sensors and exposes the data to Apple HomeKit
as three separate sensors: one for temperature, one for humidity and for carbon dioxide. It also includes an alarm for CO2 level above 2000 ppm.

It additionally serves the same readings as JSON on the local network, so
other devices (such as the e-paper dashboard) can consume them without going
through Apple Home. Apple publishes no controller-side API, so a HomeKit
accessory cannot be read by a third-party device - hence the local API.

## Key Features

- **WiFi Connectivity**: Automatically connects to your configured network and reconnects if disconnected.
- **Sensor Support**: Reads real-time data from supported DHT sensors.
- **Apple HomeKit**: Integrates directly with HomeKit, allowing you to monitor temperature and humidity from any iOS device.
- **Smart Updates**: Notifies HomeKit only when values change significantly or at regular intervals.
- **Local JSON API**: Same values over plain HTTP, discoverable via mDNS.
- **OTA updates**: Dual-slot firmware update over HTTP with automatic rollback.
- **Runtime room name**: One image runs on every unit; the room is set over HTTP.

---

## Wiring

| Pin             | Description             | Default GPIO |
|----------------|-------------------------|--------------|
| `I2C_SDA`      | SHT21 Data Pin          | `GPIO8`      |
| `I2C_SCL`      | SHT21 Clock Pin         | `GPIO9`      |
| `CO2_TX`       | MH-Z19b UART TX         | `GPIO5`      |
| `CO2_RX`       | MH-Z19b UART RX         | `GPIO4`      |
| `LED_GPIO`     | Onboard / External LED  | `GPIO8`      |

>Board ESP32C3 Super Mini

---

## Update Behavior

- Updates are triggered:
  - When temperature changes >= **0.5 deg C**
  - When humidity changes >= **1.0%**
  - Or every **1 minutes** (as a fallback)

---

## Requirements

- **idf version:** `>=5.0`
- **espressif/mdns version:** `1.8.0`
- **wolfssl/wolfssl version:** `5.7.6`
- **achimpieters/esp32-homekit version:** `1.0.0`
---

## HomeKit Accessories

This firmware registers as **1 HomeKit accessory** with **4 services**:

- **Temperature Sensor**
- **Humidity Sensor**
- **CO2 Sensor**
- **CO2 Alarm**

You can monitor it in the Apple Home app.

---

## Local JSON API

Served on port **8080**, kept off the port HomeKit uses so the two never
contend for a socket.

### `GET /api/values`

    {"room":"Son's room","id":"MIOT32-THC-943ab8","slot":"ota_0",
     "temp":22.5,"humidity":62.7,"co2":587,"airq":1,"alert":false,"uptime":6}

| Field      | Meaning                                        |
|------------|------------------------------------------------|
| `room`     | Room name, set at runtime (see below)          |
| `id`       | Accessory name, derived from the MAC           |
| `slot`     | OTA partition currently running                |
| `temp`     | deg C from the SHT21                           |
| `humidity` | % RH from the SHT21                            |
| `co2`      | ppm from the MH-Z19b                           |
| `airq`     | HomeKit air quality level, 0 (unknown) to 5    |
| `alert`    | true above the CO2 alert threshold             |
| `uptime`   | seconds since boot                             |

### `POST /api/room`

Room name as the raw body, max 31 bytes. Stored in NVS, so it survives
reflashing and OTA.

    curl -X POST --data-binary "Son's room" \
      http://MIOT32-THC-943ab8.local:8080/api/room

Takes effect immediately for `/api/values`, and on the next reboot for the
mDNS TXT record.

### `POST /api/update`

See OTA below.

## Discovery

Each unit advertises `_mazyiot._tcp` on port 8080 with TXT records
`room=<name>` and `path=/api/values`. Browse that service to find every
sensor on the LAN without a hardcoded list:

    dns-sd -B _mazyiot._tcp

Do not rely on a custom mDNS hostname. The HomeKit component claims the
hostname first (`MIOT32-THC-xxxxxx.local`, already unique per unit via MAC)
and any later `mdns_hostname_set()` is ignored.

---

## OTA

The partition table is dual-slot. Flash is 4MB and the app is ~1.0MB, so it
fits twice with roughly 34% headroom:

    nvs       data ota      0x9000   0x6000
    otadata   data ota      0xf000   0x2000
    phy_init  data phy      0x11000  0x1000
    ota_0     app  ota_0    0x20000  0x180000
    ota_1     app  ota_1    0x1a0000 0x180000

Update a unit by POSTing the binary:

    curl -X POST --data-binary @build/mazy-iot-sensor.bin \
      http://MIOT32-THC-943ab8.local:8080/api/update

It writes the inactive slot, reboots into it, and reports the new `slot` in
`/api/values`. Takes about 10 seconds over WiFi.

### Rollback

`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` is on. A new image is only marked
valid once it has booted, joined WiFi and started serving the API; if it
never gets that far, the bootloader reverts to the previous slot on the next
reset. A useful side effect is that a second OTA only succeeds if the running
image was marked valid - `esp_ota_set_boot_partition()` returns
`ESP_ERR_OTA_ROLLBACK_INVALID_STATE` otherwise.

### Converting a unit that predates OTA

Older units used a single `factory` partition and need one USB flash to pick
up the new table. After that they update over the air permanently.

**HomeKit pairing survives this**, because `nvs` stays at the same offset and
size (`0x9000`, `0x6000`) and `idf.py flash` writes only the bootloader,
partition table and app. Do **not** run `idf.py erase-flash` - that is the one
thing that would wipe the pairing and force removing and re-adding the
accessory in the Home app.

Back up NVS first anyway, it costs two seconds:

    python -m esptool --port <port> read_flash 0x9000 0x6000 nvs-backup.bin

Those dumps contain HomeKit pairing keys, so keep them out of git.

---

## Deploying several units

All units run the **same binary**; only the room name differs, and that lives
in NVS. Per unit:

1. `idf.py -p <port> flash`
2. Wait for it to join WiFi (about 5 seconds)
3. `curl -X POST --data-binary "<room>" http://MIOT32-THC-<id>.local:8080/api/room`

`CONFIG_ESP_ROOM_NAME` is only the factory default, used until something is
stored in NVS.

Currently deployed: Son's room, Daughter's room, Main bedroom.

---

## Configuration

WiFi credentials live in `sdkconfig` (gitignored); `sdkconfig.defaults` only
carries `"***"` placeholders. If you wipe `sdkconfig`, the credentials go with
it and the device will sit in a reconnect loop - `sdkconfig.old` is usually
the only other copy.

---

## TODO

- **WiFi manager.** Move credentials out of `sdkconfig` and into NVS, the same
  way the room name works now, with a fallback AP and a captive portal for
  first-time setup. Compiled-in credentials mean a rebuild per network and a
  lost `sdkconfig` takes the credentials with it. NVS-stored credentials also
  survive OTA, so a unit could be moved to another network without USB.
