#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
//DS18B20

#define ONE_WIRE_BUS D1 //Pin to which is attached a temperature sensor D2 - GPIO4 // D1 - GPIO5 (electrodragon - D1-GPIO4!!!)
#define ONE_WIRE_MAX_DEV 10 //The maximum number of devices

const int MEASURE_INTERVAL = 3;
unsigned long lastMeasureSent = 0;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dtsensors(&oneWire);

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void loop() {

  if (millis() - lastMeasureSent >= MEASURE_INTERVAL * 1000UL || lastMeasureSent == 0) {
    // read DS18
    dtsensors.requestTemperatures();
    delay(500);
    float rawTemp = -127;

    Serial.println("Found ");
    Serial.print(dtsensors.getDeviceCount());
    Serial.println(" device(s) on bus");

  DeviceAddress deviceAddress;
  for (int i=0;i<dtsensors.getDeviceCount();i++) {
      dtsensors.getAddress(deviceAddress, i);
      Serial.print(i);
      Serial.print(" device on ");
      printAddress(deviceAddress);
      rawTemp = dtsensors.getTempC(deviceAddress);
      Serial.print(" Temp: ");
      Serial.println(rawTemp);

  }


    lastMeasureSent = millis();
  }

}


void setup() {
  // Hardware part
  dtsensors.begin();
  Serial.begin(9600);
}

