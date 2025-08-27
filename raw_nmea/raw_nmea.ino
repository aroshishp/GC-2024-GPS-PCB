#include <HardwareSerial.h>

HardwareSerial GPS(2);  // Use UART2 on ESP32

void setup() {
    Serial.begin(115200);    // Serial monitor
    GPS.begin(9600, SERIAL_8N1, 27, 32);  // GPS module at 9600 baud (TX=16, RX=17)
}

void loop() {

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input == "restart") {
      delay(100);
      ESP.restart();
    }
  }
    while (GPS.available()) {
        char c = GPS.read();
        Serial.print(c);  // Print raw NMEA sentence to Serial Monitor
        // Serial.println("\n\n\n\n");
    }
}
