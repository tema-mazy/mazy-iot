/** * Mazy-IoT Smart Home helper * *
*/
#include <mazy-iot.h>

String iot_ssid { "Mazy-IoT" };
String iot_pass { "" };
String iot_mqtt_serv { "" };
uint16_t iot_mqtt_port = 1883;
String iot_mqtt_path { "devices/" };
String iot_mqtt_lastwill { "i will survive!" };

Ticker timerMqttReconnect;
Ticker timerWifiReconnect;
Ticker timerWifiStat;
Ticker blinker;

AsyncMqttClient mqttClient;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

uint64_t _milliseconds;
uint32_t _lastTick;

char _deviceId[13]; 
char _lastwill[256];

char addrs[17] = { 0 };

std::vector <TopicHandler> actions;
std::unique_ptr<char[]> _mqttPayloadBuffer;

int LED_MCU = 16; // nodemcu D0 / GPIO16 onboard LED ! Inverted !


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
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });
  ArduinoOTA.begin();
}


void iotsetup(String ssid, String pass, String mqttserv, uint16_t  mqttport, String mqttpath, int led_gpio) {
    LED_MCU = led_gpio;
    iotsetup(ssid,pass,mqttserv,  mqttport, mqttpath);
}
void iotsetup(String ssid, String pass, String mqttserv, uint16_t  mqttport, String mqttpath) {
	Serial.println(F("# Mazy-IoT-WunderWaffel Boots "));
	Serial.print(F("# WifiLED at GPIO"));
	Serial.println(LED_MCU);
	pinMode(LED_MCU,OUTPUT);
	digitalWrite(LED_MCU, LOW ); 

	uint8_t mac[6];
	WiFi.macAddress(mac);
	snprintf(_deviceId,13 , "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	iot_ssid = ssid;
	iot_pass = pass;
	iot_mqtt_serv = mqttserv;
	iot_mqtt_port = mqttport;
	iot_mqtt_path = mqttpath+String(_deviceId)+"/";
	iot_mqtt_lastwill = iot_mqtt_path+"$online";

	Serial.println("@Node "+String(ESP.getChipId(),16));
	Serial.println("@SSID: "+String(iot_ssid));
	//Serial.println("Pass: "+String(iot_pass));
	Serial.println("@MQTT host: "+String(iot_mqtt_serv));
	Serial.println("@MQTT port: "+String(iot_mqtt_port));
	Serial.println("@MQTT path: "+String(iot_mqtt_path));
}

void _changeState() {
	digitalWrite(LED_MCU, !(digitalRead(LED_MCU))); 
}
void _ledOff() {
	blinker.detach();
	digitalWrite(LED_MCU, HIGH ); 
}

void _connectToMqtt() {
  Serial.println(F(". MQTT connecting..."));
  blinker.attach(0.3, _changeState);
  mqttClient.connect();
}

void _connectToWifi() {
	_ledOff();
	Serial.println(F(". Wi-Fi connecting..."));
	blinker.attach(2.0, _changeState);
	WiFi.begin();
}

void _onWifiConnect(const WiFiEventStationModeGotIP& event) {
	Serial.println(F(". Wi-Fi Connected."));
	Serial.print(F("IP address: "));
	Serial.println(WiFi.localIP());
	_ledOff();
	_connectToMqtt();

}

void _onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
	Serial.println(F("x Wi-Fi Disconnected."));
	timerMqttReconnect.detach(); 
	timerWifiReconnect.once(5, _connectToWifi);
}

void _mqttUpdateSubscription() {
  _ledOff();
  for (std::vector<TopicHandler>::iterator i = actions.begin(); i != actions.end(); ++i) {
		Serial.println(i->topic);
		mqttClient.subscribe((i->topic).c_str(), 0);
  }
}

void sendMqtt(String item, String value) {
	mqttClient.publish((iot_mqtt_path + item ).c_str(), 1, true, value.c_str());
}

void _sendStat() {
	uint32_t now = millis();
	_milliseconds += (now - _lastTick);
	_lastTick = now;

	char uptimeStr[20 + 1];
	itoa( _milliseconds / 1000ULL, uptimeStr, 10);

	sendMqtt("$online","true");
	Serial.println("* Uptime "+String(uptimeStr));

	sendMqtt(F("$stats/uptime"), uptimeStr);
	sendMqtt(F("$stats/signal"),String(WiFi.RSSI()));
}

// from Homie by Marvin Roger
bool __fillPayloadBuffer(char * topic, char * payload, const AsyncMqttClientMessageProperties& properties, size_t len, size_t index, size_t total) {
	// Reallocate Buffer everytime a new message is received
	if (_mqttPayloadBuffer == nullptr || index == 0) _mqttPayloadBuffer = std::unique_ptr<char[]>(new char[total + 1]);
	// copy payload into buffer
	memcpy(_mqttPayloadBuffer.get() + index, payload, len);
	// return if payload buffer is not complete
	if (index + len != total)
		return true;
	// terminate buffer
	_mqttPayloadBuffer.get()[total] = '\0';
	return false;
}



void _onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
	if (__fillPayloadBuffer(topic, payload, properties, len, index, total))
		return; // invalid paload
		
	String ps = String(_mqttPayloadBuffer.get());
	
	Serial.println(topic);
	Serial.println(ps);
	
	for (std::vector<TopicHandler>::iterator i = actions.begin(); i != actions.end(); ++i) {
		if ( i->topic == topic) {
			i->action(ps);
		}
	}
}


void _onMqttConnect(bool sessionPresent) {
	_ledOff();
	Serial.println(F(". MQTT connected."));
	timerWifiStat.attach(30.0,_sendStat);
	mqttClient.onMessage(_onMqttMessage);
	sendMqtt(F("$online"),"true");
	_mqttUpdateSubscription();
}

void _onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
	Serial.println(F("x MQTT Disconnected."));
	timerWifiStat.detach();
	if (WiFi.isConnected()) {
		timerMqttReconnect.once(2, _connectToMqtt);
	}
}



void _setupMqtt() {
	mqttClient.onConnect(_onMqttConnect);
	mqttClient.onDisconnect(_onMqttDisconnect);
	mqttClient.setServer( iot_mqtt_serv.c_str() , iot_mqtt_port);
	mqttClient.setWill(iot_mqtt_lastwill.c_str(), 1, true, "false");
	_connectToMqtt();
}




void iotconnect() {

  Serial.println(F("# Setting up node"));
  WiFi.hostname("MazyIoT"+String(ESP.getChipId(),16)+".local");  
  WiFi.mode(WIFI_STA);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.begin(iot_ssid.c_str(),iot_pass.c_str());

  wifiConnectHandler = WiFi.onStationModeGotIP(_onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(_onWifiDisconnect);
  
  _iototasetup();
  _setupMqtt();
  
}
   

void iotTopicActionHandler(String topic, action_p action) {
	String t = iot_mqtt_path+topic;
	TopicHandler th = { t , action };
	actions.push_back(th);
	if (mqttClient.connected()) {
		mqttClient.subscribe(t.c_str(), 0);
	}
}

void iotloop() {
	ArduinoOTA.handle();	
    if (WiFi.isConnected() && mqttClient.connected()) {
		_ledOff();
	}
}

String state(unsigned int st) {
    if ( st == HIGH ) { return "on"; }
    return "off";
}

String a2h(DeviceAddress data) {
    const char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7','8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    for ( int i = 0; i < 8; i++)   {
    addrs[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
    addrs[2 * i + 1] = hexmap[data[i] & 0x0F];
    }
    return String(addrs);
}


void ftoa(float f, char *s ) {
    char s1[12];
    memset(s,0,sizeof(s));
    if (f == -127 ) {
	strcat(s," --");
	return;
    }

    
    memset(s1,0,11);
    itoa(f, s, 10);
    strcat(s, ".");
    unsigned int i = (f - (int)f) * 10;
    memset(s1,0,11);
    itoa(i, s1, 10);
    strcat(s, s1);
}
void ftoa2(float f, char *s ) {
    char s1[12];
    memset(s,0,sizeof(s));
    if (f < 0 ) {
	strcat(s," --");
	return;
    }

    memset(s1,0,11);
    itoa(f, s, 10);
    strcat(s, ".");

    unsigned int i = (f - (int)f) * 100;
    memset(s1,0,11);
    itoa(i, s1, 10);
    if (i < 10) {
	strcat(s, "0");
    }
    strcat(s, s1);
}

bool iotConnected() {
    return WiFi.isConnected() && mqttClient.connected();
}