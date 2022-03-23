#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "SoftServo.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

//--------------Wi-Fi------------------
const char* ssid = "******"; // Название сети Wi-Fi (Вместо звёздочек)
const char* password = "*******"; // Пароль от сети Wi-Fi (Вместо звёздочек)
//-------------------------------------

unsigned long bot_lasttime;
const unsigned long BOT_MTBS = 500;
bool system_mode = 0, solar_mode = 0, windmill_mode = 0;
int light = -1, wind = -1;

const char BotToken[] = "1904336275:AAF7OqAX4SyrhmGq1xzx-dGAo4AsEu4csWQ"; // Токен бота телеграм @RafflesSNKRSBot
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot (BotToken, secured_client);

SoftServo servos[3]; // Создаём массив серво-моторов
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
  servos[0].attach(16); servos[1].attach(3); servos[2].attach(1);
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
  if (scn < 1000)
  {
    if (radio.available())
    {
      radio.read(data, 5);
      Serial.print("Data came from pipe " + String(pipe) + ": "); Serial.print(data[0]);
      Serial.println(" " + String(data[1]) + " " + String(data[2]) + " " + String(data[3]) + " " + String(data[4]));
      if(data[0]==0){
        light = (data[1] + data[2] + data[3] + data[4])/4; // Присваиваем среднее значение света с 4 фоторезисторов
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
        lcd.setCursor(12, 0);
        lcd.print("   ");
        lcd.setCursor(12, 0);
        lcd.print("-");
      }
      if(sg1==0){
        lcd.setCursor(11, 1);
        lcd.print("   ");
        lcd.setCursor(11, 1);
        lcd.print("-");
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
    if(light < 100 && wind > 5){ // При темноте и сильном ветре
      solar_mode = 0; // Складывается солнечная панель
      windmill_mode = 1; // Ветрогенератор активен
      // Горит свет
    }
    else if(light >= 100 && wind > 5){ // Светло и сильный ветер
      solar_mode = 0; // Складывается солнечная панель
      windmill_mode = 1; // Ветрогенератор активен
      // НЕ горит свет
    }
    else if(light >= 100 && wind < 5){ // Светло и слабый ветер
      windmill_mode = 0; // Блокировка ветрогенератора
      solar_mode = 1; // Раскладывается солнечная панель
      // НЕ горит свет
    }
    else if(light < 100 && wind < 5){
      solar_mode = 0; // Складывается солнечная панель
      windmill_mode = 0; // Ветрогенератор НЕ активен
      // Горит свет
    }
  }
  else{ // Управление в ручном режиме
    if(windmill_mode == 0){
      // Сервопривод блокирует лопасти
    }
    else{
      // Сервопривод НЕ блокирует лопасти
    }
    if(solar_mode == 0){
      // Сервоприводы складывают солнечную панель
    }
    else{
      // Сервоприводы РАСКЛАДЫВАЮТ солнечную панель
    }
  }
}