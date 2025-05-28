#include <ArduinoOTA.h>
#include <Ticker.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <FastLED.h>
#include <html.h>
#include "gradient_palettes.h"
#include <WiFiManager.h>

#ifndef NUM_LEDS
#define NUM_LEDS    50
#endif

#ifdef NODEMCU
#define TRIGGER_PIN 0 // #flash Button on GPIO0
#define LED_MCU_GPIO 16  // onboard MCU led
#define DENSITY      1
#endif

#ifdef WEMOSD1MINI
#define TRIGGER_PIN D1 // Button on D1
#define LED_MCU_GPIO 2 // onboard esp8266 led
#define DENSITY      1       //255 / NUM_LEDS
#endif



#define DATA_PIN D2    // but physically to D4 .. why ??? i don't know

// hardcoded


FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#ifdef LEDSTRIP
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define FRAMES_PER_SECOND  50

#else
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB
#define FRAMES_PER_SECOND  50

#endif
CRGB leds[NUM_LEDS];


volatile unsigned int state = 1;
volatile unsigned int globalBrightness = 128;
volatile unsigned int globalDelay = 60;
volatile unsigned int brightnessBreath = 1;
volatile unsigned int cycleHue = 1;
volatile unsigned int hues = 20;
volatile unsigned int palc = 1;
volatile unsigned int pals = 5;
volatile unsigned int svv = 5;

extern const TProgmemRGBGradientPalettePtr gGradientPalettes[]; // These are for the fixed palettes in gradient_palettes.h
extern const uint8_t gGradientPaletteCount;                     // Total number of fixed palettes to display.
volatile uint8_t gCurrentPaletteNumber = 0;                              // Current palette number from the 'playlist' of color palettes
volatile uint8_t gCurrentPatternNumber = 0;
volatile uint8_t gHue = 0;
CRGBPalette16 gCurrentPalette;
volatile byte breathBrightness;

ESP8266WebServer server(80);

// Network credentials
String ssid { "Mazy-IoT-Lights-" };

Ticker ttread;
Ticker blinker;
Ticker timerWifiReconnect;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
WiFiManager wifiManager;

/* effects */

void rainbow() {
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, DENSITY  );

}

void addGlitter( fract8 chanceOfGlitter) {
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}


void glitter() {
  fadeToBlackBy( leds, NUM_LEDS, 10);
  addGlitter(80);
}

void confetti() {
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 16);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(32), 200, globalBrightness);
  addGlitter(80);

}

uint32_t getPixColor(CRGB thisPixel) {
  return (((uint32_t)thisPixel.r << 16) | (thisPixel.g << 8) | thisPixel.b);
}

int prevPos = 0;
void sinelon() {
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 32);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  if ( pos >= prevPos) {
    for (int i=prevPos+1; i<=pos; i++ ) {
      leds[i] += CHSV( gHue, 255, globalBrightness);
    }
  } else {
    for (int i=pos; i<prevPos; i++ ) {
      leds[i] += CHSV( gHue, 255, globalBrightness);
    }

  }
  prevPos = pos;
}



void drops() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, globalBrightness);
    dothue += 16;
  }
}

void bpm() {
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t beat = beatsin8( 32, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(gCurrentPalette, gHue+(i*2), beat-(i*10));
  }
}

void milis() {
  long ms = millis()/500;
  leds[ms % NUM_LEDS] = (ms % 2 == 0)?CRGB::Red:CRGB::Blue;
  leds[NUM_LEDS - ms % NUM_LEDS] = (ms % 2 == 0)?CRGB::Blue:CRGB::Red;
  leds[(millis()/50) % NUM_LEDS] = CHSV(gHue+random8(-10,10), 200, globalBrightness);
  fadeToBlackBy(leds, NUM_LEDS, 10);                     // 8 bit, 1 = slow, 255 = fast
}

void colors() {
    fill_solid(leds,NUM_LEDS, CHSV(gHue, 255, globalBrightness));

}

bool initflag = true; 
void plua() {

if (initflag) {    
   for ( int c = 0; c < 4; c++) {   
   for ( int i = 0; i < NUM_LEDS/4; i++) {
   switch( c ) {
    case 0:
       leds[c*NUM_LEDS/4+i] =  CRGB::Blue;
       break;
    case 1:
       leds[c*NUM_LEDS/4+i] =  CRGB::Yellow;
       break;
    case 2:
       leds[c*NUM_LEDS/4+i] =  CRGB::White;
       break;
    case 3:
       leds[c*NUM_LEDS/4+i] =  CRGB::Red;
       break;
   }
  }}
  initflag = false;
  } else {
    EVERY_N_MILLISECONDS(100) {

       CRGB first = leds[0];
       for ( int i = 1; i < NUM_LEDS; i++) {
	    leds[i-1] = leds[i];  
       }
       leds[NUM_LEDS-1] = first;
  }}
}

void fullBlack() {
  fill_solid(leds,NUM_LEDS,CRGB::Black);
  FastLED.show();
}



#define COOLING  55
// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120


void Fire2012WithPalette() {
// Array of temperature readings at each simulation cell
fadeToBlackBy(leds, NUM_LEDS, 10);                     // 8 bit, 1 = slow, 255 = fast
  const int numLeds = (NUM_LEDS > 200)? 200 : NUM_LEDS;

  static byte heat[numLeds];

  // Step 1.  Cool down every cell a little
    for( int i = 0; i < numLeds; i++) {
      heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / numLeds) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= numLeds - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random8() < SPARKING ) {
      int y = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < numLeds; j++) {
      // Scale the heat value from 0-255 down to 0-240
      // for best results with color palettes.
      byte colorindex = scale8( heat[j], 240);
      CRGB color = ColorFromPalette( gCurrentPalette , colorindex, globalBrightness);
      //CRGB color = ColorFromPalette( gCurrentPalette , colorindex);
      leds[j] = color;
    }
}



void candles() {
    for (int x = 0; x < NUM_LEDS; x++) {
      uint8_t flicker = random8(1, 100);
      leds[x] = CRGB(255 - flicker * 2, 150 - flicker, flicker / 2);
    }
} // candles()

void blendwave() {
  CRGB clr1;
  CRGB clr2;
  uint8_t speed;
  uint8_t loc1;


    speed = beatsin8(6, 0, 255);

    clr1 = blend(CHSV(beatsin8(3, 0, 255), 255, globalBrightness), CHSV(beatsin8(4, 0, 255), 255, globalBrightness), speed);
    clr2 = blend(CHSV(beatsin8(4, 0, 255), 255, globalBrightness), CHSV(beatsin8(3, 0, 255), 255, globalBrightness), speed);


    loc1 = beatsin8(10, 0, NUM_LEDS - 1);


    fill_gradient_RGB(leds, 0, clr2, loc1, clr1);
    fill_gradient_RGB(leds, loc1, clr2, NUM_LEDS - 1, clr1);

} // blendwave()




/* This is adapted from a routine created by Mark Kriegsman
*  Usage - noise8();
*/

uint16_t dist = 12345;         // A random number for our noise generator.
uint8_t scale = 30;          // Wouldn't recommend changing this on the fly, or the animation will be really blocky.

void noise8_pal() {

    for ( int i = 0; i < NUM_LEDS; i++) {                                     // Just ONE loop to fill up the LED array as all of the pixels change.
      uint8_t index = inoise8(i * scale, dist + i * scale) % 255;            // Get a value from the noise function. I'm using both x and y axis.
      leds[i] = ColorFromPalette(gCurrentPalette, index, globalBrightness, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
    }
    dist += beatsin8(10, 1, 4);
    // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
     // In some sketches, I've used millis() instead of an incremented counter. Works a treat.
} // noise8_pal()

uint8_t thisindex = 0;
void matrix_pal() {
  thisindex++;
   // One line matrix
    if (random8(90) > 80) {

        leds[NUM_LEDS - 1] = ColorFromPalette(gCurrentPalette, thisindex, globalBrightness, LINEARBLEND);
    } else {

        leds[NUM_LEDS - 1] = CHSV(gHue, 200, 50);
    }

      for (int i = 0; i < NUM_LEDS - 1 ; i++ ) leds[i] = leds[i + 1];


} // matrix_pal()

/*  This is from Serendipitous Circles from the August 1977 and April 1978 issues of Byte Magazine. I didn't do a very good job of it, but am at least getting some animation and the routine is very short.
*/

/*  Usage - serendipitous_pal();
*/

uint16_t Xorig = 0x013;
uint16_t Yorig = 0x021;
uint16_t X = Xorig;
uint16_t Y = Yorig;
uint16_t Xn;
uint16_t Yn;


void serendipitous_pal() {

  EVERY_N_SECONDS(15) {
    X = Xorig;
    Y = Yorig;
  }

  //Xn = X - (Y / 2); Yn = Y + (Xn / 2);
  //  Xn = X-Y/2;   Yn = Y+Xn/2;
  //  Xn = X-(Y/2); Yn = Y+(X/2.1);
  //  Xn = X-(Y/3); Yn = Y+(X/1.5);
  Xn = X-(2*Y); Yn = Y+(X/2.1);

  X = Xn;
  Y = Yn;
    thisindex = (sin8(X) + cos8(Y)) / 2;
    leds[X % (NUM_LEDS)] = ColorFromPalette(gCurrentPalette, thisindex, globalBrightness, LINEARBLEND);
    fadeToBlackBy(leds, NUM_LEDS, 32);                     // 8 bit, 1 = slow, 255 = fast

} // serendipitous_pal()

uint8_t c = 0;
void snow() {
    fadeToBlackBy( leds, NUM_LEDS, 8*DENSITY/1.5);
    addGlitter(80);
    EVERY_N_MILLISECONDS(svv*10) {
	for (int i = 0; i < NUM_LEDS - 1 ; i++ ) leds[i] = leds[i + 1];
    }

}


// *********** "дыхание" яркостью ***********
boolean brightnessDirection;
void brightnessRoutine() {
  if (brightnessDirection) {
    breathBrightness += 1;
    if (breathBrightness > globalBrightness + 32 || breathBrightness > 254 ) {
      brightnessDirection = false;
    }
  } else {
    breathBrightness -= 1;
    if (breathBrightness < globalBrightness - 32 || breathBrightness < 1) {
      brightnessDirection = true;
    }
  }
  FastLED.setBrightness(breathBrightness);
}


// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { plua, confetti, sinelon, rainbow, glitter, drops, bpm, Fire2012WithPalette, milis, colors , candles, blendwave, noise8_pal, matrix_pal, serendipitous_pal, snow};
int effectsEnabled[] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0,0,0,0,0};
String effectsName[] = { "PYCNiP3DA","Конфеті", "Повзучка", "Веселка", "Спалахи", "Крапельки", "Пульс", "Вогонь", "Секунди", "Кольори", "Свічки", "Хвилі","Шум", "Матриця", "Serendipity","Сніг"};

//SimplePatternList gPatterns = { rainbow };
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

bool checkOne() {
  uint sum = 0;
  for (int i = 0; i <  ARRAY_SIZE( gPatterns); i ++) {
    sum += effectsEnabled[i];
  }
  return (sum == 0);
}

void nextPattern() {
  int cnt =0;
  int next = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
  // add one to the current pattern number, and wrap around at the end
  while ( effectsEnabled[next] != 1 ) {
/*      Serial.println(cnt);
      Serial.print(F("next "));
      Serial.print(next);
      Serial.print(":");
      Serial.println(effectsEnabled[next]);*/
      next = (next + 1) % ARRAY_SIZE( gPatterns);

    cnt++;
    if (cnt > 30) {
      Serial.println(F("All patterns OFF!!!: "));
      effectsEnabled[0] = 1;
      gCurrentPatternNumber = 0;
      return;
    }
  }
  gCurrentPatternNumber = next;
  Serial.print(F("New pattern: "));
  Serial.println(gCurrentPatternNumber);

}

/* EEPROM */
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

void storeSettings() {
    eeWriteInt(0 ,state);
    eeWriteInt(4 ,globalDelay);
    eeWriteInt(8 ,globalBrightness);
    EEPROM.write(12,brightnessBreath);
    EEPROM.write(13,cycleHue);
    eeWriteInt(14,hues);
    EEPROM.write(19,palc);
    eeWriteInt(20,pals);
    eeWriteInt(24,svv);

    for (int i=0; i <  ARRAY_SIZE( gPatterns); i ++) {
    	byte e=(effectsEnabled[i]==1)?1:0;
    	EEPROM.write(30+i, e);
    	Serial.print(i);
    	Serial.print(":");
    	Serial.println(e);
    }
    EEPROM.commit();
}

void readSettings() {
    state = eeReadInt(0);
    globalDelay = eeReadInt(4);
    globalBrightness = eeReadInt(8);
    brightnessBreath = EEPROM.read(12);
    cycleHue = EEPROM.read(13);
    hues = eeReadInt(14);
    palc = EEPROM.read(19);
    pals = eeReadInt(20);
    svv = eeReadInt(24);

    for (int i=0; i <  ARRAY_SIZE( gPatterns); i ++) {
      	byte e=EEPROM.read(30+i);
      	Serial.print(i);
      	Serial.print(":");
      	Serial.println(e);
        effectsEnabled[i] = (e == 1)?1:0;
    }
    if (checkOne()) effectsEnabled[0] = 1;
    FastLED.setBrightness(globalBrightness);
}

/******************************/

/* webserver */
void handleRoot() {


    const String 	eTpl = "<li><label for='e{id}'><input {chk} type='checkbox' value='1' id='e{id}' name='e{id}'><span>{name}</span></label></li>";
    String page = FPSTR(_MIOT_HEAD);
    page += FPSTR(_MIOT_STYLE);
    page += FPSTR(_MIOT_BODY);
    page += FPSTR(_MIOT_SCRIPT);

    String effname = (state == 1)?effectsName[gCurrentPatternNumber]:"OFF";
    page.replace("{mode}", effname);
    page.replace("{title}", String("X-Mas Lights: "+effname));

    page.replace("{brt}", String(globalBrightness));
    page.replace("{brbr}", (brightnessBreath == 1)?" checked ":"");
    page.replace("{del}", String(globalDelay));

    page.replace("{huec}", (cycleHue == 1)?" checked ":"");
    page.replace("{hues}", String(hues));


    page.replace("{palc}", (palc == 1)?" checked ":"");
    page.replace("{pals}", String(pals));

    page.replace("{fps}", String(FRAMES_PER_SECOND));
    page.replace("{svv}", String(FRAMES_PER_SECOND - svv));

    String sEff = "";
    for (int i=0; i <  ARRAY_SIZE( gPatterns); i ++) {
      String e = eTpl;
	    e.replace("{chk}", (effectsEnabled[i] == 1)?"checked":"");
	    e.replace("{id}",String(i));
      e.replace("{name}", effectsName[i]);
      sEff += e;
    }
    page.replace("{effects}", sEff);
    page.replace("{millis}", String(millis()));
    page.replace("{state}", String((state == 0)?"<b style='color:red;'>On</b>":"<b style='color:blue;'>Off</b>"));
    page.replace("{statebage}", String((state == 1)?"on":"off"));
    page.replace("{signal}", String(WiFi.RSSI()));

    server.sendHeader("Content-Length", String(page.length()));
    server.send(200, "text/html; charset=utf-8", page);
}



void updateVars() {
    String message = "";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

  Serial.println(message);


  globalBrightness = server.arg("brt").toInt();
  globalDelay = server.arg("del").toInt();

  brightnessBreath = server.arg("brbr").toInt();

  cycleHue = server.arg("huec").toInt();
  hues = server.arg("hues").toInt();


  palc = server.arg("palc").toInt();
  pals = server.arg("pals").toInt();
   
  svv = (FRAMES_PER_SECOND - server.arg("svv").toInt()) ;


  for (int i=0; i <  ARRAY_SIZE( gPatterns); i ++) {
    effectsEnabled[i] = (server.arg("e"+String(i)).toInt() == 1)?1:0;
  }

  if (checkOne()) effectsEnabled[0] = 1;



}

void handleNext() {
    nextPattern();
    server.sendHeader("Location", String("/"), true);
    server.send ( 302, "text/plain", "/");
    server.client().stop(); // Stop is needed because we sent no content length
}

void changeState() {
  if ( state == 0) {
 Serial.println(F("STATE: 0->1"));
      state = 1;
  } else {
      state = 0;
    fullBlack();
Serial.println(F("STATE: 1->0"));
  }
  storeSettings();

}
void handleState() {
    changeState();
    server.sendHeader("Location", String("/"), true);
    server.send ( 302, "text/plain", "/");
    server.client().stop(); // Stop is needed because we sent no content length
}



void handleSave() {
    // update vars
    updateVars();
    //save to memory
    storeSettings();
    gCurrentPatternNumber = ARRAY_SIZE( gPatterns)-1;
    nextPattern();
    FastLED.setBrightness(globalBrightness);

    server.sendHeader("Location", String("/"), true);
    server.send ( 302, "text/plain", "/");
    // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
}

void resetSettings() {
      Serial.println(F("Reset settings..."));
      server.sendHeader("Location", String("/"), true);
      server.send ( 302, "text/plain", "/");
      // Empty content inhibits Content-length header so we have to close the socket ourselves.
      server.client().stop(); // Stop is needed because we sent no content length      

      wifiManager.resetSettings();
      SPIFFS.format();
      EEPROM.write(98, 0);
      EEPROM.commit();
      fullBlack();
      Serial.println(F("Reboot..."));

      delay(1000);
      ESP.restart();

}

volatile unsigned long buttlastSent = 0;
volatile unsigned int pressCnt = 0;
volatile bool setNext = false;
volatile bool setState = false;

void ICACHE_RAM_ATTR button_ISR() {
    unsigned long edelay = millis() - buttlastSent;
    if (edelay > 2000) { pressCnt = 0; }
    if (edelay >= 30) { // remove jitter
//	     Serial.println(F(" ** Button pressed "));
	     buttlastSent = millis();
	     pressCnt++;
    }
    if (pressCnt == 1) { setNext = true; }
    if (pressCnt == 3) { setState = true; }

    if (pressCnt > 10) {
      pressCnt = 0;
      resetSettings();
    }

}


void blink() {
  digitalWrite(LED_MCU_GPIO, !digitalRead(LED_MCU_GPIO));
}

void ledOff() {
 blinker.detach();
 digitalWrite(LED_MCU_GPIO, HIGH);
}


void connectToWifi() {
  Serial.println(F("# Connecting to Wi-Fi..."));
  blinker.attach(1.0, blink);
  WiFi.begin();
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.print(F("# Connected! IP address: "));
  Serial.println(WiFi.localIP());
  ledOff();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println(F("# Disconnected from Wi-Fi."));
  timerWifiReconnect.once(5, connectToWifi);
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.print(F("@@ Entered config mode IP:"));
  Serial.println(WiFi.softAPIP());
  blinker.attach(0.2, blink);
}

void _iototasetup() {
  ArduinoOTA.setPassword(OTAKEY);
  ArduinoOTA.onStart([]() { fullBlack(); blinker.attach(0.3, blink); });
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
void setup() {
  Serial.begin(9600);
  Serial.println(F("# Mazy IoT WunderWaffel boot"));
  ssid = ssid + String(ESP.getChipId(),16);

  pinMode(LED_MCU_GPIO, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);

  Serial.println(F("# EEPROM "));
  // EEPROM
  EEPROM.begin(102);
  delay(50);
  if (EEPROM.read(98) != 27) {   // first run
    EEPROM.write(98, 27);
    storeSettings();
    wifiManager.resetSettings();
  } else {
    readSettings();
    nextPattern();
  }

  Serial.println(F("# Init LED "));
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);


  // set master brightness control
  FastLED.setBrightness(globalBrightness);

  Serial.println(F(" done."));


  wifiManager.setDebugOutput(false);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setTimeout(60);
  wifiManager.setConfigPortalTimeout(60);

  Serial.println(F("# WiFi manager start..."));

  blinker.attach(0.6, blink);

  if (wifiManager.autoConnect(ssid.c_str())) {
    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  } else {
    Serial.println(F("# WiFi AP start..."));
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(10, 0, 0, 10), IPAddress(10, 0, 0, 10), IPAddress(255, 255, 255, 0));   // subnet FF
    WiFi.softAP(ssid.c_str());
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    ledOff();
  }
  if (WiFi.isConnected()) {
    Serial.print(F("# Connected! IP address: "));
    Serial.println(WiFi.localIP());
    ledOff();
  }


  Serial.println(F("# OTA setup "));
  _iototasetup();


  Serial.println(F("# Button attach "));
  attachInterrupt(digitalPinToInterrupt(TRIGGER_PIN), button_ISR , RISING);


  Serial.print(F("# HTTP server .."));
    //web server
     server.on("/", handleRoot);
     server.on("/save", handleSave);
     server.on("/next", handleNext);
     server.on("/state", handleState);
     server.on("/reset", resetSettings);
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
   random16_add_entropy( random8());
   Serial.println(F(" started"));

   gCurrentPaletteNumber = random8(0, gGradientPaletteCount);
   gCurrentPalette = gGradientPalettes[gCurrentPaletteNumber];
}


void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  if (setState) {
	setState = false;
	changeState();
  }

  EVERY_N_MILLISECONDS(1000/FRAMES_PER_SECOND) {
  if (state == 1) {
    gPatterns[gCurrentPatternNumber]();
    if (setNext) { setNext = false; nextPattern(); initflag = true;  }
    

    if (cycleHue == 1 && !( gCurrentPatternNumber == 0 || gCurrentPatternNumber == 7 || gCurrentPatternNumber == 11 )) { // fire & waves
        EVERY_N_MILLISECONDS_I( timingObj, 20 ) {
	       timingObj.setPeriod(hues);
	       gHue++;
	     }

    }
    if (palc == 1) {
      EVERY_N_SECONDS_I(timingObj, 5) {
      timingObj.setPeriod(pals);
      gCurrentPaletteNumber++;
      if (gCurrentPaletteNumber >= gGradientPaletteCount ) gCurrentPaletteNumber =0;
      gCurrentPalette = gGradientPalettes[gCurrentPaletteNumber];
    }} else  {
      if ( gCurrentPatternNumber == 7) gCurrentPalette = fire_gp; // fire_gp
      if ( gCurrentPatternNumber == 12) gCurrentPalette = es_emerald_dragon_08_gp; // MATRIX

    }

    if (brightnessBreath == 1 && !(gCurrentPatternNumber == 0 || gCurrentPatternNumber == 3)) {
        EVERY_N_MILLISECONDS( 15 ) { brightnessRoutine(); }
    }
    

    EVERY_N_SECONDS_I( timingObj, 5 ) { timingObj.setPeriod(globalDelay); nextPattern(); } // change patterns periodically
    FastLED.show();

  }
  }
}
