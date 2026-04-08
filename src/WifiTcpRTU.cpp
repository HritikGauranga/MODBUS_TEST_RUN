//Hritik Nagpure
// TCPRTU_WiFi.cpp - Modbus RTU + TCP over Ethernet (DHCP) + TCP over WiFi
// All three interfaces share the same register space simultaneously.

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <WiFi.h>
#include <ArduinoModbus.h>
#include <ModbusRTU.h>

// ================== RTU SETUP ==================
ModbusRTU mbRTU;
#define RXD2 9  //SD2
#define TXD2 10 //SD3

// ================== ETHERNET TCP SETUP ==================
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE};
EthernetServer ethServer(502); //ethServer(502) - existing Ethernet server 
ModbusTCPServer modbusTCPServer;   // existing Ethernet Modbus server
EthernetClient activeClient; //
bool clientActive = false; //false because we start with no client connected

// ================== WIFI TCP SETUP ==================
const char* ssid     = "Airtel4G";
const char* password = "GaurangA@2025#";
WiFiServer wifiServer(502);
ModbusTCPServer wifiModbus;        // separate WiFi Modbus server
WiFiClient activeWiFiClient;
bool wifiClientActive = false;
unsigned long lastWiFiReconnect = 0;
#define WIFI_RECONNECT_INTERVAL_MS 5000  // attempt reconnect every 5 seconds, non-blocking

// ================== HARDWARE ==================
#define LED_PIN  2
#define PUMP_PIN 15
#define LED_PIN2 16

// ================== SOURCE TRACKING ==================
typedef enum { SRC_NONE, SRC_RTU, SRC_TCP, SRC_WIFI } DataSource;

DataSource srcLed   = SRC_NONE;
DataSource srcPump  = SRC_NONE;
DataSource srcLed2  = SRC_NONE;
DataSource srcTemp  = SRC_NONE;
DataSource srcSpeed = SRC_NONE;

// ================== SHARED DATA ==================
bool     coilLed     = false;
bool     coilPump    = false;
bool     coilLed2    = false;
uint16_t setTemp     = 20;
uint16_t setSpeed    = 100;
uint16_t actualTemp  = 0;
uint16_t voltage     = 230;
uint16_t counterVal  = 0;

// ================== PREVIOUS VALUES — ETHERNET ==================
bool     prevCoilLed_TCP   = false;
bool     prevCoilPump_TCP  = false;
bool     prevCoilLed2_TCP  = false;
uint16_t prevSetTemp_TCP   = 20;
uint16_t prevSetSpeed_TCP  = 100;

// ================== PREVIOUS VALUES — RTU ==================
bool     prevCoilLed_RTU   = false;
bool     prevCoilPump_RTU  = false;
bool     prevCoilLed2_RTU  = false;
uint16_t prevSetTemp_RTU   = 20;
uint16_t prevSetSpeed_RTU  = 100;

// ================== PREVIOUS VALUES — WIFI ==================
bool     prevCoilLed_WIFI   = false;
bool     prevCoilPump_WIFI  = false;
bool     prevCoilLed2_WIFI  = false;
uint16_t prevSetTemp_WIFI   = 20;
uint16_t prevSetSpeed_WIFI  = 100;

// ================== TIMING ==================
#define LOOP_INTERVAL_MS  10
#define DHCP_RENEW_MS     60000
unsigned long lastLoopTime  = 0;
unsigned long lastDHCPCheck = 0;

// ================== HARDWARE APPLY ==================
void applyHardware() {
  digitalWrite(LED_PIN,  coilLed  ? HIGH : LOW);
  digitalWrite(PUMP_PIN, coilPump ? HIGH : LOW);
  digitalWrite(LED_PIN2, coilLed2 ? HIGH : LOW);
}

// ================== SIMULATION ==================
void updateSimulatedMetrics() {
  counterVal = millis() / 1000; // simple uptime counter in seconds
  actualTemp = setTemp + (millis() % 5); // actual temp fluctuates slightly around setTemp
  voltage    = 220 + (millis() % 10); // voltage fluctuates slightly around 220V
}

// ================== RTU SYNC ==================
void syncFromRTU() {
  bool     newLed   = mbRTU.Coil(0);
  bool     newPump  = mbRTU.Coil(1);
  bool     newLed2  = mbRTU.Coil(2);
  uint16_t newTemp  = mbRTU.Hreg(0);
  uint16_t newSpeed = mbRTU.Hreg(1);

  if (newLed != prevCoilLed_RTU) { //
    coilLed            = newLed;
    prevCoilLed_RTU    = newLed;
    prevCoilLed_TCP    = newLed;  // sync all baselines
    prevCoilLed_WIFI   = newLed;
    srcLed = SRC_RTU; //srcLed = SRC_RTU because the change came from RTU, this will help us identify the source of truth for this data point and prevent loops
    // Immediately update other servers to prevent false change detection
    modbusTCPServer.coilWrite(0, coilLed);
    wifiModbus.coilWrite(0, coilLed); //Here if we change the value on RTU coil 0, if On it will store at coilLed variable and then it will update the coil 0 of TCP and WiFi server with the new value, this way we keep all servers in sync and prevent loops because when TCP and WiFi servers get updated, they will see that the value is same as previous and won't trigger another update back to RTU
    Serial.printf("[RTU] LED -> %s\n", coilLed ? "ON" : "OFF");
  }
  if (newPump != prevCoilPump_RTU) {
    coilPump           = newPump;
    prevCoilPump_RTU   = newPump;
    prevCoilPump_TCP   = newPump;
    prevCoilPump_WIFI  = newPump;
    srcPump = SRC_RTU;
    // Immediately update other servers
    modbusTCPServer.coilWrite(1, coilPump);
    wifiModbus.coilWrite(1, coilPump);
    Serial.printf("[RTU] Pump -> %s\n", coilPump ? "ON" : "OFF");
  }
  if (newLed2 != prevCoilLed2_RTU) {
    coilLed2           = newLed2;
    prevCoilLed2_RTU   = newLed2;
    prevCoilLed2_TCP   = newLed2;
    prevCoilLed2_WIFI  = newLed2;
    srcLed2 = SRC_RTU;
    // Immediately update other servers
    modbusTCPServer.coilWrite(2, coilLed2);
    wifiModbus.coilWrite(2, coilLed2);
    Serial.printf("[RTU] LED2 -> %s\n", coilLed2 ? "ON" : "OFF");
  }
  if (newTemp != prevSetTemp_RTU) {
    setTemp            = newTemp;
    prevSetTemp_RTU    = newTemp;
    prevSetTemp_TCP    = newTemp;
    prevSetTemp_WIFI   = newTemp;
    srcTemp = SRC_RTU;
    // Immediately update other servers
    modbusTCPServer.holdingRegisterWrite(0, setTemp);
    wifiModbus.holdingRegisterWrite(0, setTemp);
    Serial.printf("[RTU] SetTemp -> %d\n", setTemp);
  }
  if (newSpeed != prevSetSpeed_RTU) {
    setSpeed           = newSpeed;
    prevSetSpeed_RTU   = newSpeed;
    prevSetSpeed_TCP   = newSpeed;
    prevSetSpeed_WIFI  = newSpeed;
    srcSpeed = SRC_RTU;
    // Immediately update other servers
    modbusTCPServer.holdingRegisterWrite(1, setSpeed);
    wifiModbus.holdingRegisterWrite(1, setSpeed);
    Serial.printf("[RTU] SetSpeed -> %d\n", setSpeed);
  }
}

void syncToRTU() {
  mbRTU.Coil(0, coilLed);
  mbRTU.Coil(1, coilPump);
  mbRTU.Coil(2, coilLed2);
  mbRTU.Hreg(0, setTemp);
  mbRTU.Hreg(1, setSpeed);
  mbRTU.Ireg(0, actualTemp);
  mbRTU.Ireg(1, voltage);
  mbRTU.Ireg(2, counterVal);
}

// ================== ETHERNET TCP SYNC ==================
void syncFromTCP() {
  bool     newLed   = modbusTCPServer.coilRead(0);
  bool     newPump  = modbusTCPServer.coilRead(1);
  bool     newLed2  = modbusTCPServer.coilRead(2);
  uint16_t newTemp  = modbusTCPServer.holdingRegisterRead(0);
  uint16_t newSpeed = modbusTCPServer.holdingRegisterRead(1);

  if (newLed != prevCoilLed_TCP) {
    coilLed           = newLed;
    prevCoilLed_TCP   = newLed;
    prevCoilLed_RTU   = newLed;  // sync all baselines
    prevCoilLed_WIFI  = newLed;
    srcLed = SRC_TCP;
    // Immediately update other servers to prevent false change detection
    mbRTU.Coil(0, coilLed);
    wifiModbus.coilWrite(0, coilLed);
    Serial.printf("[ETH] LED -> %s\n", coilLed ? "ON" : "OFF");
  }
  if (newPump != prevCoilPump_TCP) {
    coilPump          = newPump;
    prevCoilPump_TCP  = newPump;
    prevCoilPump_RTU  = newPump;
    prevCoilPump_WIFI = newPump;
    srcPump = SRC_TCP;
    // Immediately update other servers
    mbRTU.Coil(1, coilPump);
    wifiModbus.coilWrite(1, coilPump);
    Serial.printf("[ETH] Pump -> %s\n", coilPump ? "ON" : "OFF");
  }
  if (newLed2 != prevCoilLed2_TCP) {
    coilLed2          = newLed2;
    prevCoilLed2_TCP  = newLed2;
    prevCoilLed2_RTU  = newLed2;
    prevCoilLed2_WIFI = newLed2;
    srcLed2 = SRC_TCP;
    // Immediately update other servers
    mbRTU.Coil(2, coilLed2);
    wifiModbus.coilWrite(2, coilLed2);
    Serial.printf("[ETH] LED2 -> %s\n", coilLed2 ? "ON" : "OFF");
  }
  if (newTemp != prevSetTemp_TCP) {
    setTemp           = newTemp;
    prevSetTemp_TCP   = newTemp;
    prevSetTemp_RTU   = newTemp;
    prevSetTemp_WIFI  = newTemp;
    srcTemp = SRC_TCP;
    // Immediately update other servers
    mbRTU.Hreg(0, setTemp);
    wifiModbus.holdingRegisterWrite(0, setTemp);
    Serial.printf("[ETH] SetTemp -> %d\n", setTemp);
  }
  if (newSpeed != prevSetSpeed_TCP) {
    setSpeed          = newSpeed;
    prevSetSpeed_TCP  = newSpeed;
    prevSetSpeed_RTU  = newSpeed;
    prevSetSpeed_WIFI = newSpeed;
    srcSpeed = SRC_TCP;
    // Immediately update other servers
    mbRTU.Hreg(1, setSpeed);
    wifiModbus.holdingRegisterWrite(1, setSpeed);
    Serial.printf("[ETH] SetSpeed -> %d\n", setSpeed);
  }
}

void syncToTCP() {
  modbusTCPServer.coilWrite(0, coilLed);
  modbusTCPServer.coilWrite(1, coilPump);
  modbusTCPServer.coilWrite(2, coilLed2);
  modbusTCPServer.holdingRegisterWrite(0, setTemp);
  modbusTCPServer.holdingRegisterWrite(1, setSpeed);
  modbusTCPServer.inputRegisterWrite(0, actualTemp);
  modbusTCPServer.inputRegisterWrite(1, voltage);
  modbusTCPServer.inputRegisterWrite(2, counterVal);
}

// ================== WIFI TCP SYNC ==================
void syncFromWiFi() {
  bool     newLed   = wifiModbus.coilRead(0);
  bool     newPump  = wifiModbus.coilRead(1);
  bool     newLed2  = wifiModbus.coilRead(2);
  uint16_t newTemp  = wifiModbus.holdingRegisterRead(0);
  uint16_t newSpeed = wifiModbus.holdingRegisterRead(1);

  if (newLed != prevCoilLed_WIFI) {
    coilLed           = newLed;
    prevCoilLed_WIFI  = newLed;
    prevCoilLed_RTU   = newLed;  // sync all baselines
    prevCoilLed_TCP   = newLed;
    srcLed = SRC_WIFI;
    // Immediately update other servers to prevent false change detection
    mbRTU.Coil(0, coilLed);
    modbusTCPServer.coilWrite(0, coilLed);
    Serial.printf("[WiFi] LED -> %s\n", coilLed ? "ON" : "OFF");
  }
  if (newPump != prevCoilPump_WIFI) {
    coilPump          = newPump;
    prevCoilPump_WIFI = newPump;
    prevCoilPump_RTU  = newPump;
    prevCoilPump_TCP  = newPump;
    srcPump = SRC_WIFI;
    // Immediately update other servers
    mbRTU.Coil(1, coilPump);
    modbusTCPServer.coilWrite(1, coilPump);
    Serial.printf("[WiFi] Pump -> %s\n", coilPump ? "ON" : "OFF");
  }
  if (newLed2 != prevCoilLed2_WIFI) {
    coilLed2          = newLed2;
    prevCoilLed2_WIFI = newLed2;
    prevCoilLed2_RTU  = newLed2;
    prevCoilLed2_TCP  = newLed2;
    srcLed2 = SRC_WIFI;
    // Immediately update other servers
    mbRTU.Coil(2, coilLed2);
    modbusTCPServer.coilWrite(2, coilLed2);
    Serial.printf("[WiFi] LED2 -> %s\n", coilLed2 ? "ON" : "OFF");
  }
  if (newTemp != prevSetTemp_WIFI) {
    setTemp           = newTemp;
    prevSetTemp_WIFI  = newTemp;
    prevSetTemp_RTU   = newTemp;
    prevSetTemp_TCP   = newTemp;
    srcTemp = SRC_WIFI;
    // Immediately update other servers
    mbRTU.Hreg(0, setTemp);
    modbusTCPServer.holdingRegisterWrite(0, setTemp);
    Serial.printf("[WiFi] SetTemp -> %d\n", setTemp);
  }
  if (newSpeed != prevSetSpeed_WIFI) {
    setSpeed          = newSpeed;
    prevSetSpeed_WIFI = newSpeed;
    prevSetSpeed_RTU  = newSpeed;
    prevSetSpeed_TCP  = newSpeed;
    srcSpeed = SRC_WIFI;
    // Immediately update other servers
    mbRTU.Hreg(1, setSpeed);
    modbusTCPServer.holdingRegisterWrite(1, setSpeed);
    Serial.printf("[WiFi] SetSpeed -> %d\n", setSpeed);
  }
}

void syncToWiFi() {
  wifiModbus.coilWrite(0, coilLed);
  wifiModbus.coilWrite(1, coilPump);
  wifiModbus.coilWrite(2, coilLed2);
  wifiModbus.holdingRegisterWrite(0, setTemp);
  wifiModbus.holdingRegisterWrite(1, setSpeed);
  wifiModbus.inputRegisterWrite(0, actualTemp);
  wifiModbus.inputRegisterWrite(1, voltage);
  wifiModbus.inputRegisterWrite(2, counterVal);
}

// ================== DHCP MAINTAIN ==================
void maintainDHCP() {
  unsigned long now = millis();
  if (now - lastDHCPCheck < DHCP_RENEW_MS) return; //this 
  lastDHCPCheck = now;

  int result = Ethernet.maintain(); //
  switch (result) {
    case 0: break;
    case 1: Serial.println("[DHCP] Renew failed");  break;
    case 2: Serial.print("[DHCP] Renewed — IP: ");
            Serial.println(Ethernet.localIP());      break;
    case 3: Serial.println("[DHCP] Rebind failed"); break;
    case 4: Serial.print("[DHCP] Rebound — IP: ");
            Serial.println(Ethernet.localIP());      break;
  }
}

// ================== ETHERNET NETWORK HANDLER ==================
void processNetwork() {
  if (clientActive) {
    if (!activeClient.connected()) {
      activeClient.stop();
      clientActive = false;
      Serial.println("[ETH] Client disconnected");
    } else {
      modbusTCPServer.poll();
    }
  } else {
    EthernetClient newClient = ethServer.available();
    if (newClient) {
      activeClient = newClient;
      modbusTCPServer.accept(activeClient);
      clientActive = true;
      Serial.println("[ETH] Client connected");
    }
  }
}

// ================== WIFI NETWORK HANDLER ==================
void processWiFi() {
  // Non-blocking WiFi status check with reconnection attempt
  static bool wasConnected = false;
  bool isConnected = (WiFi.status() == WL_CONNECTED);
  
  if (!isConnected) {
    unsigned long now = millis();
    if (now - lastWiFiReconnect >= WIFI_RECONNECT_INTERVAL_MS) {
      lastWiFiReconnect = now;
      if (wasConnected) {
        Serial.println("[WiFi] Dropped — reconnecting...");
      }
      WiFi.begin(ssid, password);
    }
    wasConnected = false;
    return;  // Exit early if WiFi not connected
  }
  
  // WiFi just connected or was already connected
  if (!wasConnected) {
    Serial.print("[WiFi] Reconnected — IP: ");
    Serial.println(WiFi.localIP());
    wasConnected = true;
  }

  if (wifiClientActive) {
    if (!activeWiFiClient.connected()) {
      activeWiFiClient.stop();
      wifiClientActive = false;
      Serial.println("[WiFi] Client disconnected");
    } else {
      wifiModbus.poll();
    }
  } else {
    WiFiClient newClient = wifiServer.available();
    if (newClient) {
      activeWiFiClient = newClient;
      wifiModbus.accept(activeWiFiClient);
      wifiClientActive = true;
      Serial.println("[WiFi] Client connected");
    }
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN,  OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  digitalWrite(LED_PIN,  LOW);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(LED_PIN2, LOW);

  // ---------- RTU ----------
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  mbRTU.begin(&Serial2);
  mbRTU.slave(1);

  mbRTU.addCoil(0, coilLed);
  mbRTU.addCoil(1, coilPump);
  mbRTU.addCoil(2, coilLed2);
  mbRTU.addHreg(0, setTemp);
  mbRTU.addHreg(1, setSpeed);
  mbRTU.addIreg(0, actualTemp);
  mbRTU.addIreg(1, voltage);
  mbRTU.addIreg(2, counterVal);

  // ---------- ETHERNET + DHCP ----------
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
    Serial.println("[ETH] Modbus init failed!");
    while (1);
  }
  modbusTCPServer.configureCoils(0, 3);
  modbusTCPServer.configureHoldingRegisters(0, 2);
  modbusTCPServer.configureInputRegisters(0, 3);

  // Initialize Ethernet Modbus registers with current values
  modbusTCPServer.coilWrite(0, coilLed);
  modbusTCPServer.coilWrite(1, coilPump);
  modbusTCPServer.coilWrite(2, coilLed2);
  modbusTCPServer.holdingRegisterWrite(0, setTemp);
  modbusTCPServer.holdingRegisterWrite(1, setSpeed);
  modbusTCPServer.inputRegisterWrite(0, actualTemp);
  modbusTCPServer.inputRegisterWrite(1, voltage);
  modbusTCPServer.inputRegisterWrite(2, counterVal);

  // ---------- WIFI ----------
  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Connecting");
  unsigned long wifiStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartTime < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] Connection timeout — WiFi disabled, proceeding with ETH+RTU");
  }

  wifiServer.begin();
  if (!wifiModbus.begin()) {
    Serial.println("[WiFi] Modbus init failed!");
    while (1);
  }
  wifiModbus.configureCoils(0, 3);
  wifiModbus.configureHoldingRegisters(0, 2);
  wifiModbus.configureInputRegisters(0, 3);

  // Initialize WiFi Modbus registers with current values
  wifiModbus.coilWrite(0, coilLed);
  wifiModbus.coilWrite(1, coilPump);
  wifiModbus.coilWrite(2, coilLed2);
  wifiModbus.holdingRegisterWrite(0, setTemp);
  wifiModbus.holdingRegisterWrite(1, setSpeed);
  wifiModbus.inputRegisterWrite(0, actualTemp);
  wifiModbus.inputRegisterWrite(1, voltage);
  wifiModbus.inputRegisterWrite(2, counterVal);

  Serial.println("RTU + Ethernet + WiFi Bridge Ready");
}

// ================== LOOP ==================
void loop() {

  // RTU must run every iteration — no gate
  mbRTU.task();

  unsigned long now = millis();
  if (now - lastLoopTime < LOOP_INTERVAL_MS) return;
  lastLoopTime = now;

  maintainDHCP();
  processNetwork();
  processWiFi();

  syncFromRTU();
  syncFromTCP();
  syncFromWiFi();

  applyHardware();
  updateSimulatedMetrics();

  syncToRTU();
  syncToTCP();
  syncToWiFi();
}