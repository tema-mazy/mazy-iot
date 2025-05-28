#include <Arduino.h>

#define HALL_PIN D7

float calibrationFactor = 4.5;
volatile int ticks = 0, Speed = 0;

float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

void pickrpm () { ticks++; }

void setup() {
  Serial.begin(9600);
  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), pickrpm, FALLING );
  flowRate          = 0.0;
  flowMilliLitres   = 0;
  totalMilliLitres  = 0;
}

void loop() {
  ticks = 0;      // Make ticks zero before starting interrupts.
  sei();
  delay (1000);     //Wait 1 second
  cli();

    flowRate = ticks / calibrationFactor;
    flowMilliLitres = (flowRate / 60) * 1000;
    totalMilliLitres += flowMilliLitres;

    // Print the flow rate for this second in litres / minute
    Serial.print("Flow rate: ");
    Serial.print(int(flowRate));  // Print the integer part of the variable
    Serial.print("L/min");
    Serial.print("\t"); 		  // Print tab space
    // Print the cumulative total of litres flowed since starting
    Serial.print("Output Liquid Quantity: ");        
    Serial.print(totalMilliLitres);
    Serial.println("mL"); 
    Serial.print("\t"); 		  // Print tab space
    Serial.print(totalMilliLitres/1000);
    Serial.print("L");
}



