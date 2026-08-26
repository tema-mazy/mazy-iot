# mazy-test-sht21

Reads an SHT21 over I2C and prints temperature and humidity to the console
every 2 seconds. Nothing else - no WiFi, no networking.

Target: ESP32-C3.

## Wiring

From `main/main.c`:

| Signal | GPIO    |
|--------|---------|
| SDA    | `GPIO8` |
| SCL    | `GPIO9` |

I2C port 0, 100 kHz, internal pull-ups enabled, sensor address `0x40`.

## Measurement

Per cycle:

1. write `0xF5` (humidity), wait 50 ms, read 3 bytes
2. write `0xF3` (temperature), wait 50 ms, read 3 bytes

Both raw values mask off the low two status bits, then:

    humidity    = -6.00  + 125.00 * raw / 65536
    temperature = -46.85 + 175.72 * raw / 65536

## Requirements

- **idf** `>=5.0`

## Notes on the code

- No return value from the I2C calls is checked, so a disconnected sensor
  prints whatever is in the buffer rather than an error.
- The log tag is `BME280`, which does not match the sensor being read.
