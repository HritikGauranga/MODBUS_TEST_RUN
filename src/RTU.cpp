#include <Arduino.h> 
#include <ModbusRTU.h>

ModbusRTU mb;

// UART pins
#define RXD2 9 //SD2
#define TXD2 10 //SD3

// LED
#define LED_PIN 2

// Registers
uint16_t reg0 = 0;
uint16_t reg1 = 100;

// Coil variable
bool ledState = false;

void setup() {

  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  mb.begin(&Serial2);
  mb.slave(1);

  // Holding Registers
  mb.addHreg(0, reg0);
  mb.addHreg(1, reg1);

  // 🔥 ADD COIL (IMPORTANT)
  mb.addCoil(0, ledState);

  Serial.println("✅ Modbus RTU Slave Started (LED Ready)");
}

void loop() {

  // Modbus engine
  mb.task();

  // 🔥 READ COIL VALUE (from Modbus master)
  ledState = mb.Coil(0);

  // Apply to hardware
  digitalWrite(LED_PIN, ledState);

  // Debug (print only when changed)
  static bool prev = -1;
  if (ledState != prev) {
    Serial.print("LED: ");
    Serial.println(ledState ? "ON" : "OFF");
    prev = ledState;
  }

  // Optional: update registers
  mb.Hreg(0, reg0++);
  mb.Hreg(1, reg1++);

  delay(10);
}