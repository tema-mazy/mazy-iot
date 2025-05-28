#include <mazy-iot.h>

void a1(String payload) {
	Serial.print("a1 ");
	Serial.println(payload);
	if ( payload == "on") {
	    digitalWrite(D5, LOW);
	}
	if ( payload == "off") {
	    digitalWrite(D5, HIGH);
	}
}

void setup() {
  Serial.begin(9600);
  pinMode(D5,OUTPUT);
  iotsetup("HAS","smarthome33","pi", 1883, "devices/");
  iotconnect();
  iotTopicActionHandler("top1",a1);
}


void loop() {
}
