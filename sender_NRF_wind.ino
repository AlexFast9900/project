#include <EncButton.h>
EncButton<EB_TICK, A0, A1> enc;
#include <SPI.h>
#include <RF24.h>
RF24 radio(6, 7); // порты D9, D10: CSN CE
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
  pinMode(A4, INPUT);
}
bool flag = 0;
void loop() {
  int sensorValue = digitalRead(A4);
  //Serial.print("Hall Sensor = " );        // Выводим текст
  //Serial.println(sensorValue);
  int count = 0; 
  for(int i=0; i<1000; i++){ // Считаем количество поворотов за 2 секунды
    delay(1);
    sensorValue = digitalRead(A4);
    if(sensorValue == 0 && flag == 0){
      count++; flag = 1; //Serial.println(sensorValue);
    }
    if(sensorValue == 1 && flag == 1){
      flag = 0; //Serial.println(sensorValue);
    }
  }
  //Serial.println("COUNT: " + String(count));
  data[1] = count/2*5;
  data[0] = 1; data[2] = -1; data[3] = -1; data[4] = -1;
  radio.write(&data, 5);
  Serial.println("data= " + String(data[1]));
  //delay(1000);
}
