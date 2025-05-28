#include <Arduino.h>
#include <GFX.h>
#include <SPI.h>

#define ELINK_CS 5
#define ELINK_BUSY 4
#define ELINK_RESET 16
#define ELINK_DC 17

#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h> // including both doesn't hurt
#include <Fonts/FreeMonoBold9pt7b.h>

GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=*/ ELINK_CS , /*DC=*/ ELINK_DC, /*RST=*/ ELINK_RESET, /*BUSY=*/ ELINK_BUSY)); // GDEH0213B73

#define DEFAULT_FONT FreeMonoBold9pt7b


const char HelloWorld[] = "Hello World!";

void setup()
{
  Serial.begin(115200);
  Serial.println("Boot...");
  display.init();
  display.setFont(&DEFAULT_FONT);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(HelloWorld);
  }
  while (display.nextPage());

  Serial.println("Boot DONE");
}

void loop() {};
