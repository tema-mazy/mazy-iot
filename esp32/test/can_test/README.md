# can_test

Sends an OBD-II vehicle-speed request on the CAN bus once a second and logs
the reply.

Target: ESP32-C6.

## CAN setup

From `main/main.c`:

| Setting | Value                    |
|---------|--------------------------|
| TX      | `GPIO1`                  |
| RX      | `GPIO3`                  |
| Bitrate | 500 kbit/s               |
| Mode    | `TWAI_MODE_NORMAL`       |
| Filter  | accept all               |

`TWAI_MODE_NORMAL` means this node transmits onto the bus.

## Request and response

Transmits on ID `0x7DF`:

    02 01 0D 00 00 00 00 00

Mode `01`, PID `0x0D` (vehicle speed).

Accepts a reply whose ID masks to `0x7E0` (`identifier & 0x7F0`), with DLC at
least 4 and `data[1] == 0x41`, `data[2] == 0x0D`. Speed in km/h is `data[3]`.

Logs "No CAN response" if nothing arrives within 1 s.

## Requirements

- **idf** `>=6.0` per `main/idf_component.yml`.

The code only uses `driver/twai.h`, so that bound is higher than the sources
require.
