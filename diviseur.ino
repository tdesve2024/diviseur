/**
 * Diviseur - Application Arduino Nano ESP32
 *
 * Board: Arduino Nano ESP32 (arduino:esp32:nano_nora)
 * Core: Arduino ESP32 Core
 */

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("=== Diviseur - Arduino Nano ESP32 ===");
  Serial.println("Initialisation...");
}

void loop() {
  // TODO: Implémenter la logique du diviseur
}
