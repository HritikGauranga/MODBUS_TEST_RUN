// #include <SPI.h>
// #include <Ethernet.h>
// #include <ArduinoModbus.h>
// #include <ModbusRTU.h>

// // ---------- RTU setup ----------
// ModbusRTU mbRTU;
// #define RXD2 16
// #define TXD2 17

// // ---------- TCP setup ----------
// byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE};
// IPAddress ip(192, 168, 8, 177);
// EthernetServer ethServer(502);
// ModbusTCPServer modbusTCPServer;

// // ---------- Common data ----------
// #define LED_PIN 2
// #define PUMP_PIN 4

// bool coilLed = false;
// bool coilPump = false;
// uint16_t setTemp = 20;
// uint16_t setSpeed = 100;
// uint16_t actualTemp = 0;
// uint16_t voltage = 230;
// uint16_t counterVal = 0;

// void applyHardware() {
//   digitalWrite(LED_PIN, coilLed ? HIGH : LOW);
//   digitalWrite(PUMP_PIN, coilPump ? HIGH : LOW);
// }

// void syncFromRTU() {
//   coilLed = mbRTU.Coil(0);
//   coilPump = mbRTU.Coil(1);
//   setTemp = mbRTU.Hreg(0);
//   setSpeed = mbRTU.Hreg(1);
// }

// void syncToRTU() {
//   mbRTU.Coil(0, coilLed);
//   mbRTU.Coil(1, coilPump);
//   mbRTU.Hreg(0, setTemp);
//   mbRTU.Hreg(1, setSpeed);
// }

// void syncFromTCP() {
//   if (!modbusTCPServer.connected()) return;
//   coilLed = modbusTCPServer.coilRead(0);
//   coilPump = modbusTCPServer.coilRead(1);
//   setTemp = modbusTCPServer.holdingRegisterRead(0);
//   setSpeed = modbusTCPServer.holdingRegisterRead(1);
// }

// void syncToTCP() {
//   if (!modbusTCPServer.connected()) return;

//   modbusTCPServer.coilWrite(0, coilLed);
//   modbusTCPServer.coilWrite(1, coilPump);
//   modbusTCPServer.holdingRegisterWrite(0, setTemp);
//   modbusTCPServer.holdingRegisterWrite(1, setSpeed);

//   modbusTCPServer.inputRegisterWrite(0, actualTemp);
//   modbusTCPServer.inputRegisterWrite(1, voltage);
//   modbusTCPServer.inputRegisterWrite(2, counterVal);
// }

// void updateSimulatedMetrics() {
//   counterVal = millis() / 1000;
//   actualTemp = setTemp + (millis() % 5);
//   voltage = 220 + (millis() % 10);
// }

// void setup() {
//   Serial.begin(115200);
//   while (!Serial);

//   pinMode(LED_PIN, OUTPUT);
//   pinMode(PUMP_PIN, OUTPUT);
//   digitalWrite(LED_PIN, LOW);
//   digitalWrite(PUMP_PIN, LOW);
//   Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
//   mbRTU.begin(&Serial2);
//   mbRTU.slave(1);

//   mbRTU.addHreg(0, setTemp);
//   mbRTU.addHreg(1, setSpeed);
//   mbRTU.addCoil(0, coilLed);
//   mbRTU.addCoil(1, coilPump);

//   SPI.begin(18, 19, 23, 5);
//   Ethernet.init(5);
//   Ethernet.begin(mac, ip);
//   delay(1000);

//   Serial.printf("Ethernet hardware: %d, link: %d\n", Ethernet.hardwareStatus(), Ethernet.linkStatus());
//   Serial.printf("IP: %s\n", Ethernet.localIP().toString().c_str());

//   ethServer.begin();

//   if (!modbusTCPServer.begin()) {
//     Serial.println("Modbus TCP server start failed");
//     while (1) delay(100);
//   }

//   modbusTCPServer.configureHoldingRegisters(0, 2);
//   modbusTCPServer.configureCoils(0, 2);
//   modbusTCPServer.configureInputRegisters(0, 3);

//   applyHardware();
//   Serial.println("✅ TCP+RTU bridge started");
// }

// void processNetwork() {
//   EthernetClient client = ethServer.available();

//   if (!client) return;

//   if (modbusTCPServer.accept(client)) {
//     Serial.println("TCP client connected");

//     while (client.connected()) {
//       mbRTU.task();
//       modbusTCPServer.poll();

//       syncFromRTU();
//       syncFromTCP();
//       applyHardware();
//       updateSimulatedMetrics();
//       syncToRTU();
//       syncToTCP();
      
//       delay(10);
//     }

//     client.stop();
//     Serial.println("TCP client disconnected");
//   }
// }

// void loop() {
//   mbRTU.task();
//   syncFromRTU();
//   processNetwork();

//   updateSimulatedMetrics();
//   applyHardware();
//   syncToRTU();
//   syncToTCP();

//   delay(10);
// }

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoModbus.h>
#include <ModbusRTU.h>

// ================== RTU SETUP ==================
ModbusRTU mbRTU;
#define RXD2 16
#define TXD2 17

// ================== TCP SETUP ==================
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE};
IPAddress ip(192, 168, 8, 177);

EthernetServer ethServer(502);
ModbusTCPServer modbusTCPServer;
EthernetClient activeClient;

// ================== HARDWARE ==================
#define LED_PIN 2
#define PUMP_PIN 4

// ================== SHARED DATA ==================
bool coilLed = false;
bool coilPump = false;

uint16_t setTemp = 20;
uint16_t setSpeed = 100;

uint16_t actualTemp = 0;
uint16_t voltage = 230;
uint16_t counterVal = 0;

// ================== HARDWARE APPLY ==================
void applyHardware() {
  digitalWrite(LED_PIN, coilLed ? HIGH : LOW);
  digitalWrite(PUMP_PIN, coilPump ? HIGH : LOW);
}

// ================== RTU SYNC ==================
void syncFromRTU() {
  coilLed = mbRTU.Coil(0);
  coilPump = mbRTU.Coil(1);
  setTemp = mbRTU.Hreg(0);
  setSpeed = mbRTU.Hreg(1);
}

void syncToRTU() {
  mbRTU.Coil(0, coilLed);
  mbRTU.Coil(1, coilPump);

  mbRTU.Hreg(0, setTemp);
  mbRTU.Hreg(1, setSpeed);

  // 🔥 Added Input Registers
  mbRTU.Ireg(0, actualTemp);
  mbRTU.Ireg(1, voltage);
  mbRTU.Ireg(2, counterVal);
}

// ================== TCP SYNC ==================
void syncFromTCP() {
  // No connected() check needed
  coilLed = modbusTCPServer.coilRead(0);
  coilPump = modbusTCPServer.coilRead(1);

  setTemp = modbusTCPServer.holdingRegisterRead(0);
  setSpeed = modbusTCPServer.holdingRegisterRead(1);
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
  voltage = 220 + (millis() % 10);
}

// ================== NETWORK HANDLER (NON-BLOCKING) ==================
void processNetwork() {

  EthernetClient client = ethServer.available();

  if (client) {
    activeClient = client;
    modbusTCPServer.accept(activeClient);
    Serial.println("🌐 TCP client connected");
  }

  // Always poll (IMPORTANT)
  modbusTCPServer.poll();
}

// ================== SETUP ==================
void setup() {

  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);

  // ---------- RTU ----------
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  mbRTU.begin(&Serial2);
  mbRTU.slave(1);

  mbRTU.addCoil(0, coilLed);
  mbRTU.addCoil(1, coilPump);

  mbRTU.addHreg(0, setTemp);
  mbRTU.addHreg(1, setSpeed);

  // 🔥 Added RTU input registers
  mbRTU.addIreg(0, actualTemp);
  mbRTU.addIreg(1, voltage);
  mbRTU.addIreg(2, counterVal);

  // ---------- TCP ----------
  SPI.begin(18, 19, 23, 5);
  Ethernet.init(5);
  Ethernet.begin(mac, ip);
  delay(1000);

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());

  ethServer.begin();

  if (!modbusTCPServer.begin()) {
    Serial.println("❌ TCP Server Failed");
    while (1);
  }

  modbusTCPServer.configureCoils(0, 2);
  modbusTCPServer.configureHoldingRegisters(0, 2);
  modbusTCPServer.configureInputRegisters(0, 3);

  Serial.println("✅ TCP + RTU Bridge Started");
}

// ================== LOOP ==================
void loop() {

  // 1️⃣ Handle RTU (always fast)
  mbRTU.task();

  // 2️⃣ Handle TCP (non-blocking)
  processNetwork();

  // 3️⃣ Read from both sides
  syncFromRTU();
  syncFromTCP();

  // 4️⃣ Apply outputs
  applyHardware();

  // 5️⃣ Update device data
  updateSimulatedMetrics();

  // 6️⃣ Sync back to both
  syncToRTU();
  syncToTCP();

  delay(5);
}