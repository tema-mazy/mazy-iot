#include <Arduino.h>
#define ledPin D6

void setup() {
  Serial.begin(9600);
  Serial.println("Off");

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW );
  delay(2000);


  Serial.println("10% Duty Cycle");
  analogWrite(ledPin,102);
  delay(2000);
 
  Serial.println("20% Duty Cycle");
  analogWrite(ledPin,205);
  delay(2000);
 
  Serial.println("50% Duty Cycle");
  analogWrite(ledPin,511);
  delay(2000);
 
  Serial.println("70% Duty Cycle");
  analogWrite(ledPin,714);
  delay(2000);
 
  Serial.println("100% Duty Cycle");
  analogWrite(ledPin,1023);
  delay(2000);

  for (int i=0; i< 1024; i++) {
    analogWrite(ledPin, i);
    delay(10);
  }
  for (int i=1023; i>-1 ; i--) {
    analogWrite(ledPin, i);
    delay(10);
  }


}

void loop() {
  // LED "breath"
  for (int i=200; i< 1000; i++) {
    analogWrite(ledPin, i);
    delay(1);
  }
  for (int i=1000; i>200 ; i--) {
    analogWrite(ledPin, i);
    delay(4);
  }


}
