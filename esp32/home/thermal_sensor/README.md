# `HomeKit Temperature & Humidity & CO2 Sensor`

## What it does

This project reads temperature and humidity data from a SHT21 and MH-Z19b sensors and exposes the data to Apple HomeKit 
as three separate sensors: one for temperature, one for humidity and for carbon dioxide. It also includes an alarm for CO2 level above 2000 ppm.

## Key Features

- **WiFi Connectivity**: Automatically connects to your configured network and reconnects if disconnected.
- **Sensor Support**: Reads real-time data from supported DHT sensors.
- **Apple HomeKit**: Integrates directly with HomeKit, allowing you to monitor temperature and humidity from any iOS device.
- **Smart Updates**: Notifies HomeKit only when values change significantly or at regular intervals.

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
  - When temperature changes ≥ **0.5°C**
  - When humidity changes ≥ **1.0%**
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


