#include <ArduinoOTA.h>
#include <Ticker.h>
// sensor CO2
#include <SoftwareSerial.h>
// display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>

#include <ArduinoJson.h>
#include <AsyncMqttClient.h>

#include <Adafruit_Sensor.h>

//#define SENSOR_BME
#define SENSOR_HTU

#ifdef SENSOR_BME
#include <Adafruit_BME280.h>
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;
#endif

#ifdef SENSOR_HTU
#include <HTU21D.h>
/*
HTU21D(resolution)

resolution:
HTU21D_RES_RH12_TEMP14 - RH: 12Bit, Temperature: 14Bit, by default
HTU21D_RES_RH8_TEMP12  - RH: 8Bit,  Temperature: 12Bit
HTU21D_RES_RH10_TEMP13 - RH: 10Bit, Temperature: 13Bit
HTU21D_RES_RH11_TEMP11 - RH: 11Bit, Temperature: 11Bit
*/
HTU21D            htu(HTU21D_RES_RH12_TEMP14);
#endif


#define TRIGGER_PIN D3 // @reset config on button "flash"
#define LED_ESP D4 // esp onboard GPIO2
#define LED_MCU D0 // nodemcu onboard GPIO16
#define LED_B   D0 //
#define LED_G   D7 //
#define LED_R   D8 //


#define MH_Z19_RX D5
#define MH_Z19_TX D6

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

#define INTERVAL 10.0       // read interval, s;
#define INTERVAL_DISPLAY 15.0    // change screen
#define INTERVAL_STORE 60.0  // store to gist

#define MODE_SETUP 0
#define MODE_CLIENT 1
#define MODE_STANDALONE 2
#define SCREENS 6
#define DNS_PORT 53

volatile long previousMillis = 0;
volatile int mode = 0; // 0 - setup //2 - AP ( offline ) 1 - STA( client )

volatile  int  showScreen = 0; // 0 CO2 value 1 - history gist  last- mode
volatile  int  avg_ppm  = 440;
volatile  int  alert  = 1300;
volatile  int  alert_beep  = 2000;

volatile  int  gist[SCREEN_WIDTH] = { 0 }; // uroborus
volatile  int  gistDisplay[SCREEN_WIDTH] = { 0 }; // uroborus
volatile  int  head = 0; // pointer to head
volatile  bool timeToSensor = false;
volatile  bool timeToDisplay = false;
volatile  bool bmeEnabled = false;
volatile  float temp, humi, pres;
volatile int err = 0;


char    mqtt_server[40];
char    mqtt_port[6] = "1883";
char    service_mode[2] = "2";
char    alert_level[6] = "1200";

bool shouldSaveConfig = false;

Ticker timerSensor;
Ticker timerDisplay;
Ticker timerStore;
Ticker timerMqttReconnect;
Ticker timerWifiReconnect;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 // define MH-Z19 sensor pins
SoftwareSerial co2Serial(MH_Z19_RX, MH_Z19_TX);


ESP8266WebServer server(80);
DNSServer dnsServer;
AsyncMqttClient mqttClient;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

// Network credentials
String ssid { "Mazy-IoT-CO2-" };
String pass { "cleanair" };

String mqttpath { "undefined/" };
void ftoa(float f, char *s ) {
    char s1[12];
    memset(s,0,sizeof(s));
    memset(s1,0,11);
    itoa(f, s, 10);
    strcat(s, ".");
    unsigned int i = (f - (int)f) * 10;
    itoa(i, s1, 10);
    strcat(s, s1);
}

void _iototasetup() {
  ArduinoOTA.setPassword("OTAPASS");
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
    if (error == OTA_AUTH_ERROR);          // Auth failed
    else if (error == OTA_BEGIN_ERROR);    // Begin failed
    else if (error == OTA_CONNECT_ERROR);  // Connect failed
    else if (error == OTA_RECEIVE_ERROR);  // Receive failed
    else if (error == OTA_END_ERROR);      // End failed
  });
  ArduinoOTA.begin();
}


int readCO2() {
  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  byte response[9]; // for answer
  co2Serial.write(cmd, 9); //request PPM CO2

  // The serial stream can get out of sync. The response starts with 0xff, try to resync.
  while (co2Serial.available() > 0 && (unsigned char)co2Serial.peek() != 0xFF) {
    co2Serial.read();
  }

  memset(response, 0, 9);
  co2Serial.readBytes(response, 9);

  if (response[1] != 0x86) {
    Serial.println(F("Invalid response from co2 sensor!"));
    digitalWrite(LED_R,HIGH);
    digitalWrite(LED_G,LOW);
    //digitalWrite(LED_ESP,HIGH);delay(50);digitalWrite(LED_ESP,LOW); // beeper
    err = err+1;
    return -1;
  }

  byte crc = 0;
  for (int i = 1; i < 8; i++) {
    crc += response[i];
  }
  crc = 255 - crc + 1;

  if (response[8] == crc) {
    int responseHigh = (int) response[2];
    int responseLow = (int) response[3];
    int ppm = (256 * responseHigh) + responseLow;
    return ppm;
  } else {
    Serial.println(F("CRC error!"));
    return -1;
  }
}

void prepareGist() {
    int x = 0;
    for (int i = head; i < SCREEN_WIDTH; i++) {
       x = i - head;
       gistDisplay[x] = gist[i];
    }
    for (int i = 0; i < head; i++) {
       x =  SCREEN_WIDTH - head + i;
       gistDisplay[x] = gist[i];
    }
}

boolean isIp(String str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}
String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}


void handleRoot() {
    char a[10] = "";
    String message = "<html><head>";

     message  += "<style>";
     message  += "body { font-family: Arial, Helvetica, sans-serif;  font-size: 16px; line-height: 2.0em; text-align: center;}";
     message  +=" a { color: #ccc; border: 1px solid #333; background: #336699; padding: 5px; width: 100%;}";
     message  +=".cont { max-width: 500px; text-align: left; }";
     message  +=" html, body { height: 100%; padding: 5px;}</style>";
     message  += "<meta name='viewport' content='width=device-width, initial-scale=1.0' />";
     message  += "<meta http-equiv='refresh' content='15;url=/' />";

     message  += "<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script><script type=\"text/javascript\">";
     message  += "google.charts.load('current', {'packages':['corechart']});google.charts.setOnLoadCallback(drawChart);";
     message  += "function drawChart() { var data = google.visualization.arrayToDataTable([['Time', 'ppm'],";

     for (int i = 0; i < SCREEN_WIDTH; i++) {
        message  += "['"+String((SCREEN_WIDTH-i)) +"', "+String(gistDisplay[i])+" ],";
     }
     message  += "]);";
     message  += "var options = {  title: 'CO2 Concentration', hAxis: {title: 'min',  titleTextStyle: {color: '#333'}},vAxis: {minValue: 400 } };";
     message  += "var chart = new google.visualization.AreaChart(document.getElementById('chart_div'));chart.draw(data, options); }</script>";

     message  +="</head><body><div class='cont'>";
     message  += "<h1>CO<sub>2</sub> "+String(avg_ppm)+" ppm</h1>\n";
     ftoa(temp,a);
     message  += "<h3>Temperature  "+String(a)+"°C</h3>\n";
     ftoa(humi,a);
     message  += "<h3>Humidity  "+String(a)+"%</h3>\n";
     ftoa(pres,a);
     message  += "<h3>Pressure  "+String(a)+"hPa</h3>\n";



     message  += "<br />\n";
     message  += " <div id=\"chart_div\" style=\"width: 100%; height: 500px;\"></div>\n";

     message  += "<hr /> © 2019 Mazy IoT<br/>";
     message  += "</div></body></html>\n\n";
     server.send(200, "text/html; charset=utf-8", message);
}

void drawConnectionDetails() {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.setTextColor(WHITE);
        display.println(F("Connect to WiFi:\n\n"));
        display.println("SSID: " + ssid);
        display.println("pass: " + pass+"\n");
        display.println(F("http://192.168.4.1"));
        display.display();
}

void drawWSDetails() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.setTextColor(WHITE);
    display.println(F("WiFi Connected:\n\n"));
    display.print(F("http://"));
    display.println(WiFi.localIP());
    display.display();
}

//callback notifying the need to save config
void saveConfigCallback() {
        Serial.println(F("Should save config"));
        shouldSaveConfig = true;
}
void factoryReset() {
        display.clearDisplay();
        display.setCursor(0,0);

        Serial.println(F("Resetting to factory settings"));
        display.println(F("Resetting to factory settings"));
        WiFiManager wifiManager;
        wifiManager.resetSettings();
        SPIFFS.format();
        display.println(F("HALT"));
        display.display();
        ESP.restart();
}

void configModeCallback (WiFiManager *wifiManager) {
        drawConnectionDetails();
}

void connectToWifi() {
  Serial.println(F("Connecting to Wi-Fi..."));
  WiFi.begin();
}

void connectToMqtt() {
  Serial.println(F("Connecting to MQTT..."));
  mqttClient.connect();
}

void sendMqtt(int appm, float temp, float humi, float press) {
  char a[10] = "";
  itoa(appm,a,10);
  mqttClient.publish(String(mqttpath+"ppm"). c_str(), 0, true, a);
  ftoa(temp,a);
  mqttClient.publish(String(mqttpath+"temp"). c_str(), 0, true, a);
  ftoa(humi,a);
  mqttClient.publish(String(mqttpath+"humi"). c_str(), 0, true, a);
  ftoa(pres,a);
  mqttClient.publish(String(mqttpath+"pres"). c_str(), 0, true, a);
}

void onMqttConnect(bool sessionPresent) {
  Serial.println(F("Connected to MQTT."));
  Serial.print(F("Session present: "));
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println(F("Disconnected from MQTT"));
  if (WiFi.isConnected()) {
    timerMqttReconnect.once(2, connectToMqtt);
  }
}


void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println(F("Connected to Wi-Fi"));
  connectToMqtt();

}
void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println(F("Disconnected from Wi-Fi."));
  timerMqttReconnect.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  timerWifiReconnect.once(5, connectToWifi);
}

void setupMqtt() {
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    Serial.println("mqtt@ "+String(mqtt_server)+" "+String(mqtt_port));
    mqttClient.setServer( mqtt_server , atoi(mqtt_port) );
    connectToMqtt();
}

void setupWs() {
    /* Setup the DNS server redirecting all the domains to the apIP */
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    //web server
     server.on("/", handleRoot);
     server.onNotFound([]() {
         if (!isIp(server.hostHeader()) ) {
          Serial.println(F("Request redirected to captive portal"));
          server.sendHeader(F("Location"), String("http://") + toStringIp(server.client().localIP()), true);
          server.send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
          server.client().stop(); // Stop is needed because we sent no content length
          return;
         }
       server.send(404, "text/plain", "nothing here");
     });
     server.begin();
     Serial.println(F("HTTP server started"));
}

void readConfig() {
    //read configuration from FS json
    Serial.println(F("Settings init..."));
    Serial.print(F("mounting FS..."));

    if (SPIFFS.begin()) {
      Serial.println(F("OK"));
      if (SPIFFS.exists(F("/config.json"))) {
        //file exists, reading and loading
        Serial.print(F("Reading config file ... "));
        File configFile = SPIFFS.open(F("/config.json"), "r");
        if (configFile)     {
          Serial.println("OK");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);

          configFile.readBytes(buf.get(), size);
          DynamicJsonDocument json(1024);
          auto error = deserializeJson(json, buf.get());
              if (error) {
                  Serial.print(F("deserializeJson() failed with code "));
                  Serial.println(error.c_str());
                  return;
              }
                  serializeJson(json, Serial);
                  Serial.println(" OK");
                  strcpy(mqtt_server, json["mqtt_server"]);
                  strcpy(mqtt_port, json["mqtt_port"]);
                  strcpy(service_mode, json["service_mode"]);
                  mode = atoi(service_mode);
                  strcpy(alert_level, json["alert_level"]);
                  alert = atoi(alert_level);
                  if ( alert == 0) { alert = 1200; }

       } else {
            Serial.println(F("failed!"));
       }
          configFile.close();
   } // exists
    } else {
      Serial.println(F("failed!"));
    }
    Serial.println("");
}
void setupWiFi() {
        WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
        WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
        itoa(mode,service_mode,10);
        WiFiManagerParameter custom_mode("service_mode", "service_mode", service_mode, 1);
        itoa(alert,alert_level,10);
        WiFiManagerParameter custom_alert("alert_level", "alert_level", alert_level, 6);
        WiFiManager wifiManager;
        wifiManager.setSaveConfigCallback(saveConfigCallback);
        wifiManager.setAPCallback(configModeCallback);

        //add all your parameters here
        wifiManager.addParameter(&custom_mqtt_server);
        wifiManager.addParameter(&custom_mqtt_port);
        wifiManager.addParameter(&custom_mode);
        wifiManager.addParameter(&custom_alert);

        //reset settings - for testing
        //wifiManager.resetSettings();

        wifiManager.setTimeout(120);

        //fetches ssid and pass and tries to connect
        //if it does not connect it starts an access point with the specified name
        //and goes into a blocking loop awaiting configuration
        if (!wifiManager.autoConnect(ssid.c_str(), pass.c_str())) {
          Serial.println(F("failed to connect and hit timeout"));
          delay(3000);
          //reset and try again, or maybe put it to deep sleep
          mode = MODE_STANDALONE;
          Serial.println(F("Mode =  standalone"));
        } else {
            //read updated parameters
            strcpy(mqtt_server, custom_mqtt_server.getValue());
            strcpy(mqtt_port, custom_mqtt_port.getValue());
            strcpy(service_mode, custom_mode.getValue());
            mode = atoi(service_mode);
            strcpy(alert_level, custom_alert.getValue());
            alert = atoi(alert_level);

            //save the custom parameters to FS
            if (shouldSaveConfig) {
              Serial.println(F("Saving config"));

              DynamicJsonDocument json(1024);
              json["mqtt_server"] = mqtt_server;
              json["mqtt_port"] = mqtt_port;
              json["service_mode"] = service_mode;
              json["alert_level"] = alert_level;


              File configFile = SPIFFS.open(F("/config.json"), "w");
              if (!configFile) {
                Serial.println(F("failed to open config file for writing"));
              }

              serializeJson(json, Serial);
              serializeJson(json, configFile);
              configFile.close();
          }//end save
        }
        if (mode == MODE_CLIENT) {
            Serial.println(F("WiFi connected"));
            Serial.print(F("IP address: "));
            Serial.println(WiFi.localIP());
            drawWSDetails();

            wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
            wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
            setupMqtt();

        }
}


void initDisplay() {
    Serial.println(F("Init display. "));
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
     if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
       Serial.println(F("SSD1306 allocation failed. "));
     }
    display.clearDisplay();
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(WHITE);        // Draw white text
    display.setCursor(8,0);
    display.println(F("Mazy-IoT-CO2"));
    display.println(F("Starting services"));
    display.println(F("Press and hold FLASH to clear SPIFFS"));
    display.display();
}


void time2updateDisplay() {
    timeToDisplay = true;
}

void beep() {
        digitalWrite(LED_ESP,HIGH);delay(250);digitalWrite(LED_ESP,LOW); // beeper
}

void updateDisplay() {
    char a[10] = "";
    display.clearDisplay();
    digitalWrite(LED_R,LOW); digitalWrite(LED_G,LOW);

    if ( avg_ppm > alert ) {
      Serial.println(F("WARNING! HIGH CO2"));
      display.setTextSize(1);
      display.setCursor(16,0);
      display.print(F("WARNING! HIGH CO2"));
      digitalWrite(LED_R,HIGH);
      if ( avg_ppm > alert_beep) {beep(); }
  } else {
     if (avg_ppm > 100) {
      //digitalWrite(LED_G,HIGH);
      analogWrite(LED_G,256);
      if ( avg_ppm > 800 ) { digitalWrite(LED_R,HIGH); } // yellow
  } else { digitalWrite(LED_R,HIGH); } // RED
  }

    switch (showScreen) {
        case 0:
            display.setTextSize(4);
            display.setCursor(0,16);
            // show curr value
            if (avg_ppm < 100 || avg_ppm > 5000) {
                Serial.println(F("ERR: PPM not valid"));
                display.setCursor(0,0);
                display.print(F("ERROR"));
              } else {
                display.print(" " + String(avg_ppm));
                display.setTextSize(1);
                display.setCursor(56,56);
                display.print(F("CO2 ppm"));
              }
            break;
        case 1: // show Gist
            prepareGist();
            // draw gistogram
            for (int i = 0; i < SCREEN_WIDTH; i++) {
               int y = map(gistDisplay[i],400,1000,SCREEN_HEIGHT-1,15);
               if (y < 0) { y = 0; }
               display.drawLine(i, SCREEN_HEIGHT-1, i, y, WHITE);
            }

    
            break;
        case 2: // temp. numi
                ftoa(temp,a);
                display.setTextSize(3);
                display.setCursor(0,16);
                display.print(" " + String(a)+String((char)247));
                display.setTextSize(1);
                display.setCursor(10,56);
                display.print("temperature, "+String((char)247)+"C");
        break;
        case 3: // temp. numi
                ftoa(humi,a);
                display.setTextSize(3);
                display.setCursor(0,16);
                display.print(" " + String(a)+"%");
                display.setTextSize(1);
                display.setCursor(10,56);
                display.print(F("relative humidity"));
                if (pres == 0) {
                    showScreen++; // skip next screen
                }
        break;

        case 4: //  press
                ftoa(pres,a);
                display.setTextSize(3);
                display.setCursor(0,16);
                display.print("" + String(a)+"");
                display.setTextSize(1);
                display.setCursor(10,56);
                display.print(F("pressure, hPa"));
                if (head >4) { showScreen++; } // skip config screen if > 5min uptime

        break;


        case 5: // setup

            if (mode == MODE_CLIENT || mode == MODE_SETUP) {
                drawWSDetails();
            } else {
                display.setCursor(0,8);
                display.setTextSize(1);
                display.println(F("Mazy-IoT-CO2 sensor"));
                display.println(F("Standalone mode."));
                display.display();
                delay(2000);
                drawConnectionDetails();

            }
            break;
    }

    showScreen++;
    if (showScreen >= SCREENS) {showScreen = 0;}
    display.display();
    timeToDisplay = false;
}
void storeGist() {
    gist[head++] = avg_ppm;
    if (head >= SCREEN_WIDTH ) {
        head = 0;
    }
}
void time2readSensor() {
    timeToSensor = true;
}
void readSensor() {
    digitalWrite(LED_B,HIGH);
    int ppm = readCO2();
    avg_ppm = (ppm+avg_ppm)/2;
    Serial.print(F("raw PPM = "));
    Serial.print(String(ppm));
    Serial.print(F(" Avg PPM = "));
    Serial.println(String(avg_ppm));

#ifdef SENSOR_BME
    temp = bme.readTemperature();
    humi = bme.readHumidity();
    pres = round((bme.readPressure()+1570))/100.0F;
#endif
#ifdef SENSOR_HTU
    humi = htu.readHumidity();
    temp = htu.readTemperature();
    pres = 0;
#endif

    Serial.print(F("Temperature = "));
    Serial.print(temp);
    Serial.println(F(" *C"));

    Serial.print(F("Correctd pressure = "));
    Serial.print(pres);
    Serial.println(F(" hPa"));

    Serial.print(F("Humidity = "));
    Serial.print(humi);
    Serial.println(F(" %"));

    if (mode == MODE_CLIENT && WiFi.isConnected()) {
        sendMqtt(avg_ppm,temp,humi,pres);
    }

    timeToSensor = false;
    digitalWrite(LED_B, LOW);
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("Mazy-IoT-CO2 Boots"));
  co2Serial.begin(9600);

#ifdef SENSOR_BME
  bmeEnabled = bme.begin(0x76);
  if (!bmeEnabled) {
      Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
  } else {
      Serial.println(F("BME280 sensor found."));
  }
#endif
#ifdef SENSOR_HTU
  bmeEnabled = htu.begin(D2,D1); // SDA/SCL
  if (!bmeEnabled) {
      Serial.println(F("Could not find a valid HTU21D sensor, check wiring!"));
  } else {
      Serial.print(F("HTU21D sensor found. fw ver "));
      Serial.println(htu.readFirmwareVersion());
  }
#endif

   //(F()) saves string to flash & keeps dynamic memory free


  ssid = ssid + String(ESP.getChipId(),16);
  mqttpath = "sensor/co2-"+String(ESP.getChipId(),16)+"/";

  pinMode(TRIGGER_PIN, INPUT);
  pinMode(LED_ESP,OUTPUT);

  pinMode(LED_MCU,OUTPUT);
  pinMode(LED_R,OUTPUT);
  pinMode(LED_G,OUTPUT);
  pinMode(LED_B,OUTPUT);


  initDisplay();
  Serial.println(F("Sleep for 5 sec. Press and hold  FLASH to clear SPIFFS "));
  delay(2000);
  if ( digitalRead(TRIGGER_PIN) == LOW) { 
      factoryReset();
  }
  readConfig();
  Serial.print(F("Serving mode: "));
  Serial.print(mode);
  if (mode == MODE_CLIENT || mode == MODE_SETUP) {
      Serial.println(F(" WiFi client. Setting WiFi."));
      setupWiFi();
  } else {
      Serial.println(F(" Standalone mode"));
      String ssid = "Mazy-IoT-CO2"+String(ESP.getChipId(),16);
      Serial.println(WiFi.softAP(ssid.c_str(), pass) ? "Ready" : "Failed!");
      WiFi.persistent(true);
      Serial.println(F("Started AP"));

      IPAddress myIP = WiFi.softAPIP();
      Serial.print(F("AP IP address: "));
      Serial.println(myIP);
      Serial.println("");

      /* Setup the DNS server redirecting all the domains to the apIP */
      dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());



      display.clearDisplay();
      display.println(F(" Standalone mode"));
      display.print(F("AP IP address: "));
      display.println(myIP);
      display.display();
      delay(2000);

  }

  _iototasetup();
  setupWs();


  // timer for sensor reader
  timerSensor.attach(INTERVAL, time2readSensor);
  // timer to update display
  timerDisplay.attach(INTERVAL_DISPLAY,time2updateDisplay);
  // timer to store to gist
  timerStore.attach(INTERVAL_STORE,storeGist);

}



void loop() {
     ArduinoOTA.handle();
     dnsServer.processNextRequest();
     server.handleClient();
     if (timeToSensor) { readSensor(); }
     if (timeToDisplay) { updateDisplay(); }
     if ( err > 20) {
	ESP.restart();
     }
}
