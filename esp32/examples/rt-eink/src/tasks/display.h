#ifndef TASK_UPDATE_DISPLAY
#define TASK_UPDATE_DISPLAY

#include <Arduino.h>

#include <SPI.h>
#include "../config.h"

// TTGO T5 2.3 
#define ELINK_CS 5
#define ELINK_BUSY 4
#define ELINK_RESET 16
#define ELINK_DC 17

#define ENABLE_GxEPD2_GFX 1
#include <GxEPD2_BW.h>
#include <GxEPD2_GFX.h>

#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeMono24pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=*/ ELINK_CS , /*DC=*/ ELINK_DC, /*RST=*/ ELINK_RESET, /*BUSY=*/ ELINK_BUSY)); // GDEH0213B73

#define DEFAULT_FONT FreeMonoBold9pt7b

#define FONT9 FreeMono9pt7b
#define FONT12 FreeMono12pt7b
#define FONT18 FreeMono18pt7b
#define FONT24 FreeMono24pt7b

#define FONT9B FreeMonoBold9pt7b
#define FONT12B FreeMonoBold12pt7b
#define FONT18B FreeMonoBold18pt7b
#define FONT24B FreeMonoBold24pt7b

extern Values _values;

const char HelloWorld[] = "Mazy IoT Boot";

void drawBoot(const void* parameters) {
  //display.setRotation(0);
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 0);
  display.println();
  display.println("Mazy's IoT ");
  display.println("Booting ...");
  //display.println("");
  //display.println("");
  //display.println("");
  //display.println("");
}

void drawClear(const void* parameters) {
  display.fillScreen(GxEPD_WHITE);
}

void drawSignalStrength(){
  byte X = display.width()-21;
  const byte bar_w = 3;

  //  display.fillRect(x, y, w, h, GxEPD_BLACK);

  // Draw the four base rectangles
  for (int i=0;i<4;i++) {
    display.fillRect(X + i*(bar_w+1), 10, bar_w, bar_w-1, GxEPD_BLACK); // Bar 1
  }

  if (_values.wifi_strength <0) {

  // Draw bar 1
  if(_values.wifi_strength > -80){
    display.fillRect(X, 8, bar_w, 1, GxEPD_BLACK);
  }

  // Draw bar 2
  if(_values.wifi_strength > -70){
    display.fillRect(X+(bar_w+1), 6, bar_w, 3, GxEPD_BLACK);
  }
  
  // Draw bar 3
  if(_values.wifi_strength > -60){
    display.fillRect(X+2*(bar_w+1), 3, bar_w, 6, GxEPD_BLACK);
  }

  // Draw bar 4
  if(_values.wifi_strength >= -55){
    display.fillRect(X+3*(bar_w+1), 0, bar_w, 9, GxEPD_BLACK);
  }
  }  
}

void drawStatus(const void* parameters) {
  display.setPartialWindow(0, 0, display.width(), 40);
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 12);
  display.setFont(&FONT9B);
  Serial.println(_values.time);
  display.print(_values.time);
  display.print(" ");
  display.print((_values.b_on)?" ON":"OFF");

    drawSignalStrength();
}

void drawState(const void* parameters) {
  display.setPartialWindow(0, 20, display.width(), 40);
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 32);
  display.setFont(&FONT9B);
  display.print(_values.currentState);
}

void drawSensors(const void* parameters) {
  display.setPartialWindow(0, 30, display.width(), 60);
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 52);
  display.setFont(&FONT12B);
  display.print(_values.p_sys);
}


/**
 * Metafunction that takes care of drawing all the different
 * parts of the display (or not if it's turned off).
 */
void TaskUpdateDisplay(void * parameter){
  Serial.println(F("[D] Config ..."));
  display.init();
  display.setRotation(0);
  display.setFont(&FONT9);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.drawPaged(drawBoot,0);
  delay(1000);
  Serial.println(F("[D] OK "));


  for (;;){
    Serial.println(F("[D] Updating..."));
    display.drawPaged(drawStatus,0);
    display.drawPaged(drawState,0);
    Serial.println(F("[D] Done"));
    // Sleep for X seconds, then update display again!
    vTaskDelay(5000 / portTICK_PERIOD_MS);

  }
}

#endif