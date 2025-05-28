//
//
// Debounced button task
//
//

#include "../config.h"

volatile int numberOfButtonInterrupts = 0;
volatile bool lastState;
volatile uint32_t debounceTimeout = 0;

// For setting up critical sections (enableinterrupts and disableinterrupts not available)
// used to disable and interrupt interrupts

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;


// Interrupt Service Routine - Keep it short!
void IRAM_ATTR handleButtonInterrupt() {
  portENTER_CRITICAL_ISR(&mux);
  numberOfButtonInterrupts++;
  lastState = digitalRead(BUTTONPIN);
  debounceTimeout = xTaskGetTickCount(); //version of millis() that works from interrupt
  portEXIT_CRITICAL_ISR(&mux);
}

//
// RTOS Task for reading button pushes (debounced)
//

void TaskButtonRead( void * parameter)
{
  String taskMessage = "[B] Debounced ButtonRead Task running on core ";
  taskMessage = taskMessage + xPortGetCoreID();
  Serial.println(taskMessage);

  // set up button Pin
  pinMode (BUTTONPIN, INPUT);
  pinMode(BUTTONPIN, INPUT_PULLUP);  // Pull up to 3.3V on input - some buttons already have this done

  attachInterrupt(digitalPinToInterrupt(BUTTONPIN), handleButtonInterrupt, FALLING);

  uint32_t saveDebounceTimeout;
  bool saveLastState;
  int save;

  // Enter RTOS Task Loop
  while (1) {

    portENTER_CRITICAL_ISR(&mux); // so that value of numberOfButtonInterrupts,l astState are atomic - Critical Section
    save  = numberOfButtonInterrupts;
    saveDebounceTimeout = debounceTimeout;
    saveLastState  = lastState;
    portEXIT_CRITICAL_ISR(&mux); // end of Critical Section

    bool currentState = digitalRead(BUTTONPIN);

    // This is the critical IF statement
    // if Interrupt Has triggered AND Button Pin is in same state AND the debounce time has expired THEN you have the button push!
    //
    if ((save != 0) //interrupt has triggered
        && (currentState == saveLastState) // pin is still in the same state as when intr triggered
        && (millis() - saveDebounceTimeout > DEBOUNCETIME ))
    { // and it has been low for at least DEBOUNCETIME, then valid keypress
      
      if (currentState == LOW)
      {
        Serial.printf("[B] is pressed and debounced, current tick=%d\n", millis());
      }
      else
      {
        Serial.printf("[B] is released and debounced, current tick=%d\n", millis());
      }
      
      Serial.printf("[B] Interrupt Triggered %d times, current State=%u, time since last trigger %dms\n", save, currentState, millis() - saveDebounceTimeout);
      
      portENTER_CRITICAL_ISR(&mux); // can't change it unless, atomic - Critical section
      numberOfButtonInterrupts = 0; // acknowledge keypress and reset interrupt counter
      portEXIT_CRITICAL_ISR(&mux);


      vTaskDelay(10 / portTICK_PERIOD_MS);
    }

      vTaskDelay(10 / portTICK_PERIOD_MS);

  }
}

