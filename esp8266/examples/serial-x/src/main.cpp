#include <Arduino.h>

int count = 0;


void setup() {
  Serial.begin(115200);
}

void loop() {
    Serial.println(count++);
    delay(1000);
}