#include <Homie.h>
#include <ArduinoOTA.h>

#include <adc.h>

#define MEASURE_INTERVAL 1

unsigned long lastMeasureSent = 0;



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



void loopHandler() {
  if (millis() - lastMeasureSent >= MEASURE_INTERVAL * 1000UL || lastMeasureSent == 0) {

    int raw = getRaw(10,100);

    Homie.getLogger() << "raw " << raw << endl;
    Homie.getLogger() << endl;


    lastMeasureSent = millis();
  }

}



void setupHandler() {
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
  //ArduinoOTA.handle();
  //Homie.loop();
  int raw = getRaw(10,100);
  Homie.getLogger() << "raw " << raw << endl;
  Homie.getLogger() << endl;

}
