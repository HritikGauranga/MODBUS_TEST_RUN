#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ModbusIP_ESP8266.h>

#define LED_PIN 2

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };
IPAddress ip(192, 168, 8, 177);

ModbusIP mb;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  SPI.begin(18, 19, 23, 5);
  Ethernet.init(5);
  Ethernet.begin(mac, ip);

  delay(1000);

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());

  // Start Modbus TCP server
  mb.server();

  // Coils
  mb.addCoil(0); // LED

  // Holding Registers
  mb.addHreg(0); // Set Temp
  mb.addHreg(1); // Speed

  // Input Registers
  mb.addIreg(0);
  mb.addIreg(1);
  mb.addIreg(2);

  Serial.println("Modbus TCP Ready");
}

void loop() {
  mb.task();

  // Read from client
  int led = mb.Coil(0);
  digitalWrite(LED_PIN, led);

  int setTemp = mb.Hreg(0);
  int setSpeed = mb.Hreg(1);

  // Simulated values
  mb.Ireg(0, setTemp + (millis() % 5));
  mb.Ireg(1, 220 + (millis() % 10));
  mb.Ireg(2, millis() / 1000);
}