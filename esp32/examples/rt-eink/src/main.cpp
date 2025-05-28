#include <ArduinoOTA.h>
#include <driver/adc.h>
#include "WiFi.h"

#include <config.h>
#include <enums.h>

#include <tasks/wifi.h>
#include <tasks/wifi-signal.h>
#include <tasks/ntp.h>
#include <tasks/blink.h>
#include <tasks/display.h>
#include <tasks/sensors.h>
#include <tasks/button.h>

Values _values;
char   _deviceId[13] = { 0 };

void setup() {
  Serial.begin(115200);
  Serial.println(F("Mazy IoT Wunderwaffel"));
  Serial.println(F("Booting ... "));
  Serial.println(F("Starting Tasks ... "));

  // Wifi on CORE 0
  xTaskCreatePinnedToCore(&TaskWiFiKeepAlive ,"TaskWiFiAlive"     ,1024,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(&TaskUpdateDisplay ,"TaskUpdateDisplay" ,4096,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(&TaskUpdateSensors ,"TaskUpdateSensor"  ,4096,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(&TaskBlink         ,"TaskBlink"         ,1024,NULL,3,NULL,ARDUINO_RUNNING_CORE); // low priority

  xTaskCreate(&TaskButtonRead  ,"TaskButtonRead" ,1024,NULL,0,NULL);
  xTaskCreate(&TaskWiFiUpdateSS  ,"TaskWiFiUpdateSS" ,1024,NULL,1,NULL);
  // NTP
#if NTP_TIME_SYNC_ENABLED == true
    xTaskCreate(&TaskTimeUpdateNTP, "TaskTimeUpdateNTP", 4096,NULL,1,NULL);
#endif
  Serial.print(F("Boot done"));
}

void loop() {
  // Empty. Things are done in Tasks.
}

