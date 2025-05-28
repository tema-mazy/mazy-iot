#include <Arduino.h>

uint8_t wire1 = D1;
uint8_t wire2 = D3;
uint8_t wire3 = D2;
uint8_t wire4 = D4;

const uint16_t _delay = 2; /* delay in between two steps. minimum delay more the rotational speed */

void sequence(bool a, bool b, bool c, bool d){  /* four step sequence to stepper motor */
  digitalWrite(wire1, a);
  digitalWrite(wire2, b);
  digitalWrite(wire3, c);
  digitalWrite(wire4, d);
  delay(_delay);
}

void stepCW(int steps) {
  for(int i = 0; i<steps; i++)
  {
    sequence(HIGH, LOW, LOW, LOW);
    sequence(HIGH, HIGH, LOW, LOW);
    sequence(LOW, HIGH, LOW, LOW);
    sequence(LOW, HIGH, HIGH, LOW);
    sequence(LOW, LOW, HIGH, LOW);
    sequence(LOW, LOW, HIGH, HIGH);
    sequence(LOW, LOW, LOW, HIGH);
    sequence(HIGH, LOW, LOW, HIGH);
  }
  sequence(HIGH, LOW, LOW, LOW);

}

void setup() {
  pinMode(wire1, OUTPUT); /* set four wires as output */
  pinMode(wire2, OUTPUT);
  pinMode(wire3, OUTPUT);
  pinMode(wire4, OUTPUT);
}

void loop() {
 stepCW(1024);
 delay(2000);
}