//Hritik Nagpure
// TCPRTU.cpp - Modbus TCP + RTU bridge on ESP32+W5500+FT232
// This code demonstrates how to create a Modbus bridge that connects a Modbus RTU slave (over UART) to a Modbus TCP server (over Ethernet).
// The ESP32 acts as both a Modbus RTU slave and a Modbus TCP server, allowing clients on the network to read/write data that is also accessible via the RTU interface.

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoModbus.h>
#include <ModbusRTU.h>

// ================== RTU SETUP ==================
ModbusRTU mbRTU;
// #define RXD2 16
// #define TXD2 17
#define RXD2 9  //SD2
#define TXD2 10 //SD3

// ================== TCP SETUP ==================
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE};
// IPAddress ip(192, 168, 8, 177);  // removed — DHCP assigns IP automatically

EthernetServer ethServer(502);
ModbusTCPServer modbusTCPServer;
EthernetClient activeClient;
bool clientActive = false;

// ================== HARDWARE ==================
#define LED_PIN  2
#define PUMP_PIN 15

// ================== SOURCE TRACKING ==================
typedef enum { SRC_NONE, SRC_RTU, SRC_TCP } DataSource;

DataSource srcLed   = SRC_NONE;
DataSource srcPump  = SRC_NONE;
DataSource srcTemp  = SRC_NONE;
DataSource srcSpeed = SRC_NONE;

// ================== SHARED DATA ==================
bool     coilLed     = false;
bool     coilPump    = false;
uint16_t setTemp     = 20;
uint16_t setSpeed    = 100;
uint16_t actualTemp  = 0;
uint16_t voltage     = 230;
uint16_t counterVal  = 0;

// ================== PREVIOUS VALUES ==================
bool     prevCoilLed_RTU   = false;
bool     prevCoilPump_RTU  = false;
uint16_t prevSetTemp_RTU   = 20;
uint16_t prevSetSpeed_RTU  = 100;

bool     prevCoilLed_TCP   = false;
bool     prevCoilPump_TCP  = false;
uint16_t prevSetTemp_TCP   = 20;
uint16_t prevSetSpeed_TCP  = 100;

// ================== TIMING ==================
#define LOOP_INTERVAL_MS  10
#define DHCP_RENEW_MS     60000  // check DHCP lease every 60 seconds
unsigned long lastLoopTime  = 0;
unsigned long lastDHCPCheck = 0;

// ================== HARDWARE APPLY ==================
void applyHardware() {
  digitalWrite(LED_PIN,  coilLed  ? HIGH : LOW);
  digitalWrite(PUMP_PIN, coilPump ? HIGH : LOW);
}

// ================== RTU SYNC ==================
void syncFromRTU() {
  bool     newLed   = mbRTU.Coil(0);
  bool     newPump  = mbRTU.Coil(1);
  uint16_t newTemp  = mbRTU.Hreg(0);
  uint16_t newSpeed = mbRTU.Hreg(1);

  if (newLed != prevCoilLed_RTU) {
    coilLed = newLed;
    prevCoilLed_RTU = newLed;
    srcLed = SRC_RTU;
    Serial.printf("[RTU] LED -> %s\n", coilLed ? "ON" : "OFF");
  }
  if (newPump != prevCoilPump_RTU) {
    coilPump = newPump;
    prevCoilPump_RTU = newPump;
    srcPump = SRC_RTU;
    Serial.printf("[RTU] Pump -> %s\n", coilPump ? "ON" : "OFF");
  }
  if (newTemp != prevSetTemp_RTU) {
    setTemp = newTemp;
    prevSetTemp_RTU = newTemp;
    srcTemp = SRC_RTU;
    Serial.printf("[RTU] SetTemp -> %d\n", setTemp);
  }
  if (newSpeed != prevSetSpeed_RTU) {
    setSpeed = newSpeed;
    prevSetSpeed_RTU = newSpeed;
    srcSpeed = SRC_RTU;
    Serial.printf("[RTU] SetSpeed -> %d\n", setSpeed);
  }
}

void syncToRTU() {
  mbRTU.Coil(0, coilLed);
  mbRTU.Coil(1, coilPump);
  mbRTU.Hreg(0, setTemp);
  mbRTU.Hreg(1, setSpeed);
  mbRTU.Ireg(0, actualTemp);
  mbRTU.Ireg(1, voltage);
  mbRTU.Ireg(2, counterVal);
}

// ================== TCP SYNC ==================
void syncFromTCP() {
  bool     newLed   = modbusTCPServer.coilRead(0);
  bool     newPump  = modbusTCPServer.coilRead(1);
  uint16_t newTemp  = modbusTCPServer.holdingRegisterRead(0);
  uint16_t newSpeed = modbusTCPServer.holdingRegisterRead(1);

  if (newLed != prevCoilLed_TCP) {
    coilLed = newLed;
    prevCoilLed_TCP = newLed;
    srcLed = SRC_TCP;
    Serial.printf("[TCP] LED -> %s\n", coilLed ? "ON" : "OFF");
  }
  if (newPump != prevCoilPump_TCP) {
    coilPump = newPump;
    prevCoilPump_TCP = newPump;
    srcPump = SRC_TCP;
    Serial.printf("[TCP] Pump -> %s\n", coilPump ? "ON" : "OFF");
  }
  if (newTemp != prevSetTemp_TCP) {
    setTemp = newTemp;
    prevSetTemp_TCP = newTemp;
    srcTemp = SRC_TCP;
    Serial.printf("[TCP] SetTemp -> %d\n", setTemp);
  }
  if (newSpeed != prevSetSpeed_TCP) {
    setSpeed = newSpeed;
    prevSetSpeed_TCP = newSpeed;
    srcSpeed = SRC_TCP;
    Serial.printf("[TCP] SetSpeed -> %d\n", setSpeed);
  }
}

void syncToTCP() {
  modbusTCPServer.coilWrite(0, coilLed);
  modbusTCPServer.coilWrite(1, coilPump);
  modbusTCPServer.holdingRegisterWrite(0, setTemp);
  modbusTCPServer.holdingRegisterWrite(1, setSpeed);
  modbusTCPServer.inputRegisterWrite(0, actualTemp);
  modbusTCPServer.inputRegisterWrite(1, voltage);
  modbusTCPServer.inputRegisterWrite(2, counterVal);
}

// ================== SIMULATION ==================
void updateSimulatedMetrics() {
  counterVal = millis() / 1000;
  actualTemp = setTemp + (millis() % 5);
  voltage    = 220 + (millis() % 10);
}

// ================== DHCP MAINTAIN ==================
void maintainDHCP() {
  unsigned long now = millis();
  if (now - lastDHCPCheck < DHCP_RENEW_MS) return;
  lastDHCPCheck = now;

  int result = Ethernet.maintain();
  switch (result) {
    case 0: break;  // lease still valid, nothing to do
    case 1: Serial.println("[DHCP] Renew failed");  break;
    case 2: Serial.print("[DHCP] Renewed — IP: ");
            Serial.println(Ethernet.localIP());      break;
    case 3: Serial.println("[DHCP] Rebind failed"); break;
    case 4: Serial.print("[DHCP] Rebound — IP: ");
            Serial.println(Ethernet.localIP());      break;
  }
}

// ================== NETWORK HANDLER ==================
void processNetwork() {
  if (clientActive) {
    if (!activeClient.connected()) {
      activeClient.stop();
      clientActive = false;
      Serial.println("[TCP] Client disconnected");
    } else {
      modbusTCPServer.poll();
    }
  } else {
    EthernetClient newClient = ethServer.available();
    if (newClient) {
      activeClient = newClient;
      modbusTCPServer.accept(activeClient);
      clientActive = true;
      Serial.println("[TCP] Client connected");
    }
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN,  OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(LED_PIN,  LOW);
  digitalWrite(PUMP_PIN, LOW);

  // ---------- RTU ----------
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  mbRTU.begin(&Serial2);
  mbRTU.slave(1);

  mbRTU.addCoil(0, coilLed);
  mbRTU.addCoil(1, coilPump);
  mbRTU.addHreg(0, setTemp);
  mbRTU.addHreg(1, setSpeed);
  mbRTU.addIreg(0, actualTemp);
  mbRTU.addIreg(1, voltage);
  mbRTU.addIreg(2, counterVal);

  // ---------- TCP + DHCP ----------
  SPI.begin(18, 19, 23, 5);
  Ethernet.init(5);

  Serial.println("Requesting DHCP...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("[DHCP] Failed — check router/cable");
    while (1);
  }

  delay(1000);

  Serial.print("IP:      "); Serial.println(Ethernet.localIP());
  Serial.print("Gateway: "); Serial.println(Ethernet.gatewayIP());
  Serial.print("Subnet:  "); Serial.println(Ethernet.subnetMask());
  Serial.print("Link:    "); Serial.println(Ethernet.linkStatus());

  ethServer.begin();

  if (!modbusTCPServer.begin()) {
    Serial.println("[TCP] Server init failed!");
    while (1);
  }

  modbusTCPServer.configureCoils(0, 2);
  modbusTCPServer.configureHoldingRegisters(0, 2);
  modbusTCPServer.configureInputRegisters(0, 3);

  Serial.println("TCP + RTU Bridge Ready (DHCP)");
}

// ================== LOOP ==================
void loop() {

  // RTU task must run every iteration — no delays before this
  mbRTU.task();

  unsigned long now = millis();
  if (now - lastLoopTime < LOOP_INTERVAL_MS) return;
  lastLoopTime = now;

  maintainDHCP();
  processNetwork();

  syncFromRTU();
  syncFromTCP();

  applyHardware();
  updateSimulatedMetrics();

  syncToRTU();
  syncToTCP();
}