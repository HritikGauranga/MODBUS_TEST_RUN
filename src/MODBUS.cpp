
//this is similar to DHCP server code, but instead of creating a DHCP server, we will create a Modbus RTU server that will run on top of the DHCP server.
//We will use the Ethernet library to create a DHCP server and assign IP addresses to the clients that connect to it.
//We will also use the ArduinoModbus library to create a Modbus RTU server that will run on top of the DHCP server.
//If you are using Platformio and have multiple .cpp files and want to upload specific files, you can use the "build_src_filter=+<filename.cpp>" option in the platformio.ini file. This option allows you to specify which source files should be included in the build process.


#include <Arduino.h>
#include <ArduinoRS485.h> // ArduinoModbus depends on the ArduinoRS485 library
#include <ArduinoModbus.h>

constexpr auto baudrate { 19200 };

// Calculate preDelay and postDelay in microseconds as per Modbus RTU Specification
//
// MODBUS over serial line specification and implementation guide V1.02
// Paragraph 2.5.1.1 MODBUS Message RTU Framing
// https://modbus.org/docs/Modbus_over_serial_line_V1_02.pdf
constexpr auto bitduration { 1.f / baudrate };
constexpr auto wordlen { 9.6f }; // try also with 10.0f
constexpr auto preDelayBR { bitduration * wordlen * 3.5f * 1e6 };
constexpr auto postDelayBR { bitduration * wordlen * 3.5f * 1e6 };

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Modbus RTU Client Toggle w/ Parameters");

  RS485.setDelays(preDelayBR, postDelayBR);

  // start the Modbus RTU client in 8E1 mode
  if (!ModbusRTUClient.begin(baudrate, SERIAL_8E1)) {
    Serial.println("Failed to start Modbus RTU Client!");
    while (1);
  }
}

void loop() {
  // for (slave) id 1: write the value of 0x01, to the coil at address 0x00 
  if (!ModbusRTUClient.coilWrite(1, 0x00, 0x01)) {
    Serial.print("Failed to write coil! ");
    Serial.println(ModbusRTUClient.lastError());
  }

  delay(500);

  // for (slave) id 1: write the value of 0x00, to the coil at address 0x00 
  if (!ModbusRTUClient.coilWrite(1, 0x00, 0x00)) {
    Serial.print("Failed to write coil! ");
    Serial.println(ModbusRTUClient.lastError());
  }

  // wait for 0.5 second
  delay(500);

  auto coil = ModbusRTUClient.coilRead(0x02, 0x00);

  if (coil < 0) {
    Serial.print("Failed to read coil: ");
    Serial.println(ModbusRTUClient.lastError());
  } else {
    Serial.print("Coil: ");
      Serial.println(coil);
  }

  // wait for 0.5 second
  delay(500); 
}
