# HTU21D - ESP-IDF

HTU21D i2c library for ESP-IDF.
ESP-IDF template used for this project: https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2c/i2c_simple

## Overview

This example demonstrates usage of HTU21D for reading relative humidity and temperature (RH/T).

### Hardware Required

To run this example, you should have one ESP32, ESP32-S or ESP32-C based development board as well as a htu21d. The HTU21D(F) is a new digital humidity sensor with temperature output by MEAS. Setting new standards in terms of size and intelligence, it is embedded in a reflow solderable Dual Flat No leads (DFN) package with a small 3 x 3 x 0.9 mm footprint. It is easy to operate via a simple I2C command, you can read the datasheet [here](https://cdn-shop.adafruit.com/datasheets/1899_HTU21D.pdf).

#### Pin Assignment:

**Note:** The following pin assignments are used by default, you can change these in the `menuconfig` .

|                  | SDA             | SCL           |
| ---------------- | -------------- | -------------- |
| ESP I2C Master   | I2C_MASTER_SDA | I2C_MASTER_SCL |
| htu21d       | SDA            | SCL            |


For the actual default value of `I2C_MASTER_SDA` and `I2C_MASTER_SCL` see `Example Configuration` in `menuconfig`.

**Note: ** There’s no need to add an external pull-up resistors for SDA/SCL pin, because the driver will enable the internal pull-up resistors.

### Build and Flash

Enter `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

```bash
I (434) example_usage: htu21d initialization successful
I (554) example_usage: Temperature: 31.9°C
I (554) example_usage: Humidity: 86.7%
I (2674) example_usage: Temperature: 31.9°C
I (2674) example_usage: Humidity: 86.7%
I (4794) example_usage: Temperature: 31.9°C
I (4794) example_usage: Humidity: 86.7%
I (6914) example_usage: Temperature: 31.9°C
I (6914) example_usage: Humidity: 86.9%
I (9034) example_usage: Temperature: 31.9°C
I (9034) example_usage: Humidity: 86.9%
I (11154) example_usage: Temperature: 32.0°C
I (11154) example_usage: Humidity: 87.1%
I (13274) example_usage: Temperature: 32.0°C
I (13274) example_usage: Humidity: 87.2%
I (15394) example_usage: Temperature: 32.0°C
I (15394) example_usage: Humidity: 87.1%
I (17514) example_usage: Temperature: 32.0°C
I (17514) example_usage: Humidity: 87.1%
I (19634) example_usage: Temperature: 31.9°C
I (19634) example_usage: Humidity: 87.1%
I (21754) example_usage: Temperature: 31.9°C
I (21754) example_usage: Humidity: 87.1%
```