// Hritik Nagpure
// WebMOD + DHCP TCP/RTU Bridge - ESP32 File Manager with Modbus TCP/RTU
// Combines Web File Manager (AP Mode) + DHCP Ethernet + Modbus TCP/RTU Bridge

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoModbus.h>
#include <ModbusRTU.h>

// ================== ACCESS POINT (WEB SERVER) ==================
const char* ap_ssid = "ESP32_FileServer";
const char* ap_password = "12345678";

// ================== RTU SETUP ==================
ModbusRTU mbRTU;
#define RXD2 9  // SD2
#define TXD2 10 // SD3

// ================== TCP SETUP (ETHERNET) ==================
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE};
EthernetServer ethServer(502);
ModbusTCPServer modbusTCPServer;
EthernetClient activeClient;
bool clientActive = false;

// ================== HARDWARE ==================
#define LED_PIN  2
#define PUMP_PIN 15
#define LED_PIN2 16

// ================== SOURCE TRACKING ==================
typedef enum { SRC_NONE, SRC_RTU, SRC_TCP } DataSource;
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

// ================== PREVIOUS VALUES ==================
bool     prevCoilLed_RTU   = false;
bool     prevCoilPump_RTU  = false;
bool     prevCoilLed2_RTU  = false;
uint16_t prevSetTemp_RTU   = 20;
uint16_t prevSetSpeed_RTU  = 100;

bool     prevCoilLed_TCP   = false;
bool     prevCoilPump_TCP  = false;
bool     prevCoilLed2_TCP  = false;

uint16_t prevSetTemp_TCP   = 20;
uint16_t prevSetSpeed_TCP  = 100;

// ================== TIMING ==================
#define LOOP_INTERVAL_MS  10
#define DHCP_RENEW_MS     60000
unsigned long lastLoopTime  = 0;
unsigned long lastDHCPCheck = 0;

String htmlPage();

// ================== FUNCTIONS ==================
// Hardware Apply
void applyHardware() {
  digitalWrite(LED_PIN,  coilLed  ? HIGH : LOW);
  digitalWrite(PUMP_PIN, coilPump ? HIGH : LOW);
  digitalWrite(LED_PIN2, coilLed2 ? HIGH : LOW);
}

// RTU Sync Functions
void syncFromRTU() {
  bool     newLed   = mbRTU.Coil(0);
  bool     newPump  = mbRTU.Coil(1);
  bool     newLed2  = mbRTU.Coil(2);
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
  if (newLed2 != prevCoilLed2_RTU) {
    coilLed2 = newLed2;
    prevCoilLed2_RTU = newLed2;
    srcLed2 = SRC_RTU;
    Serial.printf("[RTU] LED2 -> %s\n", coilLed2 ? "ON" : "OFF");
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
  mbRTU.Coil(2, coilLed2);
  mbRTU.Hreg(0, setTemp);
  mbRTU.Hreg(1, setSpeed);
  mbRTU.Ireg(0, actualTemp);
  mbRTU.Ireg(1, voltage);
  mbRTU.Ireg(2, counterVal);
}

// TCP Sync Functions
void syncFromTCP() {
  bool     newLed   = modbusTCPServer.coilRead(0);
  bool     newPump  = modbusTCPServer.coilRead(1);
  bool     newLed2  = modbusTCPServer.coilRead(2);
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
  if (newLed2 != prevCoilLed2_TCP) {
    coilLed2 = newLed2;
    prevCoilLed2_TCP = newLed2;
    srcLed2 = SRC_TCP;
    Serial.printf("[TCP] LED2 -> %s\n", coilLed2 ? "ON" : "OFF");
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
  modbusTCPServer.coilWrite(2, coilLed2);
  modbusTCPServer.holdingRegisterWrite(0, setTemp);
  modbusTCPServer.holdingRegisterWrite(1, setSpeed);
  modbusTCPServer.inputRegisterWrite(0, actualTemp);
  modbusTCPServer.inputRegisterWrite(1, voltage);
  modbusTCPServer.inputRegisterWrite(2, counterVal);
}

// Update Simulated Metrics
void updateSimulatedMetrics() {
  counterVal = millis() / 1000;
  actualTemp = setTemp + (millis() % 5);
  voltage    = 220 + (millis() % 10);
}

// DHCP Maintenance
void maintainDHCP() {
  unsigned long now = millis();
  if (now - lastDHCPCheck < DHCP_RENEW_MS) return;
  lastDHCPCheck = now;

  int result = Ethernet.maintain();
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

// Network Handler
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

// Async server
AsyncWebServer server(80);

// ================== FILE ==================
static File uploadFile;

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== WebMOD Async File Manager (AP Mode) ===");

  // START ACCESS POINT
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

  Serial.println("Access Point Started!");
  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  //  LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed!");
    return;
  }

  // ================== ROUTES ==================

  //  Dashboard
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", htmlPage());
  });

  //  List files
  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"files\":[";
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    bool first = true;

    while (file) {
      if (!first) json += ",";
      json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
      first = false;
      file = root.openNextFile();
    }

    json += "]}";
    request->send(200, "application/json", json);
  });

  //  Upload (FIXED)
  server.on(
    "/api/upload",
    HTTP_POST,
    [](AsyncWebServerRequest *request) {
      //  Do nothing here
    },
    [](AsyncWebServerRequest *request, String filename, size_t index,
       uint8_t *data, size_t len, bool final) {

      if (!index) {
        Serial.printf("Upload Start: %s\n", filename.c_str());

        if (filename.indexOf("..") != -1 || filename.indexOf("/") != -1) {
          Serial.println("Invalid filename!");
          return;
        }

        uploadFile = LittleFS.open("/" + filename, "w");

        if (!uploadFile) {
          Serial.println("File open failed!");
          return;
        }
      }

      if (len && uploadFile) {
        uploadFile.write(data, len);
      }

      if (final) {
        if (uploadFile) uploadFile.close();

        Serial.printf("Upload Done: %s (%u bytes)\n",
                      filename.c_str(), index + len);

        //  Send response AFTER upload completes
        request->send(200, "application/json", "{\"success\":true}");
      }
    });

  //  Download
  server.on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("filename")) {
      request->send(400, "text/plain", "Missing filename");
      return;
    }

    String filename = request->getParam("filename")->value();

    if (!LittleFS.exists("/" + filename)) {
      request->send(404, "text/plain", "File not found");
      return;
    }

    request->send(LittleFS, "/" + filename, "application/octet-stream", true);
  });

  //  Delete
  server.on("/api/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("filename")) {
      request->send(400, "application/json", "{\"error\":\"No filename\"}");
      return;
    }

    String filename = request->getParam("filename")->value();

    if (LittleFS.remove("/" + filename)) {
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      request->send(500, "application/json", "{\"error\":\"Delete failed\"}");
    }
  });

  server.begin();

  Serial.println("\nServer started!");
  Serial.println("Connect to WiFi: ESP32_FileServer");
  Serial.println("Open browser: http://192.168.4.1");

  // ================== RTU INIT ==================
  delay(500);
  Serial.println("\n=== Initializing RTU (Modbus) ===");
  
  pinMode(LED_PIN,  OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  digitalWrite(LED_PIN,  LOW);
  digitalWrite(PUMP_PIN, LOW);

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

  Serial.println("RTU Initialized!");

  // ================== ETHERNET + TCP INIT ==================
  Serial.println("\n=== Initializing Ethernet (DHCP) ===");
  
  SPI.begin(18, 19, 23, 5);
  Ethernet.init(5);

  Serial.println("Requesting DHCP...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("[DHCP] Failed — check router/cable");
    delay(2000);
  }

  delay(1000);

  Serial.print("IP:      "); Serial.println(Ethernet.localIP());
  Serial.print("Gateway: "); Serial.println(Ethernet.gatewayIP());
  Serial.print("Subnet:  "); Serial.println(Ethernet.subnetMask());
  Serial.print("Link:    "); Serial.println(Ethernet.linkStatus());

  ethServer.begin();

  if (!modbusTCPServer.begin()) {
    Serial.println("[TCP] Server init failed!");
  }

  modbusTCPServer.configureCoils(0, 3);
  modbusTCPServer.configureHoldingRegisters(0, 2);
  modbusTCPServer.configureInputRegisters(0, 3);

  Serial.println("TCP + RTU Bridge Ready!");
  Serial.println("\n=== System Initialize Complete ===\n");
}

// ================== LOOP ==================
void loop() {
  // RTU task must run every iteration — no delays before this
  mbRTU.task();

  unsigned long now = millis();
  if (now - lastLoopTime < LOOP_INTERVAL_MS) return;
  lastLoopTime = now;

  // Network & DHCP tasks
  maintainDHCP();
  processNetwork();

  // Sync data between TCP, RTU, and hardware
  syncFromRTU();
  syncFromTCP();

  // Apply hardware changes
  applyHardware();
  
  // Update simulated metrics
  updateSimulatedMetrics();

  // Sync back to both TCP and RTU
  syncToRTU();
  syncToTCP();
}

// ================== HTML ==================
String htmlPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 File Manager</title>
<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  min-height: 100vh;
  padding: 20px;
  display: flex;
  justify-content: center;
  align-items: center;
}

.container {
  background: white;
  border-radius: 12px;
  box-shadow: 0 10px 40px rgba(0,0,0,0.2);
  max-width: 800px;
  width: 100%;
  overflow: hidden;
}

.header {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
  padding: 30px 20px;
  text-align: center;
}

.header h1 {
  font-size: 28px;
  margin-bottom: 8px;
}

.header p {
  font-size: 14px;
  opacity: 0.9;
}

.content {
  padding: 30px;
}

.upload-section {
  border: 2px dashed #667eea;
  border-radius: 8px;
  padding: 20px;
  text-align: center;
  margin-bottom: 30px;
  background: #f8f9ff;
  transition: all 0.3s ease;
}

.upload-section:hover {
  border-color: #764ba2;
  background: #f0f2ff;
}

.upload-section input[type="file"] {
  display: none;
}

.upload-label {
  display: inline-block;
  padding: 10px 20px;
  background: #667eea;
  color: white;
  border-radius: 6px;
  cursor: pointer;
  transition: background 0.3s ease;
  margin-right: 10px;
}

.upload-label:hover {
  background: #764ba2;
}

.upload-btn {
  padding: 10px 30px;
  background: #28a745;
  color: white;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  font-size: 14px;
  transition: background 0.3s ease;
}

.upload-btn:hover {
  background: #218838;
}

.upload-btn:disabled {
  background: #ccc;
  cursor: not-allowed;
}

.file-name {
  margin-top: 10px;
  font-size: 12px;
  color: #667eea;
}

.files-section h2 {
  color: #333;
  margin-bottom: 20px;
  font-size: 18px;
  border-bottom: 2px solid #667eea;
  padding-bottom: 10px;
}

.file-list {
  list-style: none;
}

.file-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 15px;
  background: #f8f9fa;
  border-radius: 6px;
  margin-bottom: 10px;
  border-left: 4px solid #667eea;
  transition: all 0.3s ease;
}

.file-item:hover {
  background: #e9ecef;
  transform: translateX(5px);
}

.file-info {
  flex: 1;
  text-align: left;
}

.file-name-text {
  font-weight: 600;
  color: #333;
  word-break: break-all;
}

.file-size {
  font-size: 12px;
  color: #999;
  margin-top: 5px;
}

.file-actions {
  display: flex;
  gap: 8px;
}

.btn-action {
  padding: 8px 12px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 12px;
  transition: all 0.3s ease;
  white-space: nowrap;
}

.btn-download {
  background: #17a2b8;
  color: white;
}

.btn-download:hover {
  background: #138496;
}

.btn-delete {
  background: #dc3545;
  color: white;
}

.btn-delete:hover {
  background: #c82333;
}

.empty-state {
  text-align: center;
  padding: 40px 20px;
  color: #999;
}

.empty-state-icon {
  font-size: 48px;
  margin-bottom: 10px;
}

.status-message {
  padding: 12px;
  border-radius: 6px;
  margin-bottom: 15px;
  display: none;
}

.status-message.success {
  background: #d4edda;
  color: #155724;
  border: 1px solid #c3e6cb;
  display: block;
}

.status-message.error {
  background: #f8d7da;
  color: #721c24;
  border: 1px solid #f5c6cb;
  display: block;
}

.loading {
  display: inline-block;
  width: 12px;
  height: 12px;
  border: 2px solid #667eea;
  border-top: 2px solid transparent;
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}

@media (max-width: 600px) {
  body {
    padding: 10px;
    min-height: 100vh;
  }

  .container {
    max-width: 100%;
    border-radius: 8px;
  }

  .header {
    padding: 20px 15px;
  }

  .header h1 {
    font-size: 22px;
    margin-bottom: 5px;
  }

  .header p {
    font-size: 12px;
  }

  .content {
    padding: 15px;
  }

  .upload-section {
    padding: 15px;
    margin-bottom: 20px;
    border-radius: 6px;
  }

  .upload-label {
    display: block;
    width: 100%;
    padding: 12px 15px;
    margin-bottom: 10px;
    margin-right: 0;
    text-align: center;
    border-radius: 6px;
  }

  .upload-btn {
    width: 100%;
    padding: 12px 20px;
    font-size: 14px;
    border-radius: 6px;
  }

  .upload-btn:disabled {
    width: 100%;
  }

  .file-name {
    font-size: 11px;
    margin-top: 8px;
    word-break: break-word;
  }

  .files-section h2 {
    font-size: 16px;
    margin-bottom: 15px;
    padding-bottom: 8px;
  }

  .file-item {
    flex-direction: column;
    align-items: flex-start;
    padding: 12px;
    margin-bottom: 8px;
    border-left-width: 3px;
  }

  .file-info {
    width: 100%;
    margin-bottom: 10px;
  }

  .file-name-text {
    font-size: 14px;
    font-weight: 600;
    word-break: break-all;
  }

  .file-size {
    font-size: 11px;
    margin-top: 4px;
  }

  .file-actions {
    width: 100%;
    gap: 6px;
    display: flex;
  }

  .btn-action {
    flex: 1;
    padding: 10px 8px;
    font-size: 13px;
    border-radius: 4px;
  }

  .status-message {
    padding: 10px;
    font-size: 13px;
    border-radius: 4px;
    margin-bottom: 12px;
  }

  .empty-state {
    padding: 30px 15px;
  }

  .empty-state-icon {
    font-size: 36px;
    margin-bottom: 8px;
  }

  .empty-state p {
    font-size: 13px;
  }
}

@media (max-width: 480px) {
  body {
    padding: 8px;
  }

  .container {
    border-radius: 6px;
  }

  .header {
    padding: 15px 10px;
  }

  .header h1 {
    font-size: 18px;
  }

  .header p {
    font-size: 11px;
  }

  .content {
    padding: 12px;
  }

  .upload-section {
    padding: 12px;
    margin-bottom: 15px;
  }

  .upload-label {
    padding: 10px 12px;
    font-size: 13px;
  }

  .upload-btn {
    padding: 10px 15px;
    font-size: 13px;
  }

  .files-section h2 {
    font-size: 15px;
    margin-bottom: 12px;
  }

  .file-item {
    padding: 10px;
    margin-bottom: 6px;
  }

  .file-name-text {
    font-size: 13px;
  }

  .btn-action {
    padding: 8px 6px;
    font-size: 12px;
  }

  .empty-state {
    padding: 25px 12px;
  }

  .empty-state-icon {
    font-size: 32px;
  }
}
</style>
</head>
<body>

<div class="container">
  <div class="header">
    <h1>📁 ESP32 File Manager</h1>
    <p>Organize and manage files on your ESP32</p>
  </div>

  <div class="content">
    <div class="status-message" id="statusMsg"></div>

    <div class="upload-section">
      <label for="file" class="upload-label">📤 Choose File</label>
      <button class="upload-btn" onclick="uploadFile()">Upload</button>
      <div class="file-name" id="fileName"></div>
    </div>

    <div class="files-section">
      <h2>📋 Recent Files</h2>
      <ul class="file-list" id="list">
        <div class="empty-state">
          <div class="empty-state-icon">📭</div>
          <p>No files yet. Upload one to get started!</p>
        </div>
      </ul>
    </div>

    <input type="file" id="file" onchange="updateFileName()" style="display:none;">
  </div>
</div>

<script>

function updateFileName() {
  const file = document.getElementById("file").files[0];
  const fileNameEl = document.getElementById("fileName");
  if (file) {
    fileNameEl.textContent = "Selected: " + file.name;
  } else {
    fileNameEl.textContent = "";
  }
}

function showStatus(msg, type) {
  const statusEl = document.getElementById("statusMsg");
  statusEl.textContent = msg;
  statusEl.className = "status-message " + type;
  setTimeout(() => {
    statusEl.className = "status-message";
  }, 3000);
}

function uploadFile() {
  let file = document.getElementById("file").files[0];
  
  if (!file) {
    showStatus("Please select a file first", "error");
    return;
  }

  let formData = new FormData();
  formData.append("file", file);

  const btn = event.target;
  btn.disabled = true;
  btn.innerHTML = '<span class="loading"></span> Uploading...';

  fetch("/api/upload", {
    method: "POST",
    body: formData
  })
  .then(r => r.json())
  .then(data => {
    showStatus("File uploaded successfully!", "success");
    document.getElementById("file").value = "";
    document.getElementById("fileName").textContent = "";
    load();
  })
  .catch(e => {
    showStatus("Upload failed: " + e.message, "error");
  })
  .finally(() => {
    btn.disabled = false;
    btn.innerHTML = "Upload";
  });
}

function load() {
  fetch("/api/files")
  .then(r => r.json())
  .then(data => {
    let html = "";
    
    if (data.files.length === 0) {
      html = `<div class="empty-state">
        <div class="empty-state-icon">📭</div>
        <p>No files yet. Upload one to get started!</p>
      </div>`;
    } else {
      data.files.forEach(f => {
        const sizeKB = (f.size / 1024).toFixed(2);
        html += `<li class="file-item">
          <div class="file-info">
            <div class="file-name-text">📄 ${f.name}</div>
            <div class="file-size">${sizeKB} KB</div>
          </div>
          <div class="file-actions">
            <button class="btn-action btn-download" onclick="download('${f.name.replace(/'/g, "\\'")}')">⬇️ Download</button>
            <button class="btn-action btn-delete" onclick="del('${f.name.replace(/'/g, "\\'")}')">🗑️ Delete</button>
          </div>
        </li>`;
      });
    }
    
    document.getElementById("list").innerHTML = html;
  })
  .catch(e => {
    showStatus("Failed to load files: " + e.message, "error");
  });
}

function download(name) {
  window.open("/api/download?filename=" + encodeURIComponent(name));
}

function del(name) {
  if (confirm("Are you sure you want to delete '" + name + "'?")) {
    fetch("/api/delete?filename=" + encodeURIComponent(name), {method: "DELETE"})
    .then(r => r.json())
    .then(data => {
      showStatus("File deleted successfully!", "success");
      load();
    })
    .catch(e => {
      showStatus("Delete failed: " + e.message, "error");
    });
  }
}

// Load files on page load
load();

</script>

</body>
</html>
)rawliteral";
}