#include <Arduino.h>


void setup() {
  Serial.begin(9600);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  pinMode(D0, OUTPUT);
}

void loop() {
  digitalWrite(D4, HIGH );
  delay(1000);
  digitalWrite(D4, LOW );
  delay(1000);
  digitalWrite(D3, HIGH );
  delay(1000);
  digitalWrite(D3, LOW);
  delay(1000);
  digitalWrite(D0, HIGH );
  delay(1000);
  digitalWrite(D0, LOW );
  delay(1000);
  digitalWrite(D3, HIGH );
  digitalWrite(D4, HIGH );
  digitalWrite(D0, HIGH );
  delay(1000);
  digitalWrite(D3, LOW );
  digitalWrite(D4, LOW );
  digitalWrite(D0, LOW );
  delay(1000);

}
