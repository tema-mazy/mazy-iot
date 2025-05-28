#include "ESP8266WiFi.h"        //I can connect to a Wifi
#include "ESP8266WebServer.h"   //I can be a server 'cos I have the class ESP8266WebServer available
#include "WiFiClient.h"

const char *ssid = "HAS";  //Credentials to register network defined by the SSID (Service Set IDentifier)
const char *password = "smarthome33"; //and the second one a password if you wish to use it.
ESP8266WebServer server(80);    //Class ESP8266WebServer and default port for HTTP

const int dirPin = D1; //This pin corresponds to GPIO5 (D1) (Yellow wire) https://nodemcu.readthedocs.io/en/latest/en/modules/gpio/
const int stepPin = D2; //This pin corresponds to GPIO4 (D2) (Orange wire)
int steps = 0; //This variable is related to the number of turns. If microstepping is disabled, 200 corresponds to a complete turn.
int stepDelay = 0; //This variable is the pulse duration in milliseconds and it is related to the rotation speed. Without microstepping, 1.8º are stepDelay ms.
bool dir = HIGH; //Rotation direction. HIGH is clockwise.

void handleRootPath() {
 server.send(200, "text/plain", "Ready, player one.");
}

void handleInit() {// Handler. 192.168.XXX.XXX/Init?Dir=HIGH&Delay=5&Steps=200 (One turn clockwise in one second)
    steps = 0; //Motor stopped if the arguments are wrong.
    stepDelay = 0;
    String message = "Initialization with: ";

    if (server.hasArg("Dir")) {
	digitalWrite(dirPin, server.arg("Dir") == "HIGH"); //This is a cunning way of checking the value of the argument Dir.
         message += "Direction: ";
	 message += server.arg("Dir");
     }
     if (server.hasArg("Delay")) {
	 stepDelay = (server.arg("Delay")).toInt(); //Converts the string to integer.
	 message += " Delay: ";
	 message += server.arg("Delay");
     }
     if (server.hasArg("Steps")) {
	 steps = (server.arg("Steps")).toInt();
	 message += " Steps: ";
	 message += server.arg("Steps");
     }
 
    server.send(200, "text/plain", message); //It's better to return something so the browser don't get frustrated+ 
 
     for (int i = 0; i < steps; i++) { //Create a square wave signal with the incoming data.
	 digitalWrite(stepPin, HIGH);
	 delay(stepDelay);
	 digitalWrite(stepPin, LOW);
	 delay(stepDelay);
     }

}

void setup() {
    Serial.begin(9600); //I can debbug through the serial port

    pinMode(dirPin, OUTPUT); // Pins are outputs
    pinMode(stepPin, OUTPUT);

    // Configure NODEMCU as Access Point
    Serial.print("Configuring access point...");
    WiFi.softAP(ssid); //Password is not necessary
    IPAddress myIP = WiFi.softAPIP(); //Get the IP assigned to itself.
    Serial.print("AP IP address: "); //This is written in the PC console.
    Serial.println(myIP);

    delay(1000);

    server.on("/", handleRootPath); 
    server.on("/Init", handleInit); 

    server.begin(); //Let's call the begin method on the server object to start the server.

    Serial.println("HTTP server started");
}

void loop() {
 server.handleClient(); 
}

