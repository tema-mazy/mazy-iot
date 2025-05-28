#include <Arduino.h>

void x(byte a) {
    a = 0x0f & a; // zero high part 0000xxxx
    Serial.print("a ");
    Serial.println(a);
    Serial.print("o: ");
    byte o = a & 8; o = o >> 3;   Serial.print(o);
    digitalWrite(D5, o );
    o = a & 4;  o = o >> 2; Serial.print(o);
    digitalWrite(D6, o);
    o = a & 2;  o = o >> 1; Serial.print(o);
    digitalWrite(D7, o);
    o = a & 1;   Serial.println(o);
    digitalWrite(D8, o );
}

void setup() {
  Serial.begin(9600);
  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);
  pinMode(D8, OUTPUT);

  digitalWrite(D1, HIGH );
  digitalWrite(D5, LOW );
  digitalWrite(D6, LOW );
  digitalWrite(D7, LOW );
  digitalWrite(D8, LOW );
}

void loop() {
    for (byte i=0; i<16; i++) {
        digitalWrite(D1, LOW );
        digitalWrite(D0, HIGH ); // light OFF
        delay(5000);
        digitalWrite(D0, LOW ); // ON
        digitalWrite(D1, HIGH ); 
        delay(5000);
    }

    //delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH );
    /*
    digitalWrite(D5, LOW );
    digitalWrite(D6, LOW );
    digitalWrite(D7, LOW );
    digitalWrite(D8, LOW );


    digitalWrite(D5, HIGH );
    digitalWrite(D6, LOW );
    digitalWrite(D7, LOW );
    digitalWrite(D8, LOW );
    delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH );

    digitalWrite(D5, LOW );
    digitalWrite(D6, HIGH );
    digitalWrite(D7, LOW );
    digitalWrite(D8, LOW );
    delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH );

    digitalWrite(D5, LOW );
    digitalWrite(D6, LOW );
    digitalWrite(D7, HIGH );
    digitalWrite(D8, LOW );
    delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH );

    digitalWrite(D5, LOW );
    digitalWrite(D6, LOW );
    digitalWrite(D7, LOW );
    digitalWrite(D8, HIGH );
    delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH );

    digitalWrite(D5, LOW );
    digitalWrite(D6, LOW );
    digitalWrite(D7, HIGH );
    digitalWrite(D8, HIGH );
    delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH );

    digitalWrite(D5, LOW );
    digitalWrite(D6, HIGH );
    digitalWrite(D7, HIGH );
    digitalWrite(D8, HIGH );
    delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH );

    digitalWrite(D5, HIGH );
    digitalWrite(D6, HIGH );
    digitalWrite(D7, HIGH );
    digitalWrite(D8, HIGH );
    delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH );

    digitalWrite(D5, HIGH );
    digitalWrite(D6, HIGH );
    digitalWrite(D7, HIGH );
    digitalWrite(D8, LOW );
    delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH );

    digitalWrite(D5, HIGH );
    digitalWrite(D6, HIGH );
    digitalWrite(D7, LOW );
    digitalWrite(D8, LOW );
    delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH );

    digitalWrite(D5, HIGH );
    digitalWrite(D6, LOW );
    digitalWrite(D7, LOW );
    digitalWrite(D8, LOW );
    delay(2000); digitalWrite(D4, LOW ); delay(500); digitalWrite(D4, HIGH ); */

}
