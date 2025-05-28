#ifndef MAZYIOT_H_
#define MAZYIOT_H_

#include <ArduinoOTA.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <AsyncMqttClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <vector>

#ifndef D0 
#define D0 16
#endif
#ifndef D4
#define D4 02
#endif



typedef void (*action_p) (String payload);

typedef struct {
	String topic;
	action_p action;
} TopicHandler;


extern void iotsetup(String ssid, String pass, String mqttserv, uint16_t  mqttport, String mqttpath);
extern void iotsetup(String ssid, String pass, String mqttserv, uint16_t  mqttport, String mqttpath, int led_gpio);
extern void iotconnect();
extern void iotTopicActionHandler(String topic, action_p action);
extern void sendMqtt(String item, String value);
extern void iotloop();
String state(unsigned int st);
String a2h(DeviceAddress data);
void ftoa(float f, char *s );
void ftoa2(float f, char *s );
bool iotConnected();
#endif
