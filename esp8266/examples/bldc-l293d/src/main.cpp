#include <Arduino.h>
/*
***** BLDC DRIVER *****
*/
#define p1 D5
#define p2 D6
#define p3 D7


volatile int wait = 10;

void setup() { 
    pinMode(p1, OUTPUT);
    pinMode(p2, OUTPUT);
    pinMode(p3, OUTPUT);
    Serial.begin(9600);
}


void step(int p1s, int p2s, int p3s) {
    digitalWrite(p1, p1s); 
    digitalWrite(p2, p2s);
    digitalWrite(p3, p2s);
    delay(wait); 
}

// the loop routine runs over and over again forever:
void loop() {

if (Serial.available()){
    int inChar = (char)Serial.read(); 
    if (inChar == '-'){
	wait -=1;
    } else{
	wait +=1;
    }
    Serial.println(wait);
}
    step(1,1,0);
    step(1,0,0);
    step(1,0,1);
    step(0,0,1);
    step(0,1,1);
    step(0,1,0);
}