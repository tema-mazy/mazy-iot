
MQTTconfig readMQTTConfig () {
  int eeAddress = 300;
  MQTTconfig MQTTConfig;

  for (int i = 0; i < 32; ++i) MQTTConfig.HOST[i]   = char(EEPROM.read(eeAddress + i)); eeAddress += 32;
  for (int i = 0; i < 32; ++i) MQTTConfig.USER[i]   = char(EEPROM.read(eeAddress + i)); eeAddress += 32;
  for (int i = 0; i < 32; ++i) MQTTConfig.PASSWD[i] = char(EEPROM.read(eeAddress + i)); eeAddress += 32;
  for (int i = 0; i < 10; ++i) MQTTConfig.PORT[i] = char(EEPROM.read(eeAddress + i)); 

  return MQTTConfig;
}

void writeMQTTConfig(const char HOST[32], const char USER[32], const char PASSWD[32], const char PORT[10]) {
  int eeAddress = 300;

  for (int i = 0; i < 32; ++i)  EEPROM.write(eeAddress + i, HOST[i]); eeAddress += 32;
  for (int i = 0; i < 32; ++i)  EEPROM.write(eeAddress + i, USER[i]); eeAddress += 32;
  for (int i = 0; i < 32; ++i)  EEPROM.write(eeAddress + i, PASSWD[i]); eeAddress += 32;
  for (int i = 0; i < 10; ++i)  EEPROM.write(eeAddress + i, PORT[i]); 

  EEPROM.commit();
}

int Get_EFFIDX (String effect) {
  
  if (effect.equals("Конфетти")) return 0;
  if (effect.equals("Огонь")) return 1;
  if (effect.equals("Радуга верт.")) return 2;
  if (effect.equals("Радуга гориз.")) return 3;
  if (effect.equals("Смена цвета")) return 4;
  if (effect.equals("Безумие 3D")) return 5;
  if (effect.equals("Облака 3D")) return 6;
  if (effect.equals("Лава 3D")) return 7;
  if (effect.equals("Плазма 3D")) return 8;
  if (effect.equals("Радуга 3D")) return 9;
  if (effect.equals("Павлин 3D")) return 10;
  if (effect.equals("Зебра 3D")) return 11;
  if (effect.equals("Лес 3D")) return 12;
  if (effect.equals("Океан 3D")) return 13;
  if (effect.equals("Цвет")) return 14;
  if (effect.equals("Снегопад")) return 15;
  if (effect.equals("Матрица")) return 16;
  if (effect.equals("Светлячки")) return 17;

}

String Get_EFFName (int eff_idx) {

  switch (eff_idx) {
    case 0: return "Конфетти";
    case 1: return "Огонь";
    case 2: return "Радуга верт.";
    case 3: return "Радуга гориз.";
    case 4: return "Смена цвета";
    case 5: return "Безумие 3D";
    case 6: return "Облака 3D";
    case 7: return "Лава 3D";
    case 8: return "Плазма 3D";
    case 9: return "Радуга 3D";
    case 10: return "Павлин 3D";
    case 11: return "Зебра 3D";
    case 12: return "Лес 3D";
    case 13: return "Океан 3D";
    case 14: return "Цвет";
    case 15: return "Снегопад";
    case 16: return "Матрица";
    case 17: return "Светлячки";
  }

}

void MQTTUpdateState () {
  
   mqttclient.publish(String("homeassistant/light/"+clientId+"/status").c_str(), ONflag ? "ON" : "OFF", true);
   mqttclient.publish(String("homeassistant/light/"+clientId+"/brightness/status").c_str(), String(modes[currentMode].brightness).c_str(), true);
   mqttclient.publish(String("homeassistant/light/"+clientId+"/effect/status").c_str(), Get_EFFName(currentMode).c_str(), true);
   mqttclient.publish(String("homeassistant/light/"+clientId+"/effect/speed/status").c_str(), String(modes[currentMode].speed).c_str(), true);
   mqttclient.publish(String("homeassistant/light/"+clientId+"/effect/scale/status").c_str(), String(modes[currentMode].scale).c_str(), true);

   char sRGB[15];
   sprintf(sRGB, "%d,%d,%d", r, g, b);
   mqttclient.publish(String("homeassistant/light/"+clientId+"/rgb/status").c_str(), sRGB, true);

   mqttclient.publish(String("homeassistant/light/"+clientId+"/state").c_str(), "online", true);

}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  String Payload = "";

  #ifdef DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  #endif

  for (int i = 0; i < length; i++) Payload += (char)payload[i];
  #ifdef DEBUG
  Serial.println(Payload);
  #endif

  if (String(topic) == "homeassistant/light/"+clientId+"/switch") {

      Serial.print("Command arrived: ");
      Serial.println(Payload);

      ONflag = (Payload == "ON") ? true : false;      
      changePower();
      sendCurrent();
      MQTTUpdateState();
      
  }

  if (String(topic) == "homeassistant/light/"+clientId+"/brightness/set") {

      Serial.print("Command arrived: brightness "); Serial.println(Payload);

      modes[currentMode].brightness = Payload.toInt();
      FastLED.setBrightness(modes[currentMode].brightness);
      settChanged = true;
      eepromTimer = millis();
      MQTTUpdateState();

  }

  if (String(topic) == "homeassistant/light/"+clientId+"/effect/set") {

      Serial.print("Command arrived: effect set to "); Serial.println(Payload);

      currentMode = Get_EFFIDX(Payload);
      saveEEPROM();
      loadingFlag = true;
      FastLED.clear();
      delay(1);
      sendCurrent();
      FastLED.setBrightness(modes[currentMode].brightness);
      MQTTUpdateState();
      
  }

  if (String(topic) == "homeassistant/light/"+clientId+"/rgb/set") {

      Serial.print("Command arrived: rgb "); Serial.println(Payload);
      
      r = getValue(Payload, ',', 0).toInt();
      g = getValue(Payload, ',', 1).toInt();
      b = getValue(Payload, ',', 2).toInt();

      currentMode = 14;
      loadingFlag = true;
      delay(1);
      sendCurrent();
      FastLED.setBrightness(modes[currentMode].brightness);
      MQTTUpdateState();

  }

  if (String(topic) == "homeassistant/light/"+clientId+"/effect/speed/set") {

      Serial.print("Command arrived: speed "); Serial.println(Payload);

      modes[currentMode].speed = Payload.toInt();
      saveEEPROM();
      loadingFlag = true;
      settChanged = true;
      eepromTimer = millis();
      MQTTUpdateState();

  }

  if (String(topic) == "homeassistant/light/"+clientId+"/effect/scale/set") {

      Serial.print("Command arrived: scale "); Serial.println(Payload);

      if (currentMode == 17 && Payload.toInt() > 100) Payload = "100";
      
      modes[currentMode].scale = Payload.toInt();
      saveEEPROM();
      loadingFlag = true;
      settChanged = true;
      eepromTimer = millis();
      MQTTUpdateState();

  }

}

uint32_t timing = 0;
uint32_t mqtt_timeout = 5000;
uint8_t mqtt_reconnection_count = 0;

void MQTTreconnect() {
  if (USE_MQTT && (millis() - timing > mqtt_timeout)) {

      MQTTconfig MQTTConfig = readMQTTConfig();

      mqttclient.setServer(MQTTConfig.HOST, String(MQTTConfig.PORT).toInt());
      mqttclient.setCallback(MQTTcallback);

      if ((millis() - timing > mqtt_timeout) && !mqttclient.connected()) {

          timing = millis();
          
          if (!WiFi.isConnected()) WiFi.reconnect();
      
          Serial.printf("Attempting MQTT connection to %s on port %s as %s...", MQTTConfig.HOST, MQTTConfig.PORT, MQTTConfig.USER);

          // подключаемся к MQTT серверу
          if (mqttclient.connect(clientId.c_str(), MQTTConfig.USER, MQTTConfig.PASSWD)) {
          
            Serial.println("connected!");

            mqtt_timeout = 5000;
            mqtt_reconnection_count = 0;

            #ifdef DEBUG
            //mqttclient.subscribe(String("homeassistant/light/"+clientId+"/config").c_str());
            #endif

            HomeAssistantSendDiscoverConfig();

            // подписываемся на топики
            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/switch").c_str());
            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/status").c_str());

            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/brightness/status").c_str());
            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/brightness/set").c_str());

            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/effect/status").c_str());
            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/effect/set").c_str());

            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/rgb/status").c_str());
            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/rgb/set").c_str());

            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/effect/speed/status").c_str());
            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/effect/speed/set").c_str());
          
            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/effect/scale/status").c_str());
            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/effect/scale/set").c_str());

            mqttclient.subscribe(String("homeassistant/light/"+clientId+"/state").c_str());

            MQTTUpdateState();
          
        } else {

          mqtt_reconnection_count += 1;
          mqtt_timeout *= 2;

          if (mqtt_reconnection_count >= 9) {

            Serial.println("Сan not establish a connection, resetting ESP...");
            ESP.restart(); //ESP.reset();

            //mqtt_timeout = 5000;
            //mqtt_reconnection_count = 0;
            
          }
          
          Serial.print("failed, rc=");
          Serial.print(mqttclient.state());
          Serial.printf(" try again in %d seconds\n", mqtt_timeout/1000);
        }
      }
    }
  }

void HomeAssistantSendDiscoverConfig() {

  DynamicJsonDocument hass_discover(1024);
  
  hass_discover["~"] = "homeassistant/light/"+clientId;
  hass_discover["name"] = "Gyver Lamp "+ clientId; // name
  hass_discover["uniq_id"] = String(ESP.getChipId(), HEX); // unique_id
  hass_discover["avty_t"] = "~/state";         // availability_topic
  hass_discover["pl_avail"] = "online";        // payload_available
  hass_discover["pl_not_avail"] = "offline";   // payload_not_available

  hass_discover["bri_cmd_t"] = "~/brightness/set";     // brightness_command_topic
  hass_discover["bri_stat_t"] = "~/brightness/status"; // brightness_state_topic
  hass_discover["bri_scl"] = 255;

  hass_discover["cmd_t"] = "~/switch"; // command_topic
  hass_discover["stat_t"] = "~/status"; // state_topic
  
  hass_discover["fx_cmd_t"] = "~/effect/set";     // effect_command_topic
  hass_discover["fx_stat_t"] = "~/effect/status"; // effect_state_topic

  hass_discover["rgb_cmd_t"] = "~/rgb/set";     // rgb_command_topic
  hass_discover["rgb_stat_t"] = "~/rgb/status"; // rgb_state_topic

  String hass_discover_str;
  serializeJson(hass_discover, hass_discover_str);

  const char eff_list[] = R"=====(, "fx_list": ["Конфетти", "Огонь", "Радуга верт.", "Радуга гориз.", "Смена цвета", "Безумие 3D", "Облака 3D", "Лава 3D", "Плазма 3D", "Радуга 3D", "Павлин 3D", "Зебра 3D", "Лес 3D", "Океан 3D", "Цвет", "Снегопад", "Матрица", "Светлячки"] })=====";  // effect_list
  const char dev_reg_tpl[] = R"=====(, "device": {"ids": ["%s"], "name": "Gyver Lamp", "mf": "Alex Gyver", "mdl": "Gyver Lamp v2", "sw": "1.4 MQTT"})=====";  // device reg
  char dev_reg[256];

  sprintf(dev_reg, dev_reg_tpl, String(ESP.getChipId(), HEX).c_str());
  hass_discover_str = hass_discover_str.substring(0, hass_discover_str.length() - 1);

  hass_discover_str += dev_reg;
  hass_discover_str += eff_list;

  #ifdef DEBUG
  //Serial.println(hass_discover_str);
  //mqttclient.publish(String("homeassistant/light/"+clientId+"/config").c_str(), "");
  #endif

  mqttclient.publish(String("homeassistant/light/"+clientId+"/config").c_str(), hass_discover_str.c_str(), true) ? Serial.println("Success sent discover message") : Serial.println("Error sending discover message");
}
