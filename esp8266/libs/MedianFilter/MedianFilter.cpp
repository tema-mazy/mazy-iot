/* src: https://bitbucket.org/alxarduino/leshakfilters */
#include "MedianFilter.h"

MedianFilter::MedianFilter(byte _frameSize){
      frameSize = _frameSize;
      medianIndex = (_frameSize - 1) / 2;

    isReady=false;
    
    // alloc memory for frame
    
    frame=(Meashure*) malloc(_frameSize * sizeof(Meashure));
}


void MedianFilter::init(int initValue){
  for (byte i = 0; i < frameSize; i++) {
      frame[i].value=initValue;
      frame[i].ttl=i+1;
  }

}


void MedianFilter::registerValue(int measureValue) {
    if (!isReady) {
        init(measureValue);
        isReady = true;
    }
  
  int insertedIndex=-1;
    
  // replace older value
  for (byte i = 0; i < frameSize; i++) {
      frame[i].ttl--;
      if (frame[i].ttl == 0) { // expired
          frame[i].ttl = frameSize;
          frame[i].value = measureValue;
          insertedIndex=i;
      }
  }
  
  sortFrame(insertedIndex);

}

int MedianFilter::getCurrentValue() {
    return frame[medianIndex].value;
}

void MedianFilter::sortFrame( int insertedIndex) {
   if ((insertedIndex < frameSize - 2) && frame[insertedIndex]>frame[insertedIndex + 1]  ) {
      bubleUp(insertedIndex);
  } else if (insertedIndex > 0 && frame[insertedIndex-1]>frame[insertedIndex]) {
      bubleDown(insertedIndex);
  }
}



int MedianFilter::process(int measureValue){
  registerValue(measureValue);
  return getCurrentValue();
}

void MedianFilter::swap(int a_idx,int b_idx){
    Meashure tmp;
    memcpy(&tmp,&frame[b_idx],sizeof(Meashure));
    memcpy(&frame[b_idx],&frame[a_idx],sizeof(Meashure));
    memcpy(&frame[a_idx],&tmp,sizeof(Meashure));
}

void MedianFilter::bubleUp(int insertedIndex){
  for (int i = insertedIndex; i < frameSize-1; i++) {
    if (frame[i]>frame[i+1])swap(i, i + 1);
    else return;
  }
}

void MedianFilter::bubleDown(int insertedIndex){
  for (int i = insertedIndex; i > 0; i--) {
      if (frame[i-1]>frame[i])swap(i-1, i);
      else return;
  }
}
