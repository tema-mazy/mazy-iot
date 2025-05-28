#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>

#include <HTU21D.h>
HTU21D            htu(HTU21D_RES_RH12_TEMP14);

float temperature, humidity, pressure, altitude;


void setup() {
  Serial.begin(9600);
  delay(100);
  bool status;

  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  status = htu.begin(D2,D1);
  if (!status) {
      Serial.println("Could not find a valid HTU21 sensor, check wiring!");
  } else {
      Serial.print(F("HTU21D sensor found. fw ver "));
      Serial.println(htu.readFirmwareVersion());

 }


}
void printValues() {
    Serial.print("Temperature = ");
    Serial.print(htu.readTemperature());
    Serial.println(" *C");

    Serial.print("Humidity = ");
    Serial.print(htu.readHumidity());
    Serial.println(" %");

    Serial.println();
}


void loop() {
    printValues();
    delay(15000);
}
