#include <ArduinoOTA.h>
#include <FS.h>
#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

#include <FastLED.h>

#include <html.h>

#define TRIGGER_PIN D1 // @reset config on button "flash"
#define NUM_LEDS    100
// Data pin that led data will be written out over
#define DATA_PIN D2
#define DNS_PORT 53

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

//#define LED_TYPE    WS2812B
//#define COLOR_ORDER GRB
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB

#define FRAMES_PER_SECOND  60

#define SNOW_DENSE 30     // плотность снегопада

#define WIDTH  NUM_LEDS        // ширина матрицы
#define HEIGHT 1               // высота матрицы

#define MATRIX_TYPE 1         // тип матрицы: 0 - зигзаг, 1 - последовательная
#define CONNECTION_ANGLE 0    // угол подключения: 0 - левый нижний, 1 - левый верхний, 2 - правый верхний, 3 - правый нижний
#define STRIP_DIRECTION 0     // направление ленты из угла: 0 - вправо, 1 - вверх, 2 - влево, 3 - вниз

// **************** НАСТРОЙКА МАТРИЦЫ ****************
#if (CONNECTION_ANGLE == 0 && STRIP_DIRECTION == 0)
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y y

#elif (CONNECTION_ANGLE == 0 && STRIP_DIRECTION == 1)
#define _WIDTH HEIGHT
#define THIS_X y
#define THIS_Y x

#elif (CONNECTION_ANGLE == 1 && STRIP_DIRECTION == 0)
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y (HEIGHT - y - 1)

#elif (CONNECTION_ANGLE == 1 && STRIP_DIRECTION == 3)
#define _WIDTH HEIGHT
#define THIS_X (HEIGHT - y - 1)
#define THIS_Y x

#elif (CONNECTION_ANGLE == 2 && STRIP_DIRECTION == 2)
#define _WIDTH WIDTH
#define THIS_X (WIDTH - x - 1)
#define THIS_Y (HEIGHT - y - 1)

#elif (CONNECTION_ANGLE == 2 && STRIP_DIRECTION == 3)
#define _WIDTH HEIGHT
#define THIS_X (HEIGHT - y - 1)
#define THIS_Y (WIDTH - x - 1)

#elif (CONNECTION_ANGLE == 3 && STRIP_DIRECTION == 2)
#define _WIDTH WIDTH
#define THIS_X (WIDTH - x - 1)
#define THIS_Y y

#elif (CONNECTION_ANGLE == 3 && STRIP_DIRECTION == 1)
#define _WIDTH HEIGHT
#define THIS_X y
#define THIS_Y (WIDTH - x - 1)

#else
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y y
#pragma message "Wrong matrix parameters! Set to default"

#endif

const char* pass = "HappyNewYear";


CRGB leds[NUM_LEDS];

volatile unsigned long buttlastSent = 0;

unsigned int globalBrightness = 128;
unsigned int globalDelay = 30;
unsigned int globalR = 0x7F;
unsigned int globalG = 0x7F;
unsigned int globalB = 0x7F;
unsigned int brightnessBreath = 1;
unsigned int cycleHue = 1;

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

byte breathBrightness;

//ESP8266WebServer server(80);
//DNSServer dnsServer;
/*
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

boolean captivePortal() {
  if (!isIp(server.hostHeader()) ) {
    Serial.println("Request redirected to captive portal");
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send ( 302, "text/plain", "");
    // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}


void handleRoot() {
    if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
      return;
    }

    String page = FPSTR(HTTP_HEAD);

    page += FPSTR(HTTP_STYLE);
    page += FPSTR(HTTP_HEAD_END);
    page.replace("{title}", String("Mazy-IoT NewYear Lighs"));
    page += FPSTR(HTTP_FORM_START);
    page += FPSTR(HTTP_FORM_PARAM);
    page.replace("{bv}", String(globalBrightness));
    page.replace("{brbr}", (brightnessBreath == 1)?" checked ":"");
    page.replace("{huc}", (cycleHue == 1)?" checked ":"");
    page.replace("{dv}", String(globalDelay));

    page += FPSTR(HTTP_FORM_PARAME);
    page.replace("{csr}", String(globalR));
    page.replace("{csg}", String(globalG));
    page.replace("{csb}", String(globalB));

    page.replace("{e_static}", (cycleHue == 1)?" checked ":"");

    page += FPSTR(HTTP_FORM_END);
    page += FPSTR(HTTP_SCRIPT);
    page += FPSTR(HTTP_END);

    server.sendHeader("Content-Length", String(page.length()));
    server.send(200, "text/html; charset=utf-8", page);
}



void handleNotFound(){
    if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
      return;
    }
  server.send(404, "text/plain", "Nothing here");
}
*/

// получить номер пикселя в ленте по координатам
uint16_t getPixelNumber(int8_t x, int8_t y) {
  if ((THIS_Y % 2 == 0) || MATRIX_TYPE) {               // если чётная строка
    return (THIS_Y * _WIDTH + THIS_X);
  } else {                                              // если нечётная строка
    return (THIS_Y * _WIDTH + _WIDTH - THIS_X - 1);
  }
}




void rainbow() {
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}


void addGlitter( fract8 chanceOfGlitter) {
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void rainbowWithGlitter() {
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void glitter() {
  fadeToBlackBy( leds, NUM_LEDS, 10);
  addGlitter(80);
}

void confetti() {
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  addGlitter(80);

}

void sinelon() {
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}


// *********** "дыхание" яркостью ***********
boolean brightnessDirection;
void brightnessRoutine() {
  if (brightnessDirection) {
    breathBrightness += 2;
    if (breathBrightness > globalBrightness - 1) {
      brightnessDirection = false;
    }
  } else {
    breathBrightness -= 2;
    if (breathBrightness < 1) {
      brightnessDirection = true;
    }
  }
  FastLED.setBrightness(breathBrightness);
}


// функция отрисовки точки по координатам X Y
void drawPixelXY(int8_t x, int8_t y, CRGB color) {
  if (x < 0 || x > WIDTH - 1 || y < 0 || y > HEIGHT - 1) return;
  leds[getPixelNumber(x, y)] = color;
}

// функция получения цвета пикселя по его номеру
uint32_t getPixColor(int thisPixel) {
  if (thisPixel < 0 || thisPixel > NUM_LEDS - 1) return 0;
  return (((uint32_t)leds[thisPixel].r << 16) | ((long)leds[thisPixel].g << 8 ) | (long)leds[thisPixel].b);
}

// функция получения цвета пикселя в матрице по его координатам
uint32_t getPixColorXY(int8_t x, int8_t y) {
  return getPixColor(getPixelNumber(x, y));
}


void snow() {
  // сдвигаем всё вниз
  for (byte x = 0; x < WIDTH; x++) {
    for (byte y = 0; y < HEIGHT - 1; y++) {
      drawPixelXY(x, y, getPixColorXY(x, y + 1));
    }
  }

  for (byte x = 0; x < WIDTH; x++) {
    // заполняем случайно верхнюю строку
    // а также не даём двум блокам по вертикали вместе быть
    if (getPixColorXY(x, HEIGHT - 2) == 0 && (random(0, SNOW_DENSE) == 0))
      drawPixelXY(x, HEIGHT - 1, 0xE0FFFF - 0x101010 * random(0, 4));
    else
      drawPixelXY(x, HEIGHT - 1, 0x000000);
  }
}

void staticColor() {
  for (byte x = 0; x < WIDTH; x++) {
      drawPixelXY(x, HEIGHT - 1, CRGB(globalR,globalG,globalB));
  }
}




// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { staticColor, snow ,confetti, sinelon, rainbow, glitter };
//SimplePatternList gPatterns = { rainbow };
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern() {
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

/*
void readConfig() {
    //read configuration from FS json
    Serial.println("Settings init...");
    Serial.print("mounting FS...");

    if (SPIFFS.begin()) {
      Serial.println("OK");
      if (SPIFFS.exists("/lightsconfig.json")) {
        //file exists, reading and loading
        Serial.print("Reading config file ... ");
        File configFile = SPIFFS.open("/lightsconfig.json", "r");
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
                  String vgb = json["globalBrightness"];
                  globalBrightness = vgb.toInt();
                  String vbb = json["brightnessBreath"];
                  brightnessBreath = vbb.toInt();
                  String vch = json["cycleHue"];
                  cycleHue = vch.toInt();
                  String vgd = json["globalDelay"];
                  globalDelay = vgd.toInt();
                  String vgR = json["globalR"];
                  globalR = vgR.toInt();
                  String vgG = json["globalG"];
                  globalG = vgG.toInt();
                  String vgB = json["globalB"];
                  globalB = vgB.toInt();

                  Serial.println(" OK");
       } else {
            Serial.println("failed!");
       }
          configFile.close();
   } // exists
    } else {
      Serial.println("failed! Formatting");
      SPIFFS.format();
    }
    Serial.println("");
}
void saveConfig() {
      Serial.println("Saving config");
      DynamicJsonDocument json(1024);

      json["globalBrightness"] = globalBrightness;
      json["brightnessBreath"] = brightnessBreath;
      json["cycleHue"] = cycleHue;

      json["globalDelay"] = globalDelay;
      json["globalR"] = globalR;
      json["globalG"] = globalG;
      json["globalB"] = globalB;


      File configFile = SPIFFS.open("/lightsconfig.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
        SPIFFS.format();
      }
      serializeJson(json, Serial);
      Serial.println(" OK");
      serializeJson(json, configFile);
      configFile.close();
}//end save

void updateVars() {
    String message = "";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

  Serial.println(message);

  String s_brt = server.arg("brt");
  globalBrightness = s_brt.toInt();

  String s_brbr = server.arg("brbr");
  brightnessBreath = s_brbr.toInt();

  String s_huc = server.arg("huc");
  cycleHue = s_huc.toInt();

  String s_del = server.arg("del");
  globalDelay = s_del.toInt();

  String s_csr = server.arg("csr");
  globalR = s_csr.toInt();
  String s_csg = server.arg("csg");
  globalG = s_csg.toInt();
  String s_csb = server.arg("csb");
  globalB = s_csb.toInt();

}

void handleUpdate() {
    // update vars
    updateVars();
    server.send ( 200, "text/plain", "");
    server.client().stop(); // Stop is needed because we sent no content length
}

void handleSave() {
    // update vars
    updateVars();
    //save to memory
    saveConfig();
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send ( 302, "text/plain", "");
    // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
}


*/

void ICACHE_RAM_ATTR button_ISR() {
    unsigned long edelay = millis() - buttlastSent;
    if (edelay >= 100) { // remove jitter
	       Serial.println(F(" Button pressed "));
         nextPattern();
	       buttlastSent = millis();
    }
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

void setup() {
  Serial.begin(9600);
  Serial.println(F("# Mazy IoT WunderWaffel boot"));
  pinMode(TRIGGER_PIN, INPUT);

//  Serial.println("Sleep for 5 sec. Press and hold  FLASH to clear settings.");
/*  delay(5000);
  if ( digitalRead(TRIGGER_PIN) == LOW) {
      Serial.print("Formatting SPIFFS ... ");
      SPIFFS.format();
      Serial.println("done.");
  }
*/

/*
  readConfig();
  String ssid = "Mazy-IoT-Leds-"+String(ESP.getChipId(),16);


  Serial.println(WiFi.softAP(ssid, pass) ? "Ready" : "Failed!");

  WiFi.persistent(true);
  Serial.println("started");

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);  Serial.println("");
*/

  /* Setup the DNS server redirecting all the domains to the apIP */
//  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
//  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
/*
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/update", handleUpdate);
  server.onNotFound(handleNotFound);
*/
 // server.begin();
  //delay(1000);

  Serial.print(F("# Main mode init "));
  _iototasetup();
  attachInterrupt(digitalPinToInterrupt(TRIGGER_PIN), button_ISR , RISING);

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(globalBrightness);
}


void loop() {
  ArduinoOTA.handle();
 // dnsServer.processNextRequest();
 //  server.handleClient();


  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  // send the 'leds' array out to the actual LED strip
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND);

  // do some periodic updates
  if (cycleHue == 1) {
      EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  }
      EVERY_N_SECONDS( globalDelay ) { nextPattern(); } // change patterns periodically
  if (brightnessBreath == 1) {
      EVERY_N_MILLISECONDS( 20 ) { brightnessRoutine(); }
  }

}
