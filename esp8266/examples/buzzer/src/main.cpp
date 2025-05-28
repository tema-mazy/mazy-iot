#include <ArduinoOTA.h>

#define buzzPin    D8

#define NOTE__c    261
#define NOTE__d    294
#define NOTE__e    329
#define NOTE__f    349
#define NOTE__g    391
#define NOTE__gS   415
#define NOTE__a    440
#define NOTE__aS   455
#define NOTE__b    466
#define NOTE__cH   523
#define NOTE__cSH  554
#define NOTE__dH   587
#define NOTE__dSH  622
#define NOTE__eH   659
#define NOTE__fH   698
#define NOTE__fSH  740
#define NOTE__gH   784
#define NOTE__gSH  830
#define NOTE__aH   880

unsigned long lastMeasureSent = 0;

void beep(int f, int d) {
  tone(buzzPin, f);
  delay(d);noTone(buzzPin);delay(20);
}

void alarm() {

   beep(NOTE__a, 500);
   beep(NOTE__a, 500);
   beep(NOTE__a, 500);
   beep(NOTE__f, 350);
   beep(NOTE__cH, 150);
   beep(NOTE__a, 500);
   beep(NOTE__f, 350);
   beep(NOTE__cH, 150);
   beep(NOTE__a, 1000);
   beep(NOTE__eH, 500);
   beep(NOTE__eH, 500);
   beep(NOTE__eH, 500);
   beep(NOTE__fH, 350);
   beep(NOTE__cH, 150);
   beep(NOTE__gS, 500);
   beep(NOTE__f, 350);
   beep(NOTE__cH, 150);
   beep(NOTE__a, 1000);
   beep(NOTE__aH, 500);
   beep(NOTE__a, 350);
   beep(NOTE__a, 150);
   beep(NOTE__aH, 500);
   beep(NOTE__gSH, 250);
   beep(NOTE__gH, 250);
   beep(NOTE__fSH, 125);
   beep(NOTE__fH, 125);
   beep(NOTE__fSH, 250);
   delay(250);
   beep(NOTE__aS, 250);
   beep(NOTE__dSH, 500);
   beep(NOTE__dH, 250);
   beep(NOTE__cSH, 250);
   beep(NOTE__cH, 125);
   beep(NOTE__b, 125);
   beep(NOTE__cH, 250);
   delay(250);
   beep(NOTE__f, 125);
   beep(NOTE__gS, 500);
   beep(NOTE__f, 375);
   beep(NOTE__a, 125);
   beep(NOTE__cH, 500);
   beep(NOTE__a, 375);
   beep(NOTE__cH, 125);
   beep(NOTE__eH, 1000);
   beep(NOTE__aH, 500);
   beep(NOTE__a, 350);
   beep(NOTE__a, 150);
   beep(NOTE__aH, 500);
   beep(NOTE__gSH, 250);
   beep(NOTE__gH, 250);
   beep(NOTE__fSH, 125);
   beep(NOTE__fH, 125);
   beep(NOTE__fSH, 250);
   delay(250);
   beep(NOTE__aS, 250);
   beep(NOTE__dSH, 500);
   beep(NOTE__dH, 250);
   beep(NOTE__cSH, 250);
   beep(NOTE__cH, 125);
   beep(NOTE__b, 125);
   beep(NOTE__cH, 250);
}






void setup() {
  Serial.begin(9600);

}

void loop() {
  if (millis() - lastMeasureSent >= 30 * 1000UL || lastMeasureSent == 0) {
    alarm();
    lastMeasureSent = millis();
  }

}
