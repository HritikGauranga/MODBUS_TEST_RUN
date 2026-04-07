        Modbus RTU (UART)
               │
               ▼
        ┌────────────┐
        │   ESP32    │  ← Shared memory (coils + registers)
        └────────────┘
        ▲            ▲
        │            │
 Modbus TCP      Modbus TCP
 (Ethernet)       (WiFi)