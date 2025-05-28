#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

#define LED D4
#define DNS_PORT 53


ESP8266WebServer server(80);
DNSServer dnsServer;

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
  digitalWrite(LED, LOW);
  server.send(200, "text/plain", "hello from esp8266!");
  digitalWrite(LED, HIGH);
}

void handleNotFound(){
  Serial.println("Request");
  digitalWrite(LED, LOW);
    if (!isIp(server.hostHeader()) ) {
     Serial.println("Request redirected to captive portal");
     server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
     server.send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
     server.client().stop(); // Stop is needed because we sent no content length
     return;
    }
  server.send(404, "text/plain", "Nothing here");
  digitalWrite(LED, HIGH);

}

void setup(void){
 Serial.begin(9600);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  String ssid = "Mazy-IoT-"+String(ESP.getChipId(),16);

  Serial.println(WiFi.softAP(ssid.c_str(), NULL) ? "Ready" : "Failed!");

  WiFi.persistent(true);
  Serial.println("start");

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);  Serial.println("");

  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

}


void loop(void){
  server.handleClient();
  dnsServer.processNextRequest();
}
