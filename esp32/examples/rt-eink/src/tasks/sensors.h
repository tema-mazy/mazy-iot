#ifndef TASK_UPDATE_SENSORS
#define TASK_UPDATE_SENSORS

#include <Arduino.h>
#include "../config.h"

extern Values _values;

/**
 * Metafunction that takes care of drawing all the different
 * parts of the display (or not if it's turned off).
 */
void TaskUpdateSensors(void * parameter){
    Serial.println(F("[S] Config ..."));
    // Setup the ADC
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);
    //#ADC_0db: sets no attenuation (1V input = ADC reading of 1088).
    //#ADC_2_5db: sets an attenuation of 1.34 (1V input = ADC reading of 2086).
    //#ADC_6db: sets an attenuation of 1.5 (1V input = ADC reading of 2975).
    //#ADC_11db: sets an attenuation of 3.6 (1V input = ADC reading of 3959).

    analogReadResolution(ADC_BITS);
    pinMode(ADC_INPUT, INPUT);  
    Serial.println(F("[S] OK "));

  for (;;){
    Serial.println(F("[S] Updating..."));
    _values.p_sys=analogRead(ADC_INPUT);
    Serial.println(F("[S] Done"));
    vTaskDelay(15000 / portTICK_PERIOD_MS);

  }
}

#endif