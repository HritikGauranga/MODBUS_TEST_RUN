#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoModbus.h>

// 🔧 MAC (unique)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };

// 🔧 Match your PC network (you are on 192.168.8.x)
IPAddress ip(192, 168, 8, 177);

// Server on Modbus port
EthernetServer newserver(502);
ModbusTCPServer modbusTCPServer;

// LED pin
#define LED_PIN 2

int prevLED = -1;
int prevPump = -1;

int prevSetTemp = -1;
int prevSetSpeed = -1;

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);

  // IMPORTANT: Explicit SPI init
  SPI.begin(18, 19, 23, 5);

  // Ethernet init
  Ethernet.init(5);  
  Ethernet.begin(mac, ip);

  delay(1000); // allow W5500 to stabilize

  // Debug info
  Serial.print("Hardware: ");
  Serial.println(Ethernet.hardwareStatus());

  Serial.print("Link: ");
  Serial.println(Ethernet.linkStatus());

  Serial.print("ESP IP: ");
  Serial.println(Ethernet.localIP());

  // Start server
  newserver.begin();

  if (!modbusTCPServer.begin()) {
    Serial.println("Modbus failed!");
    while (1);
  }

  // Configure Modbus
  modbusTCPServer.configureHoldingRegisters(0, 2);
  modbusTCPServer.configureCoils(0, 2);

  Serial.println("Ready...");
}

void loop() {
  EthernetClient client = newserver.available();

  if (client) {
    Serial.println(" Client detected");

    modbusTCPServer.accept(client);

    while (client.connected()) {
  modbusTCPServer.poll();

  //  READ CURRENT VALUES
  int led = modbusTCPServer.coilRead(0);
  int pump = modbusTCPServer.coilRead(1);

  
  int setTemp = modbusTCPServer.holdingRegisterRead(0);
  int setSpeed = modbusTCPServer.holdingRegisterRead(1);

  // EVENT DETECTION (IMPORTANT)

  if (led != prevLED) {
    Serial.print(" LED changed: ");
    Serial.println(led ? "ON" : "OFF");
    prevLED = led;
  }

  if (pump != prevPump) {
    Serial.print(" Pump changed: ");
    Serial.println(pump ? "ON" : "OFF");
    prevPump = pump;
  }

  if (setTemp != prevSetTemp) {
    Serial.print("Set Temperature updated: ");
    Serial.println(setTemp);
    prevSetTemp = setTemp;
  }

  if (setSpeed != prevSetSpeed) {
    Serial.print("Set Speed updated: ");
    Serial.println(setSpeed);
    prevSetSpeed = setSpeed;
  }

  // OUTPUT
  digitalWrite(LED_PIN, led);

  // SIMULATION
  int actualTemp = setTemp + (millis() % 5);
  int voltage = 220 + (millis() % 10);
  int counter = millis() / 1000;

  modbusTCPServer.configureInputRegisters(0, 3);

  modbusTCPServer.inputRegisterWrite(0, actualTemp);
  modbusTCPServer.inputRegisterWrite(1, voltage);
  modbusTCPServer.inputRegisterWrite(2, counter);

  delay(10);
}

    Serial.println("Client disconnected");
  }
}