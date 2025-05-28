#include <Homie.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>
//DS18B20

#define ONE_WIRE_BUS D1 //Pin to which is attached a temperature sensor D2 - GPIO4 // D1 - GPIO5 (electrodragon - D1-GPIO4!!!)
#define ONE_WIRE_MAX_DEV 10 //The maximum number of devices

const int MEASURE_INTERVAL = 3;
unsigned long lastMeasureSent = 0;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dtsensors(&oneWire);

HomieNode test("test", "test");

void setup_ota() {
  ArduinoOTA.setPassword("En8shroyk9ov");
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {
    digitalWrite(D0, LOW);
    for (int i = 0; i < 20; i++)
       {
         digitalWrite(D0, HIGH);
         delay(i * 2);
         digitalWrite(D0, LOW);
         delay(i * 2);
       }
       digitalWrite(D0, HIGH);
       ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    digitalWrite(D0, total % 1);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR);          // Auth failed
    else if (error == OTA_BEGIN_ERROR);    // Begin failed
    else if (error == OTA_CONNECT_ERROR);  // Connect failed
    else if (error == OTA_RECEIVE_ERROR);  // Receive failed
    else if (error == OTA_END_ERROR);      // End failed
  });
  ArduinoOTA.begin();
}
// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void loopHandler() {

  if (millis() - lastMeasureSent >= MEASURE_INTERVAL * 1000UL || lastMeasureSent == 0) {
    // read DS18
    dtsensors.requestTemperatures();
    delay(500);
    float rawTemp = -127;

  Homie.getLogger() << "Found " << dtsensors.getDeviceCount() << " device(s) on bus" << endl;

  DeviceAddress deviceAddress;
  for (int i=0;i<dtsensors.getDeviceCount();i++) {
      dtsensors.getAddress(deviceAddress, i);
      Homie.getLogger() << "[" << i << "] addr: ";
      printAddress(deviceAddress);
      Homie.getLogger() << endl;

      rawTemp = dtsensors.getTempC(deviceAddress);
      Homie.getLogger() <<  rawTemp << endl;

  }


    lastMeasureSent = millis();
  }

  ArduinoOTA.handle();
}








void setupHandler() {

  // Hardware part
  dtsensors.begin();
  Homie.getLogger() << "OneWire bus on pin " << ONE_WIRE_BUS << endl;

  Homie.getLogger() << "Found " << dtsensors.getDeviceCount() << " device(s) on bus" << endl;

  DeviceAddress deviceAddress;

  for (int i=0;i<dtsensors.getDeviceCount();i++) {
      dtsensors.getAddress(deviceAddress, i);
      Homie.getLogger() << "[" << i << "] addr: ";
      printAddress(deviceAddress);
      Homie.getLogger() << endl;
  }
  test.advertise("test");


}

void setup() {
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);
  Homie_setFirmware("test_node", "1.1.0");
  Homie_setBrand("Mazy IoT");
  Homie.setup();
  setup_ota();
  Serial.begin(9600);
}

void loop() {
  Homie.loop();
}
