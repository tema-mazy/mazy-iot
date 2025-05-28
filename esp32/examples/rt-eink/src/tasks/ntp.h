#ifndef TASK_FETCH_TIME_NTP
#define TASK_FETCH_TIME_NTP

#if NTP_TIME_SYNC_ENABLED == true
    #include <ArduinoOTA.h>
    #include <WiFi.h>
    #include <time.h>
    #include "../enums.h"

    extern Values _Values;

    void TaskTimeUpdateNTP(void * parameter){
        Serial.println(F("[NTP] Config ..."));
	configTime(3600*NTP_TZ, 3600*NTP_DST, NTP_SERVER, "0.pool.ntp.org", "1.pool.ntp.org");
        Serial.println(F("[NTP] Config done"));


        for(;;){
            if(!WiFi.isConnected()){
                vTaskDelay(60000 / portTICK_PERIOD_MS);
                continue;
            }

            Serial.println(F("[NTP] Updating..."));

	    struct tm tmstruct ;
	    tmstruct.tm_year = 0;
	    
	    getLocalTime(&tmstruct, 5000);
            Serial.print(F("[NTP] "));
	    Serial.printf("%d-%02d-%02d %02d:%02d:%02d\n",(tmstruct.tm_year)+1900,( tmstruct.tm_mon)+1, tmstruct.tm_mday,tmstruct.tm_hour , tmstruct.tm_min, tmstruct.tm_sec);
	    
	    char lc[6] = { 0 };
            snprintf(lc,6,"%02d:%02d", tmstruct.tm_hour , tmstruct.tm_min);
            
            _values.time = String(lc);
            Serial.println(F("[NTP] Done"));

            // Sleep for a minute before checking again
            vTaskDelay(NTP_UPDATE_INTERVAL_MS / portTICK_PERIOD_MS);
        }
    }
#endif
#endif