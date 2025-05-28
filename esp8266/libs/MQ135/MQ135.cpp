/**************************************************************************/
/*!
@file     MQ135.cpp
@author   G.Krocker (Mad Frog Labs)
@license  GNU GPLv3

First version of an Arduino Library for the MQ135 gas sensor
TODO: Review the correction factor calculation. This currently relies on
the datasheet but the information there seems to be wrong.

@section  HISTORY

v1.0 - First release
*/
/**************************************************************************/

#include "MQ135.h"

/**************************************************************************/
/*!
@brief  Default constructor

@param[in] pin  The analog input pin for the readout of the sensor
*/
/**************************************************************************/

MQ135::MQ135() {
}



/**************************************************************************/
/*!
@brief  Get the resistance of the sensor, ie. the measurement value
@return The sensor resistance in kOhm
*/
/**************************************************************************/
double MQ135::getResistance(int adcraw) {
  return RLOAD*(1023.0/(double)adcraw -1.0);
}


/**************************************************************************/
/*!
@brief  Get the ppm of CO2 sensed (assuming only CO2 in the air)

@return The ppm of CO2 in the air
*/
/**************************************************************************/
double MQ135::getPPM(int adcraw) {
  return MQ135_SCALINGFACTOR * pow((getResistance(adcraw)/RZERO), MQ135_EXPONENT);
}


/**************************************************************************/
/*!
@brief  Get the resistance RZero of the sensor for calibration purposes

@return The sensor resistance RZero in kOhm
*/
/**************************************************************************/
double MQ135::getRZero(int adcraw) {
  return getResistance(adcraw) * pow((ATMOCO2/MQ135_SCALINGFACTOR), (-1.0/MQ135_EXPONENT));
}


/**************************************************************************/
/*!
@brief  Get the correction factor to correct for temperature and humidity

@param[in] t  The ambient air temperature
@param[in] h  The relative humidity

@return The calculated correction factor
*/
/**************************************************************************/
/*
double MQ135::getCorrectionFactor(float t, float h) {
    if(t < 20){
        return CORA * t * t - CORB * t + CORC - (h-33.)*CORD;
    } else {
        return CORE * t + CORF * h + CORG;
    }
}

*/
/**************************************************************************/
/*!
@brief  Get the resistance of the sensor, ie. the measurement value corrected
        for temp/hum

@param[in] t  The ambient air temperature
@param[in] h  The relative humidity

@return The corrected sensor resistance kOhm
*/
/**************************************************************************/

/*
double MQ135::getCorrectedResistance(int adcraw, float t, float h) {
  return getResistance(adcraw)/getCorrectionFactor(t, h);
}
*/

/**************************************************************************/
/*!
@brief  Get the corrected resistance RZero of the sensor for calibration
        purposes

@param[in] t  The ambient air temperature
@param[in] h  The relative humidity

@return The corrected sensor resistance RZero in kOhm
*/
/**************************************************************************/
/*
float MQ135::getCorrectedRZero(int adcraw, float t, float h) {
  return getCorrectedResistance(adcraw, t, h) * pow((ATMOCO2/PARA), (1./PARB));
}
*/
/**************************************************************************/
/*!
@brief  Get the ppm of CO2 sensed (assuming only CO2 in the air), corrected
        for temp/hum

@param[in] t  The ambient air temperature
@param[in] h  The relative humidity

@return The ppm of CO2 in the air
*/
/**************************************************************************/

/*
double MQ135::getCorrectedPPM(int adcraw,float t, float h) {
  return MQ135_SCALINGFACTOR * pow((getCorrectedResistance(adcraw, t, h)/RZERO), MQ135_EXPONENT);
}
*/
