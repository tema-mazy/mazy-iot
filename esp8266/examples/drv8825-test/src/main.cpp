#include <Arduino.h>
#define steps D2
#define dir   D1

void setup() {
  
  pinMode(steps, OUTPUT);
  pinMode(dir, OUTPUT);
}


void loop() {
  digitalWrite(dir, HIGH);
  digitalWrite(steps, HIGH);
  delay(1000);
  digitalWrite(steps, LOW);    
  delay(1000);
}

