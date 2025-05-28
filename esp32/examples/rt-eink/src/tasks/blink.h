#ifndef TASK_BLINK
#define TASK_BLINK

extern Values _values;

/**
 * LED Blink
 */
void TaskBlink(void * parameter){
// init
    pinMode(LED, OUTPUT);
    
    for (int i=0;i<10;i++) {
	digitalWrite(LED, HIGH); 
        vTaskDelay(50 / portTICK_PERIOD_MS); 
	digitalWrite(LED, LOW);
        vTaskDelay(50 / portTICK_PERIOD_MS); 
    }


  for (;;) {
//    Serial.println(F("[L] Blink ..."));
    int onT = 0;

    if (_values.currentState == UP) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); 
        continue;
    }

    
    switch(_values.currentState) {
	case CONNECTING_WIFI:
	    onT = 500;	    
	    break;
	case CONNECTING_MQTT:
	    onT = 200;	    
	    break;
	case UP:
	    onT = 0;	    
	    break;
    }

//    Serial.print(F("[L] state: "));
//    Serial.print(_values.currentState);
//    Serial.print(F(" delay "));
//    Serial.println(onT);

    if (onT > 0 ) {
	digitalWrite(LED, HIGH); 
	vTaskDelay(onT / portTICK_PERIOD_MS); 
	digitalWrite(LED, LOW);
    }
    vTaskDelay(500 / portTICK_PERIOD_MS); 
//    Serial.println(F("[L] done ..."));
    
  }
}

#endif