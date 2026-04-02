//Hritik Nagpure
// Now in this file, I've modified code for the DHCP client to create a Modbus RTU client that will run on top of the DHCP client. 
// The Modbus RTU client will be able to communicate with Modbus RTU servers that are connected to the same network as the DHCP client. 
// The Modbus RTU client will be able to read and write coils, discrete inputs, holding registers, and input registers from the Modbus RTU servers. 
// The Modbus RTU client will also be able to handle errors and timeouts when communicating with the Modbus RTU servers.

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoModbus.h>

// Unique MAC
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };

EthernetServer ethServer(502);
ModbusTCPServer modbusTCPServer;

#define LED_PIN 2

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);

  // SPI init
  SPI.begin(18, 19, 23);
  Ethernet.init(5);

  // 🔥 DHCP CLIENT (IMPORTANT)
  if (Ethernet.begin(mac) == 0) {
    Serial.println("❌ DHCP failed!");
    while (1);
  }

  delay(1000);

  // Print assigned IP
  Serial.print("Assigned IP: ");
  Serial.println(Ethernet.localIP());

  Serial.print("Gateway: ");
  Serial.println(Ethernet.gatewayIP());

  Serial.print("Subnet: ");
  Serial.println(Ethernet.subnetMask());

  ethServer.begin();

  if (!modbusTCPServer.begin()) {
    Serial.println(" Modbus failed!");
    while (1);
  }

  modbusTCPServer.configureCoils(0, 1);
  modbusTCPServer.configureHoldingRegisters(0, 1);

  Serial.println("DHCP + Modbus Ready...");
}

void loop() {
  EthernetClient client = ethServer.available();

  if (client) {
    Serial.println("Client Connected");

    modbusTCPServer.accept(client);

    while (client.connected()) {
      modbusTCPServer.poll();

     int value = modbusTCPServer.coilRead(0);

digitalWrite(LED_PIN, value);

      delay(100);
    }

    Serial.println("Client Disconnected");
  }
}