#include <Arduino.h>


#define HALL_PIN D7
//Varibles used for calculations
volatile int ticks = 0, Speed = 0;

void pickrpm () { ticks++; }

void setup() {
  Serial.begin(9600);
  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), pickrpm, RISING );
}

void loop() {
  ticks = 0;      // Make ticks zero before starting interrupts.
  sei();
  delay (1000);     //Wait 1 second
  cli();
  Speed = ((ticks * 60)/2);
  Serial.print (Speed, DEC);
  Serial.print (" RPM\r\n");

}
