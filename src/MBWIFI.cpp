#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoModbus.h>

// 🔧 WiFi Credentials
const char* ssid = "Airtel4G";
const char* password = "GaurangA@2025#";

// Server on Modbus port
WiFiServer wifiServer(502);
ModbusTCPServer modbusTCPServer;

// LED pin
#define LED_PIN 2
#define LED_PIN2 16 

int prevLED = -1;
int prevPump = -1;
int prevLED2 = -1;

int prevSetTemp = -1;
int prevSetSpeed = -1;

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);

  // 🔧 Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("ESP IP: ");
  Serial.println(WiFi.localIP());

  // Start server
  wifiServer.begin();

  if (!modbusTCPServer.begin()) {
    Serial.println("Modbus failed!");
    while (1);
  }

  // Configure Modbus
  modbusTCPServer.configureHoldingRegisters(0, 3);
  modbusTCPServer.configureCoils(0, 3);

  Serial.println("Ready...");
}

void loop() {
  WiFiClient client = wifiServer.available();

  if (client) {
    Serial.println("Client detected");

    modbusTCPServer.accept(client);

    while (client.connected()) {
      modbusTCPServer.poll();

      // READ CURRENT VALUES
      int led = modbusTCPServer.coilRead(0);
      int pump = modbusTCPServer.coilRead(1);
      int led2 = modbusTCPServer.coilRead(2);

      int setTemp = modbusTCPServer.holdingRegisterRead(0);
      int setSpeed = modbusTCPServer.holdingRegisterRead(1);

      // EVENT DETECTION
      if (led != prevLED) {
        Serial.print("LED changed: ");
        Serial.println(led ? "ON" : "OFF");
        prevLED = led;
      }

      if (pump != prevPump) {
        Serial.print("Pump changed: ");
        Serial.println(pump ? "ON" : "OFF");
        prevPump = pump;
      }
      
      if (led2 != prevLED2) {
          Serial.print("LED2 changed: ");
          Serial.println(led2 ? "ON" : "OFF");
          prevLED2 = led2;
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
      digitalWrite(LED_PIN2, led2);

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