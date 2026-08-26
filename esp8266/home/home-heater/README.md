# home-heater

Arduino/PlatformIO firmware for a Sonoff Basic driving a relay, with a DS18B20
on 1-Wire and a web UI.

Board: `esp8285`, `dout` flash mode, 1 MB linker script.

## Pins

From `src/main.cpp`:

| Signal       | GPIO     | Comment in source |
|--------------|----------|-------------------|
| Relay        | `GPIO12` | D6                |
| Button input | `GPIO0`  | D3                |
| MCU LED      | `GPIO13` | D7, "Sonoff led"  |
| 1-Wire bus   | `GPIO2`  | D4                |

## What the code does

- `OneWire` + `DallasTemperature` read the probe. `readSensor()` logs "No
  sensors on 1-Wire found" when the device count is zero, and retries up to
  five times while the reading is `DEVICE_DISCONNECTED_C` or `85.00`.
- `gateOn()` / `gateOff()` drive `RELAY_PIN`, each checking the current level
  first.
- `button_ISR()` toggles the relay, ignoring repeats within 300 ms.
- `ESP8266WebServer` serves `/` (`handleRoot`), `/save` (`handleSave`) and
  `/relay` (`handleRelay`), with a `onNotFound` handler.
- `EEPROM.begin(102)`, with hand-rolled four-byte float read/write helpers.
- `WiFiManager` handles WiFi configuration, so no credentials are compiled in.
- `ArduinoOTA` is initialised with a password and `begin()` is called.
- Three `Ticker` instances: sensor read, blinker, WiFi reconnect.

## Build

    pio run -e sonoffbasic

`upload-prod.sh` is present. Monitor baud is 9600.

Build flags: `-Wl,-Tesp8266.flash.1m.ld -DBE_MINIMAL
-D PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY`.

Libraries: `EEPROM`, `OneWire`, `DallasTemperature`, `WifiManager`.

## Security note

The OTA password appears as a literal in both `platformio.ini`
(`upload_flags = --auth=...`) and `src/main.cpp`
(`ArduinoOTA.setPassword(...)`), so it is in git history.
