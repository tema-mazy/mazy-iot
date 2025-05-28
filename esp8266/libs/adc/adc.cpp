/*
*/
#include <math.h> //include libm
#include <ArduinoOTA.h>
#include "adc.h"


int EMAFilter(float alpha, int latest, int stored){
  return round(alpha*latest) + round((1-alpha)*stored);
}

volatile int ema_ema = 0;
volatile int ema = 0;

int getRaw(int samples, int interval) {
   int i;
   int DEMA = 0;
   for (i=0;i<samples;i++){
       int sensor_value = analogRead(A0);
       ema = EMAFilter(EMA_A, sensor_value, ema);
       ema_ema = EMAFilter(EMA_A, ema, ema_ema);
       DEMA = 2*ema - ema_ema;
       if (DEMA < 0) DEMA = 0;
       delay(interval);
   }

   return DEMA;
}
int DoubleEMA(int curr, int prev) {
   int DEMA = 0;
   ema = EMAFilter(EMA_A, curr, prev);
   ema_ema = EMAFilter(EMA_A, ema, ema_ema);
   DEMA = 2*ema - ema_ema;
   if (DEMA < 0) DEMA = 0;
   return DEMA;
}
