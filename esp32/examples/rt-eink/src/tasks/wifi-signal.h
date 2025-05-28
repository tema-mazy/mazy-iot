#ifndef TASK_UPDATE_WIFI_SIGNAL
#define TASK_UPDATE_WIFI_SIGNAL

#include <ArduinoOTA.h>
#include "WiFi.h"
#include "../enums.h"

extern Values _values;

/**
 * TASK: Get the current WiFi signal strength and write it to the
 * displayValues so it can be shown by the updateDisplay task
 */
void TaskWiFiUpdateSS(void * parameter){
  for(;;){
    if(WiFi.isConnected()){
        Serial.print(F("[W] Signal strength: "));
	Serial.println(WiFi.RSSI());
        _values.wifi_strength = WiFi.RSSI();
    } else {
        _values.wifi_strength = 0;
    }
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

#endif