#include <Arduino.h>
#include <SPI.h>

#define ELINK_CS 5
#define ELINK_BUSY 4
#define ELINK_RESET 16
#define ELINK_DC 17

#define SPI_MOSI 23
#define SPI_MISO -1
#define SPI_CLK 18



#include <GxEPD.h>
#include <GxGDEH0213B73/GxGDEH0213B73.h> 
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#define DEFALUT_FONT FreeMonoBold9pt7b

GxIO_Class io(SPI, ELINK_CS, ELINK_DC, ELINK_RESET);
GxEPD_Class display(io, ELINK_RESET, ELINK_BUSY);

const char HelloWorld[] = "Hello World!";

void setup()
{
  Serial.begin(115200);
  Serial.println("Boot...");
  SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, -1);
    display.init();
    display.setRotation(1);
    display.eraseDisplay();
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&DEFALUT_FONT);
    display.setTextSize(0);  
    display.setCursor(0, 50);
    display.println(HelloWorld);
    display.update();
  delay(2000); 
  Serial.println("Boot DONE");
}

void loop() {};
