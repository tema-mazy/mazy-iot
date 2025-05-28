#ifndef ADC_H_
#define ADC_H_

#define EMA_A 0.0382182

extern int EMAFilter(float alpha, int latest, int stored);
extern int getRaw(int samples, int interval);
extern int DoubleEMA(int curr, int prev);

#endif
