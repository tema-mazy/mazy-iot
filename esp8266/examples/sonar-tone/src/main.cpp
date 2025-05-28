#include <Arduino.h>
#include <Ultrasonic.h>

#define TRIGGER_PIN    D6
#define ECHO_PIN       D7
#define TONE_PIN       D8

#define NUMBER_BUFFERS 3
#define BUFFER_SIZE    3

#define BUFFER_01      0
#define BUFFER_02      1
#define BUFFER_03      2

Ultrasonic ultrasonic(TRIGGER_PIN, ECHO_PIN);
bool disableSD = true;

// Only run 50 time so we can reburn the code easily.
#define CYCLES         50
size_t count = 0;


void setup() {
  Serial.begin(9600);
  /*
   * If NUMBER_BUFFERS is 2 then it must be followed by two size variables
   * one for each buffer to be created. The size variables do not need to be
   * the same value.
   *
   * Example: ultrasonic.sampleCreate(3, 20, 10, 3) is valid.
   *
   * Note: The minimum size for any buffer is 2. Using less than 2 will waist
   *       resources and the buffer will be ignored.
   */
  if(!ultrasonic.sampleCreate(NUMBER_BUFFERS, BUFFER_SIZE, BUFFER_SIZE,
      BUFFER_SIZE))
    {
    disableSD = true;
    Serial.println("Could not allocate memory.");
    }
}

void loop()  {
  float cmMsec;
  float msStdDev, cmStdDev;
  long microsec = ultrasonic.timing();

  cmMsec = ultrasonic.convert(microsec, Ultrasonic::CM);

  Serial.print("CM: ");
  Serial.println(cmMsec);

   tone(TONE_PIN, 880-2*cmMsec);

  delay(100);

}