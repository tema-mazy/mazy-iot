# LCD-CAN-logger

Receives CAN frames in listen-only mode and logs every one to the console.

Target: ESP32-C6, with an ST7789 LCD, SD slot and an addressable RGB LED.

## CAN setup

From `main/Counter/Counter.c`:

| Setting | Value                   |
|---------|-------------------------|
| TX      | `GPIO20`                |
| RX      | `GPIO19`                |
| Bitrate | 500 kbit/s              |
| Mode    | `TWAI_MODE_LISTEN_ONLY` |
| Filter  | accept all              |

Listen-only: the node does not transmit.

## Logging

Each received frame is logged as ID, DLC, eight data bytes, and the gap in
milliseconds since the previous frame:

    I (12810) CAN: ID:3D0 DLC:8  40 00 00 00 00 00 00 00  [0 ms]

The RGB LED is set green on each frame.

Two conditions are logged and turn the LED off:

- "CAN not connected" - nothing received before the initial deadline
- "CAN signal lost" - no frame for 5 s (`STALE_US`)

## Layout

    main/main.c        SD_Init, LCD_Init, LVGL_Init, RGB_Init, Display, Counter_Init
    main/Counter/      CAN receive loop and frame logging
    main/Display/      LVGL screen
    main/LCD_Driver/   ST7789
    main/LVGL_Driver/  LVGL port
    main/SD_Card/      SPI SD, mount point /sdcard
    main/RGB/          LED

## Files in this directory

- `SuzukiSwiftV-CAN-Log.txt` - 94595 lines of captured console output in the
  format above.
- `speed_graph.html` - standalone page titled "CAN 0x180 - Speed vs Time".
- `build.sh`, `flash.sh`, `mon.sh` - wrappers that require `IDF_PATH` to be
  set and source `$IDF_PATH/export.sh`.

## Requirements

- **idf** `>=4.4`
- **lvgl/lvgl** `~8.3.0`
- **espressif/led_strip** `^2.4.1`

## Notes on the code

`SD_Init()` runs and `main/SD_Card/SD_SPI.c` has file read/write helpers, but
the CAN log itself goes to the console, not to the card.
