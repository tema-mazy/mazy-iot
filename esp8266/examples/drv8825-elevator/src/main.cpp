#include <Arduino.h>
#define pinDir   D1
#define pinStep  D2

#define pinButtUP   D6
#define pinButtDO   D5

#define maxSteps   250
#define stepDelay    2

volatile bool     dir = LOW; // UP 
volatile int  currStep = 0; 
volatile unsigned long lastpress = 0;
volatile bool     doMove = false;


void buttUP_ISR() {
  unsigned long delay = millis() - lastpress;
  if (delay >= 500 ) { // remove jitter
	lastpress = millis();
	// move UP
        dir = HIGH;
	doMove = !doMove;
        if (currStep < 0 ) currStep=0;
	if (doMove) Serial.println("Up"); else Serial.println("Stop");

  }
}
void buttDO_ISR() {
  unsigned long delay = millis() - lastpress;
  if (delay >= 500 ) { // remove jitter
	lastpress = millis();
	// move Down
        dir = LOW;
	doMove = !doMove;
        if (currStep == maxSteps ) currStep--;
	if (doMove) Serial.println("Down"); else Serial.println("Stop");
  }
}


void setup() {
    Serial.begin(9600); //I can debbug through the serial port

    pinMode(pinDir, OUTPUT); // Pins are outputs
    pinMode(pinStep, OUTPUT);

    // attach button
    pinMode(pinButtUP, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pinButtUP), buttUP_ISR , RISING);
    // attach button
    pinMode(pinButtDO, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pinButtDO), buttDO_ISR , RISING);

}

void loop() {

    if (doMove) {
	digitalWrite(pinDir,dir); // direction 

	if (currStep >= 0 && currStep < maxSteps) {
	    Serial.print("Step ");
	    Serial.println(currStep);
	    digitalWrite(pinStep, HIGH);
	    delay(stepDelay);
	    digitalWrite(pinStep, LOW);
	    delay(stepDelay);
	    if (dir == HIGH) currStep++;
	    else currStep--;

	} else {
	    doMove = false;
	    Serial.println("End");
	}
    } else {
	delay(200);
    }
}

