/**************************************************************************/
/*!
@file     MQ135.h
@license  GNU GPLv3
*/
/**************************************************************************/
#ifndef MQ135_H
#define MQ135_H
#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

#define NODEMCU_XFACTOR 1.0

/// The load resistance on the board
#define RLOAD 320.0
/// Calibration resistance at atmospheric CO2 level
#define RZERO 20934.62
/// Parameters for calculating ppm of CO2 from sensor resistance
#define MQ135_SCALINGFACTOR 116.6020682
#define MQ135_EXPONENT -2.769034857

#define MQ135_MAXRSRO 2.428 //for CO2
#define MQ135_MINRSRO 0.358 //for CO2

/// Parameters to model temperature and humidity dependence
#define CORA 0.00035
#define CORB 0.02718
#define CORC 1.39538
#define CORD 0.0018
#define CORE -0.003333333
#define CORF -0.001923077
#define CORG 1.130128205

/// Atmospheric CO2 level for calibration purposes
#define ATMOCO2 407.79


class MQ135 {
 public:
  MQ135();
  double getRZero(int adcraw);
  double getResistance(int adcraw);
  double getPPM(int adcraw);
  /*
  double getCorrectionFactor(float t, float h);
  double getCorrectedResistance(int adcraw,float t, float h);
  double getCorrectedRZero(int adcraw,float t, float h);
  double getCorrectedPPM(int adcraw,float t, float h);
  */
};
#endif
