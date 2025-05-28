/*
  NTP-TZ-DST
  NetWork Time Protocol - Time Zone - Daylight Saving Time

  This example shows how to read and set time,
  and how to use NTP (set NTP0_OR_LOCAL1 to 0 below)
  or an external RTC (set NTP0_OR_LOCAL1 to 1 below)

  TZ and DST below have to be manually set
  according to your local settings.

  This example code is in the public domain.
*/

#include <ESP8266WiFi.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval

////////////////////////////////////////////////////////

#define SSID            "HAS"
#define SSIDPWD         "smarthome33"
#define TZ              2       // (utc+) TZ in hours
#define DST_MN          0      // use 60mn for summer time in some countries

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

void setup() {
  Serial.begin(9600);

  configTime(TZ_SEC, DST_SEC, "ntp.lucky.net");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, SSIDPWD);
  // don't wait, observe time changing when ntp timestamp is received

}


timeval tv;
time_t now;


void loop() {

  gettimeofday(&tv, nullptr);
  now = time(nullptr);
  static struct tm *cal;

  // human readable
  Serial.print(" ctime:(UTC+");
  Serial.print((uint32_t)(TZ * 60 + DST_MN));
  Serial.print("mn)");
  Serial.print(ctime(&now));
  Serial.println("");
  cal = localtime(&now);
  Serial.println("H:"+String(cal->tm_hour)+" M:"+String(cal->tm_min));


  // simple drifting loop
  delay(5000);
}
