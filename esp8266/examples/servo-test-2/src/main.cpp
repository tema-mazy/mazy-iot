#include <Arduino.h>
#include <Servo.h>
Servo servo;


int pos = 0;

void setup() {
  // initialize serial:
  Serial.begin(9600); //set serial monitor baud rate to match
  servo.attach(D6);
}

void loop() {
  // if there's any serial available, read it:
  while (Serial.available() > 0) {

    // look for the next valid integer in the incoming serial stream:
    int pos = Serial.parseInt();
    pos = constrain(pos, 0, 2400);
    servo.write(pos);
    Serial.print("  degrees =  "); 
    Serial.print(servo.read());
    Serial.print("\t");
    Serial.print("microseconds =  ");
    Serial.println(servo.readMicroseconds());
  }
}
