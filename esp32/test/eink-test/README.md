# eink-test - LilyGO T5 V2.3 2.13" e-paper sample

ESP-IDF project for the LilyGO/TTGO T5 V2.3 board (ESP32-WROVER + 2.13" e-paper).
It brings up the panel over SPI with a small self-contained driver, draws a demo
screen, then updates a counter and a sweeping marker using fast partial
refreshes.

No external components or managed dependencies.

## Layout

    CMakeLists.txt
    sdkconfig.defaults      esp32 target, 4 MB flash
    main/epaper.h/.c        SSD1680 driver + framebuffer drawing primitives
    main/font5x7.h          classic 5x7 ASCII font (from Adafruit GFX, BSD)
    main/main.c             the sample

## Pinout

Taken from LilyGO's `boards.h`, `LILYGO_T5_V213` block.

| Signal    | GPIO |
|-----------|------|
| EPD MOSI  | 23   |
| EPD SCLK  | 18   |
| EPD CS    | 5    |
| EPD DC    | 17   |
| EPD RST   | 16   |
| EPD BUSY  | 4    |

The e-paper is on SPI3 (VSPI), where 18/23 are IOMUX pins. Other things on the
board, not used here: SD card on 13/14/15/2, button on 39, LED on 19,
battery ADC on 35.

Note the V2.2 board uses a different mapping (DC 19, RST 12) - check which
revision is silkscreened on yours before flashing.

## Build and flash

    . ~/esp/5.5.1/export.sh
    export PATH="$HOME/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin:$PATH"
    idf.py set-target esp32
    idf.py -p /dev/cu.SLAB_USBtoUART flash monitor

The manual PATH line is only needed because this machine has ESP-IDF installed
for RISC-V targets; the xtensa toolchain is present but `export.sh` does not put
it on PATH. `idf.py install esp32` would fix that permanently.

The T5 V2.3 has an onboard CP2102, so it enumerates as `/dev/cu.SLAB_USBtoUART`.

## Driver notes

The framebuffer is the panel's native 122 x 250 layout, 16 bytes per row,
4000 bytes total, with `1 = white` to match what the controller expects in RAM.
`epaper_set_rotation()` remaps logical coordinates; the sample uses `EPD_ROT_90`
for a 250 x 122 landscape screen.

Two refresh paths:

- `epaper_refresh_full()` - display update sequence `0xF7`, OTP waveform mode 1.
  Flashes black/white a few times, ~5.7 s measured, no ghosting afterwards.
- `epaper_refresh_partial()` - sequence `0xFF`, OTP waveform mode 2, border held
  at `0x80`. ~0.67 s measured, no flashing, but ghosting accumulates.

Both push the framebuffer to the B/W RAM (`0x24`) and then copy it into the
previous-image RAM (`0x26`), so the next partial update has a correct reference
frame. The sample does a full refresh every 20 updates to clear ghosting.

Neither path writes a custom LUT - both rely on the waveforms in the panel's
OTP. That is what makes the same code work across the panel variants LilyGO has
shipped under this board name (DEPG0213BN, GDEM0213B74, GDEH0213B72/B73). The
first three are SSD1680; the B72/B73 are SSD1675B, which shares the command set.

Verified on hardware: full and partial refresh both render correctly, and
`EPD_ROT_90` gives the expected upright landscape image. If you use this on
another T5 and the image comes out mirrored or upside down, change the rotation
in `app_main()` - the panel origin differs between revisions. If partial refresh
ghosts badly there, it is likely a B72/B73, which needs an explicit partial LUT;
use `epaper_refresh_full()` only on that variant.
