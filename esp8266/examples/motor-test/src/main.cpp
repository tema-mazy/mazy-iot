#include <Arduino.h>
#define motorAspeed D1
#define motorAdir   D3

#define motorBspeed D2
#define motorBdir   D4

#define LED	    D5

void setup() {
  Serial.begin(9600);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW );

  Serial.println("Off");

  pinMode(motorAspeed, OUTPUT);
  digitalWrite(motorAspeed, LOW );

  pinMode(motorAdir, OUTPUT);
  digitalWrite(motorAdir, LOW );

  pinMode(motorBspeed, OUTPUT);
  digitalWrite(motorBspeed, LOW );


  pinMode(motorBdir, OUTPUT);
  digitalWrite(motorBdir, LOW );

  delay(2000);
  Serial.println("Ready");

}

void loop() {
  Serial.println("forward 100%");
  digitalWrite(LED, HIGH );

 digitalWrite(motorAdir, LOW );
 digitalWrite(motorBdir, LOW );

 analogWrite(motorAspeed,1023);
 analogWrite(motorBspeed,1023);
 delay(5000);
  Serial.println("rewerse 100%");

 digitalWrite(motorAspeed, LOW );
 digitalWrite(motorAdir, HIGH );
 digitalWrite(motorBspeed, LOW );
 digitalWrite(motorBdir, HIGH );
 analogWrite(motorAspeed,1023);
 analogWrite(motorBspeed,1023);
 delay(5000);



}
