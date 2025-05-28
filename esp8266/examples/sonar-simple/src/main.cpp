#include <Arduino.h>
#include <Ultrasonic.h>

#define TRIGGER_PIN    D7
#define ECHO_PIN       D6

Ultrasonic ultrasonic(TRIGGER_PIN, ECHO_PIN);

void setup() {
  Serial.begin(9600);
}

void loop() {
  float cmMsec;
  long microsec = ultrasonic.timing();

  cmMsec = ultrasonic.convert(microsec, Ultrasonic::CM);

      Serial.print("CM: ");
   Serial.println(cmMsec);
  delay(1000);
}