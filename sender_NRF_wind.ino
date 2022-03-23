#include <EncButton.h>
EncButton<EB_TICK, A0, A1> enc;
#include <SPI.h>
#include <RF24.h>
RF24 radio(8, 9); // порты D9, D10: CSN CE
const uint32_t pipe = 111156789; // адрес рабочей трубы;

byte data[5];

void setup() {
  Serial.begin(9600);
  Serial.println("TransmitterTester ON");
  radio.begin();                // инициализация
  //delay(2000);
  radio.setDataRate(RF24_1MBPS); // скорость обмена данными RF24_1MBPS или RF24_2MBPS
  radio.setCRCLength(RF24_CRC_8); // размер контрольной суммы 8 bit или 16 bit
  radio.setPALevel(RF24_PA_MAX); // уровень питания усилителя RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  radio.setChannel(0x6f);         // установка канала
  radio.setAutoAck(false);       // автоответ
  radio.powerUp();               // включение или пониженное потребление powerDown - powerUp
  radio.stopListening();  //радиоэфир не слушаем, только передача
  radio.openWritingPipe(pipe);   // открыть трубу на отправку
}

void loop() {
  data[0] = 1;
  data[1] = random(0); data[2] = -1; data[3] = -1; data[4] = -1;
  radio.write(&data, 5);
  Serial.println("data= " + String(data[1]));
  delay(1000);
}
