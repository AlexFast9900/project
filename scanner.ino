#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

//--------------Wi-Fi------------------
const char* ssid = "test"; // Название сети Wi-Fi (Вместо звёздочек)
const char* password = "testtest"; // Пароль от сети Wi-Fi (Вместо звёздочек)
//-------------------------------------

unsigned long bot_lasttime;
const unsigned long BOT_MTBS = 500;
bool system_mode = 0, solar_mode = 0, windmill_mode = 0, led_mode = 0;
int light = -1, wind = -1;

const char BotToken[] = "1904336275:AAF7OqAX4SyrhmGq1xzx-dGAo4AsEu4csWQ"; // Токен бота телеграм @RafflesSNKRSBot
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot (BotToken, secured_client);

int relay = D8;
Servo servos[3]; // Создаём массив серво-моторов
RF24 radio(2, 4); // порты D9, D10: CSN CE для радиомодуля
const uint32_t pipe = 111156789; // адрес рабочей трубы;
byte data[5]; // Массив для приёма данных
int scn;  //счетчик циклов прослушивания эфира
int sg0, sg1;  //счетчик числа принятых пакетов с передатчика

void handleNewMessages(int numNewMessages) // Функция бота телеграм
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/hand")
    {
      if(system_mode == 1){
        bot.sendMessage(chat_id, "Manual control mode is already enabled", "");
      }
      else{
        system_mode = 1;
        bot.sendMessage(chat_id, "Manual control mode is successfully enabled!", "");
        lcd.setCursor(15, 1); lcd.print("H");
      }
    }

    if (text == "/auto")
    {
      if(system_mode == 0){
        bot.sendMessage(chat_id, "Automatic control mode is already enabled", "");
      }
      else{
        system_mode = 0;
        bot.sendMessage(chat_id, "Automatic control mode is successfully enabled!", "");
        lcd.setCursor(15, 1); lcd.print("A");
      }
    }

    if (text == "/status")
    {
      bot.sendMessage(chat_id, "Light level: " + String(light) + "\nWind level: " + String(wind), "");
    }
    if (text == "/manage"){
      if(system_mode == 0){
        bot.sendMessage(chat_id, "‼️It is impossible to control the system while it is in automatic mode.\n\nPlease use /hand to switch to manual control.", "");
      }
      else{
        bot.sendMessage(chat_id, "Use /solar to fold/unfold the solar panel.\n\nUse /windmill to lock/unlock the windmill.", "");
      }
    }
    if (text == "/solar"){
      if(system_mode == 0){
        bot.sendMessage(chat_id, "‼️It is impossible to control the system while it is in automatic mode.\n\nPlease use /hand to switch to manual control.", "");
      }
      else{
        if(solar_mode == 0){
          solar_mode = 1;
          bot.sendMessage(chat_id, "The solar panel has been successfully unfolded!", "");
        }
        else{
          solar_mode = 0;
          bot.sendMessage(chat_id, "The solar panel has been successfully folded!", "");
        }
      }
    }
    if (text == "/windmill"){
      if(system_mode == 0){
        bot.sendMessage(chat_id, "‼️It is impossible to control the system while it is in automatic mode.\n\nPlease use /hand to switch to manual control.", "");
      }
      else{
        if(windmill_mode == 0){
          windmill_mode = 1;
          bot.sendMessage(chat_id, "The windmill has been successfully unlocked!", "");
        }
        else{
          windmill_mode = 0;
          bot.sendMessage(chat_id, "The windmill has been successfully locked!", "");
        }
      }
    }
    if (text == "/led"){
      if(system_mode == 0){
        bot.sendMessage(chat_id, "‼️It is impossible to control the system while it is in automatic mode.\n\nPlease use /hand to switch to manual control.", "");
      }
      else{
        if(led_mode == 0){
          led_mode = 1;
          bot.sendMessage(chat_id, "LED ON!", "");
        }
        else{
          led_mode = 0;
          bot.sendMessage(chat_id, "LED OFF!", "");
        }
      }
    }
    if (text == "/start")
    {
      String welcome = "Welcome to the distributed information and control system of power supply and lighting, " + from_name + ".\n";
      welcome += "Here is a list of available commands:\n\n";
      welcome += "/status : get system status data\n";
      welcome += "/auto : enable automatic system management mode (enabled by default)\n";
      welcome += "/hand : enable manual control of the system\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

void setup() {
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);
  lcd.begin(D1,D3);
  lcd.backlight();
  lcd.setCursor(2, 0);
  lcd.print("GATEWAY V1.2");
  lcd.setCursor(4, 1);
  lcd.print("Starting...");
  delay(500); lcd.clear();
  Serial.begin(9600);
  //----------------Wi-Fi----------------
  configTime(0, 0, "pool.ntp.org");      // получить время UTC через NTP
  secured_client.setTrustAnchors(&cert); // Добавить корневой сертификат для api.telegram.org
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(ssid);
  lcd.setCursor(0, 0); lcd.print("Wi-Fi SSID:"); lcd.setCursor(0, 1); lcd.print(ssid);
  delay(500); lcd.clear(); lcd.print("Wi-Fi"); lcd.setCursor(0, 1); lcd.print("Connecting");
  WiFi.begin(ssid, password); // Подключение к Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
    Serial.print(".");
  }
  lcd.clear(); lcd.print("WiFi connected!"); lcd.setCursor(0, 1); lcd.print("IP:"); lcd.print(WiFi.localIP());
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP()); // Вывести IP подключенного Wi-Fi
  delay(1000);
  lcd.clear();
  //-------------------------------------
  lcd.print("RadioReceiver ON"); lcd.setCursor(0, 1); lcd.print("Loading...");
  Serial.println("Receiver ON");
  //---------------Radio---------------
  radio.begin();
  delay(2000);
  radio.setDataRate(RF24_1MBPS); // скорость обмена данными RF24_1MBPS или RF24_2MBPS
  radio.setCRCLength(RF24_CRC_8); // размер контрольной суммы 8 bit или 16 bit
  radio.setChannel(0x6f);         // установка канала
  radio.setAutoAck(false);       // автоответ
  radio.openReadingPipe(1, pipe); // открыть трубу 1 на приём
  radio.startListening();        // приём
  //-----------------------------------
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Light level -");
  lcd.setCursor(0, 1); lcd.print("Wind level -");
  servos[0].attach(D0); servos[1].attach(3); servos[2].attach(1);
  servos[0].write(90); servos[1].write(180); servos[2].write(110); // M3 - лево/право, M1 - ветряк, M2 - верх/низ
  lcd.setCursor(15, 1); lcd.print("A");
}

void follow_light(int vl, int vp, int nl, int np){
  int v = (vl + vp)/2, n = (nl+np)/2; 
  if(n>v){
    servos[2].write(180);
  }
  else{
    servos[2].write(140);
  }
  int l = (vl+nl)/2, r = (vp+np)/2;
  if(abs(l-r)>50){
  if(l>r){
    servos[0].write(0);
  }
  else{
    servos[0].write(180);
  }
  }
  else{
    servos[0].write(90);
  }
}
void fold_panel(){
  servos[0].write(90);
  servos[2].write(110); // ИЛИ 0, Я НЕ МОГУ ТЕСТИРОВАТЬ!!!!!!!! ЕСЛИ ЧТО ПОМЕНЯТЬ НА 0!!!!!!!!
}

void loop() {   
  //------------Приём данных из бота телеграм---------------
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
  //-------------Приём данных по радиоканалу-----------------
  int solar_data[4] = {0, 0, 0, 0};
  if (scn < 1000)
  {
    if (radio.available())
    {
      radio.read(data, 5);
      Serial.print("Data came from pipe " + String(pipe) + ": "); Serial.print(data[0]);
      Serial.println(" " + String(data[1]) + " " + String(data[2]) + " " + String(data[3]) + " " + String(data[4]));
      if(data[0]==0){
        light = (data[1] + data[2] + data[3] + data[4])/4; // Присваиваем среднее значение света с 4 фоторезисторов
        solar_data[0] = data[3]; solar_data[1] = data[4]; solar_data[2] = data[2]; solar_data[3] = data[1];
        lcd.setCursor(12, 0);
        lcd.print("   ");
        lcd.setCursor(12, 0);
        lcd.print(light);
        sg0++;
      }
      if(data[0]==1){
        wind = data[1];
        lcd.setCursor(11, 1);
        lcd.print("   ");
        lcd.setCursor(11, 1);
        lcd.print(data[1]);
        sg1++;
      }
    }
  } 
  else {//всего принято
    {
      Serial.println("Принято: " + String(sg0) + " и " + String(sg1)  + " пакетов");
      if(sg0==0){
        lcd.setCursor(12, 0); lcd.print("   "); lcd.setCursor(12, 0); lcd.print("-");
      }
      if(sg1==0){
        lcd.setCursor(11, 1); lcd.print("   "); lcd.setCursor(11, 1); lcd.print("-");
      }
      sg0 = 0;
      sg1 = 0;
    }
    scn = 0;
  }
  scn++;
  delay(10);
  if (scn >= 1000) scn = 1000; //защита от переполнения счетчика
  //---------------Управление сервоприводами-------------
  if(system_mode == 0){ // управление в автоматическом режиме
    if(light < 85 && wind > 5){ // При темноте и сильном ветре
      solar_mode = 0; // Складывается солнечная панель
      windmill_mode = 1; // Ветрогенератор активен
      digitalWrite(relay, HIGH); // Горит свет
    }
    else if(light >= 85 && wind > 5){ // Светло и сильный ветер
      solar_mode = 0; // Складывается солнечная панель
      windmill_mode = 1; // Ветрогенератор активен
      digitalWrite(relay, LOW); // НЕ горит свет
    }
    else if(light >= 85 && wind < 5){ // Светло и слабый ветер
      windmill_mode = 0; // Блокировка ветрогенератора
      solar_mode = 1; // Раскладывается солнечная панель
      follow_light(solar_data[0], solar_data[1], solar_data[2], solar_data[3]);
      digitalWrite(relay, LOW); // НЕ горит свет
    }
    else if(light < 85 && wind < 5){
      solar_mode = 0; // Складывается солнечная панель
      windmill_mode = 0; // Ветрогенератор НЕ активен
      digitalWrite(relay, HIGH); // Горит свет
    }
    if(solar_mode == 0){
      fold_panel();
    }
    if(windmill_mode == 0){
      servos[1].write(170);
    }
    else{
      servos[1].write(90);
    }
  }
  else{ // Управление в ручном режиме
    if(windmill_mode == 0){
      servos[1].write(170); // Сервопривод блокирует лопасти
    }
    else{
      servos[1].write(90); // Сервопривод НЕ блокирует лопасти
    }
    if(solar_mode == 0){
      fold_panel(); // Сервоприводы складывают солнечную панель
    }
    else{
      servos[0].write(90);
      servos[2].write(180);// Сервоприводы РАСКЛАДЫВАЮТ солнечную панель
    }
    if(led_mode == 0){
      digitalWrite(relay, LOW);
    }
    else{
      digitalWrite(relay, HIGH);
    }
  }
}
