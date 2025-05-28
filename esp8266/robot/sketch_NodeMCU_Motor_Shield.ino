// подключение библиотек
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
// создание сервера
ESP8266WebServer server(80);

const char *form = "<center><form action='/'>"
   "<button name='dir' type='submit' value='4'>Forward</button><br>"
   "<button name='dir' type='submit' value='1'>Left</button>&nbsp;"
   "<button name='dir' type='submit' value='2'>Right</button><br>"
   "<button name='dir' type='submit' value='3'>Reverse</button><p>"
   "<button name='dir' type='submit' value='5'>Stop</button>"
   "</form></center>";

// останов
void stop(void) {
   analogWrite(5, 0);
     analogWrite(4, 0);
}
// вперед
void forward(void) {
     analogWrite(5, 1023); analogWrite(4, 1023);
     digitalWrite(0, HIGH);digitalWrite(2, HIGH);
}
// назад
void backward(void) {
     analogWrite(5, 1023);analogWrite(4, 1023);
     digitalWrite(0, LOW);digitalWrite(2, LOW);
}
// влево
void left(void) {
     analogWrite(5, 1023);analogWrite(4, 1023);
     digitalWrite(0, LOW);digitalWrite(2, HIGH);
}
// вправо
void right(void) {
     analogWrite(5, 1023);analogWrite(4, 1023);
     digitalWrite(0, HIGH); digitalWrite(2, LOW);
}
void handle_form() {
     // при переходе по форме
     if (server.arg("dir"))  {
         // получить значение параметра формы "dir"
         int direction = server.arg("dir").toInt();
         // выбор для кнопок
         switch (direction)   {
             case 1:  left();
                  break;
             case 2:  right();
                 break;
             case 3:  backward();
                 break;
             case 4: forward();
                break;
             case 5:  stop();
                break;         }
// пауза
   delay(300);
  }
  // отправка формы в браузер
  server.send(200, "text/html", form);
}
void setup() {
     // подсоединение к WiFi роутеру
     // задайте свои значения для essid и passphrase
     WiFi.begin("honda.org.ua", "Dfakb123");
    // static ip, gateway, netmask
    WiFi.config(IPAddress(192,168,1,2),IPAddress(192,168,1,1),
                        IPAddress(255,255,255,0));
    // подсоединение
    while (WiFi.status() != WL_CONNECTED)  {
         delay(200); }
     // callback для http server
     server.on("/", handle_form);
     // запуск сервера
     server.begin();
    pinMode(5, OUTPUT); // motor A скорость
    pinMode(4, OUTPUT); // motor B скорость
    pinMode(0, OUTPUT); //  motor A направление
    pinMode(2, OUTPUT); //  motor B направление
}
void loop() {
server.handleClient();
}
