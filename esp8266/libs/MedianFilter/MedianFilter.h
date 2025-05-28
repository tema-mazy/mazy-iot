/* src: https://bitbucket.org/alxarduino/leshakfilters */
#ifndef MEDIAN_FILTER_H
#define MEDIAN_FILTER_H

#include "Arduino.h"
typedef  struct Meashure {
    
    public:
      int value;
      byte ttl;
      
    const inline bool operator<(const Meashure& other) const {
        return (value<other.value);
    };
    
     const inline bool operator>(const Meashure& other) const {
        return (value<other.value);
    };
      
   
} Meashure;

class MedianFilter{
  public:
    MedianFilter(byte _frameSize);
    
    void registerValue(int measureValue);
    int getCurrentValue();
    
    int process(int measureValue);
    
    void init(int initValue);
    
  private:
    byte frameSize;
    byte medianIndex;
    boolean isReady;

    Meashure* frame;
    

    void sortFrame( int insertedIndex);
    
    void swap(int a_idx,int b_idx);
    
    void  bubleUp(int insertedIndex);
    void  bubleDown(int insertedIndex);
};


#endif
