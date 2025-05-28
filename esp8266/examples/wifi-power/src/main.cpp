#include <ESP8266WiFi.h>
#include <WiFiClient.h>

//SSID of your network 
char ssid[] = "HAS";
//password of your WPA Network 
char pass[] = "smarthome33";

void setup() {
 Serial.begin(9600);
 Serial.print("connecting");
 WiFi.begin(ssid, pass);

 while (WiFi.status() != WL_CONNECTED) { 
    Serial.print(".");
    delay(500);
  } 
 Serial.println(" OK ");


}

void loop () {
  if (WiFi.status() == WL_CONNECTED) { 
  long rssi = WiFi.RSSI();
    Serial.print("RSSI:");
    Serial.println(rssi);
  } else {
     WiFi.begin(ssid, pass);
  }
  delay(1000);    
}