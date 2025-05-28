#ifndef TASK_WIFI_CONNECTION
#define TASK_WIFI_CONNECTION

#include <ArduinoOTA.h>
#include "WiFi.h"
#include "../enums.h"
#include "../config.h"

extern Values _values;
extern char _deviceId[13];

void setupOTA() {

}

/**
 * Task: monitor the WiFi connection and keep it alive!
 * 
 * When a WiFi connection is established, this task will check it every 10 seconds 
 * to make sure it's still alive.
 * 
 * If not, a reconnect is attempted. If this fails to finish within the timeout,
 * the ESP32 is send to deep sleep in an attempt to recover from this.
 */
void TaskWiFiKeepAlive(void * parameter){
    char hostname[64]  = { 0 };
    uint8_t mac[6];

    Serial.println(F("[W] Config"));
    memset(hostname,0,sizeof(hostname)); 
    WiFi.macAddress(mac);
    snprintf(_deviceId, 13 , "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.print(F("[W] "));
    snprintf(hostname, 64, "MiT-%s-%s.local",_deviceId,DEVICE_NAME);
    Serial.println(hostname);

    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(hostname);  
    
    Serial.println(F("[W] Conf Done"));


    for(;;){
        if(WiFi.status() == WL_CONNECTED){
            vTaskDelay( WIFI_RECOVER_TIME_MS / portTICK_PERIOD_MS);
            continue;
        }
        Serial.println(F("[W] Connecting"));
        _values.currentState = CONNECTING_WIFI;

        WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);

        unsigned long startAttemptTime = millis();

        // Keep looping while we're not connected and haven't reached the timeout
        while (WiFi.status() != WL_CONNECTED && 
                millis() - startAttemptTime < WIFI_TIMEOUT){}

        // Make sure that we're actually connected, otherwise report failure
        if(WiFi.status() != WL_CONNECTED){
            Serial.println(F("[W] FAILED"));
            vTaskDelay(WIFI_RECOVER_TIME_MS / portTICK_PERIOD_MS);
        }
        if(WiFi.status() == WL_CONNECTED){
            Serial.print(F("[W] Connected: "));
    	    Serial.println(WiFi.localIP());
    	    _values.currentState = UP;
    	}
    }
}


#endif
