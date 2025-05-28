#include <Arduino.h>
#include <Stepper.h>

#define a1   D1
#define a2   D3

#define b1   D2
#define b2   D4

const int stepsPerRevolution = 200; // change this to fit the number of steps per revolution // for your motor


Stepper myStepper(stepsPerRevolution, a1,a2,b1,b2);

void setup() {
// set the speed at 60 rpm:
myStepper.setSpeed(60);
// initialize the serial port:
Serial.begin(9600); }

void loop() {

// step one revolution in one direction:
Serial.println("clockwise");
myStepper.step(stepsPerRevolution);
delay(500);
// step one revolution in the other direction:
Serial.println("counterclockwise");
myStepper.step(-stepsPerRevolution);
delay(500); 
}

