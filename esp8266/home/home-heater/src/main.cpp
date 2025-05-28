#include <ArduinoOTA.h>
#include <Ticker.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <html.h>

// SONOFF Basic
/*
Pin	Function
GPIO0	Button (inverted)
GPIO12	Relay and Red LED
GPIO13	Green LED (inverted)
GPIO2	1-Wire
*/

#ifndef D0
#define D0	     16 // D0 GPIO16
#endif
#define INPUT_PIN     0 // GPIO0  / D3
#define RELAY_PIN    12 // GPIO12 / D6
#define LED_MCU_GPIO 13 // GPIO13 / D7 Sonoff led
#define ONE_WIRE_BUS  2 // GPIO2  / D4    //Pin to which is attached a temperature sensor 


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dtsensors(&oneWire);
ESP8266WebServer server(80);

volatile unsigned long buttlastSent = 0;
volatile int errCount = 0;
volatile int currTemp = DEVICE_DISCONNECTED_C*10;
volatile int tempOn   = 185;
volatile int tempOff  = 200;


// Network credentials
String ssid { "Mazy-IoT-Thermos-" };

Ticker ttread;
Ticker blinker;
Ticker timerWifiReconnect;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;


volatile bool doRead = false;

void eeWriteInt(int pos, int val) {
  byte* p = (byte*) &val;
  EEPROM.write(pos, *p);
  EEPROM.write(pos + 1, *(p + 1));
  EEPROM.write(pos + 2, *(p + 2));
  EEPROM.write(pos + 3, *(p + 3));
  EEPROM.commit();
}

int eeReadInt(int pos) {
  int val;
  byte* p = (byte*) &val;
  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  return val;
}

void ftoa(float f, char *s ) {
    char s1[12];
    memset(s,0,9);
    memset(s1,0,11);
    itoa(f, s, 10);
    strcat(s, ".");
    unsigned int i = (f - (int)f) * 10;
    itoa(i, s1, 10);
    strcat(s, s1);
}

void setDoRead() {
    doRead = true;
}

void blink() {
  digitalWrite(LED_MCU_GPIO, !digitalRead(LED_MCU_GPIO));
}

void ledOff() {
 blinker.detach();
 digitalWrite(LED_MCU_GPIO, HIGH);
}

void storeTemp() {
    eeWriteInt(0,tempOn);
    eeWriteInt(4,tempOff);
    EEPROM.commit();
}

void _iototasetup() {
  ArduinoOTA.setPassword("En8shroyk9ov");
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {
    digitalWrite(D0, LOW);
    for (int i = 0; i < 20; i++)
       {
         digitalWrite(D0, HIGH);
         delay(i * 2);
         digitalWrite(D0, LOW);
         delay(i * 2);
       }
       digitalWrite(D0, HIGH);
       ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    digitalWrite(D0, total % 1);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}


void gateOn() {
    if (digitalRead(RELAY_PIN) == LOW) {
	digitalWrite(RELAY_PIN, HIGH);
	Serial.println(F(". Relay ON "));
    }
}
void gateOff() {
    if (digitalRead(RELAY_PIN) == HIGH) {
	digitalWrite(RELAY_PIN, LOW);
	Serial.println(F(". Relay OFF "));
    }
}

void ICACHE_RAM_ATTR button_ISR() {
  unsigned long delay = millis() - buttlastSent;
  if (delay >= 300) { // remove jitter
    if (digitalRead(RELAY_PIN) == LOW) {
	gateOn();
    } else {
	gateOff();
    }
    buttlastSent = millis();
  }
}


void readSensor() {
    float rawTemp = DEVICE_DISCONNECTED_C;

    if (dtsensors.getDeviceCount() == 0) {
        Serial.println(F("No sensors on 1-Wire found"));
    } else {
        dtsensors.requestTemperatures();
        delay(500);
      	rawTemp = dtsensors.getTempCByIndex(0);

        for (int i = 0; i < 5; i++) {
            if (rawTemp == DEVICE_DISCONNECTED_C || rawTemp == 85.00 ) {
    		dtsensors.requestTemperatures();
    		delay(500);
		rawTemp = dtsensors.getTempCByIndex(0);
            }
        }
        if (rawTemp != DEVICE_DISCONNECTED_C && rawTemp != 85.00) {
		Serial.print(F(". T:"));
		Serial.println(rawTemp);
	        currTemp = rawTemp*10;
		errCount = 0;
		blinker.detach();
	        digitalWrite(LED_MCU_GPIO, HIGH);

        } else {
	    errCount = errCount+1;
	    if (errCount > 20) {
		blinker.attach(0.1, blink);
	    }
	}
    }
}

void connectToWifi() {
  Serial.println(F("# Connecting to Wi-Fi..."));
  blinker.attach(0.5, blink);
  WiFi.begin();
}
   
void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.print(F("# Connected! IP address: "));
  Serial.println(WiFi.localIP());
  ledOff();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println(F("# Disconnected from Wi-Fi."));
  blinker.attach(0.5, blink);
  timerWifiReconnect.once(5, connectToWifi);
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.print(F("@@ Entered config mode IP:"));
  Serial.println(WiFi.softAPIP());
  blinker.attach(2.0, blink);
}


void handleRelay() {
  button_ISR();
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
  server.client().stop();
}
void handleSave() {
  blinker.attach(0.1, blink);

    String s_t0n = server.arg("on");
    String s_t0f = server.arg("off");
    tempOn = s_t0n.toInt();
    tempOff = s_t0f.toInt();
    storeTemp();

  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
  server.client().stop();
  blinker.detach();
}

void handleRoot() {
    char a[10] = "";

    bool relState = (digitalRead(RELAY_PIN) == HIGH);

    String page = FPSTR(_MIOT_HEAD);
    page += FPSTR(_MIOT_STYLE);
    page += FPSTR(_MIOT_SCRIPT);
    page += FPSTR(_MIOT_BODY);

    page.replace("{title}", String("Mazy-IoT Thermal Control"));
    if (currTemp != DEVICE_DISCONNECTED_C*10) {
	ftoa(currTemp/10.0,a);
	page.replace("{currTemp}", String(a));
    } else {
	page.replace("{currTemp}", "ERROR");
    }
    String style = "";
    if (relState) {
	style = (tempOn < tempOff) ? "style='color:red;'" : "style='color:blue;'";
    }
    page.replace("{mode}", String((tempOn < tempOff)?"<b "+style+">HEATING</b>":"<b "+style+">COOLING</b>"));
    ftoa(tempOn/10.0,a);
    page.replace("{tempOn}", String(a));
    ftoa(tempOff/10.0,a);
    page.replace("{tempOff}", String(a));
    page.replace("{millis}", String(millis()));
    page.replace("{state}", String((digitalRead(RELAY_PIN) == HIGH)?"<b style='color:red;'>On</b>":"<b style='color:blue;'>Off</b>"));
    page.replace("{signal}", String(WiFi.RSSI()));

    server.sendHeader("Content-Length", String(page.length()));
    server.send(200, "text/html; charset=utf-8", page);

}


void checkTemp() {
    if (currTemp != DEVICE_DISCONNECTED_C) {
    if (tempOn < tempOff) {
    // mode == HEATER
        if (currTemp < tempOn) {
	    gateOn();
	} else
	if (currTemp > tempOff) {
	    gateOff();
	}
    } else {
    // mode == COOLING
        if (currTemp > tempOn) {
	    gateOn();
	} else
	if (currTemp < tempOff) {
	    gateOff();
	}
    }
    }
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("# Mazy IoT WunderWaffel boot"));
  Serial.println(F("# Thermal Control module "));

  ssid = ssid + String(ESP.getChipId(),16);

  pinMode(LED_MCU_GPIO, OUTPUT);

  pinMode(INPUT_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); delay(200); digitalWrite(RELAY_PIN, LOW);


  dtsensors.begin();
  dtsensors.requestTemperatures();
  Serial.print(F("# 1-Wire devices: "));
  Serial.println(dtsensors.getDeviceCount());

  Serial.println(F("# EEPROM "));
  // EEPROM
  EEPROM.begin(102);
  delay(50);
  if (EEPROM.read(98) != 27) {   // first run
    EEPROM.write(98, 27);
    storeTemp();
  } else {
    tempOn = eeReadInt(0);
    tempOff = eeReadInt(4);
  }


  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  wifiManager.setAPCallback(configModeCallback);

  Serial.println(F("!!! Sleep for 2 sec. Press and hold  FLASH to reset settings !!!! "));
  delay(2000);

  if ( digitalRead(INPUT_PIN) == LOW) { 
        Serial.println(F("!!!! Resetting to factory settings !!! "));
	 wifiManager.resetSettings();
	 EEPROM.write(98, 0);
	 EEPROM.commit();
	 ESP.reset();
  }

  Serial.println(F("# WiFi manager start..."));
  blinker.attach(0.6, blink);
  wifiManager.autoConnect(ssid.c_str());

  if (WiFi.isConnected()) {
    Serial.print(F("# Connected! IP address: "));
    Serial.println(WiFi.localIP());
    ledOff();
  }

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  Serial.print(F("# Main mode init "));
  _iototasetup();
  ttread.attach(30.0,setDoRead);
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN), button_ISR , FALLING);

  Serial.println(F(" done."));
  
  Serial.print(F("# HTTP server .."));
    //web server
     server.on("/", handleRoot);
     server.on("/save", handleSave);
     server.on("/relay", handleRelay);
     server.onNotFound([]() {
       server.send(404, "text/plain", "nothing here");
     });
     server.begin();
   Serial.println(F(" started"));


}


void loop() {
 ArduinoOTA.handle();
 server.handleClient();
 if (doRead) {
    readSensor();
    if (currTemp != DEVICE_DISCONNECTED_C) {
    checkTemp();
    doRead = false;
     if (WiFi.isConnected()) {
        ledOff();
    }
    }

 }




}
