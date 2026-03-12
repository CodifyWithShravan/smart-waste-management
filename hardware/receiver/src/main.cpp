/*
 * ============================================================
 *  Smart Waste Management — ESP32 LoRa-to-USB Bridge
 *  (Receiver Side — connects LoRa module to Raspberry Pi)
 * ============================================================
 *
 *  Purpose:
 *    This ESP32 sits between the RYLR998 LoRa module and the
 *    Raspberry Pi. It acts as a transparent serial bridge:
 *      LoRa Module ←→ ESP32 ←→ USB ←→ Raspberry Pi
 *
 *    The Pi's gateway.py reads from the USB serial port and
 *    gets the raw "+RCV=..." messages as if the LoRa module
 *    were connected directly.
 *
 *  Hardware Connections:
 *  ┌─────────────────────────────────────────────────────┐
 *  │  REYAX RYLR998 LoRa Module (Serial2)               │
 *  │    TX    →  GPIO 26 (ESP32 RX2)                     │
 *  │    RX    →  GPIO 27 (ESP32 TX2)                     │
 *  │    VCC   →  3.3V                                    │
 *  │    GND   →  GND                                     │
 *  └─────────────────────────────────────────────────────┘
 *  │  ESP32 USB  →  Raspberry Pi USB Port                │
 *  └─────────────────────────────────────────────────────┘
 *
 *  Data Flow:
 *    Sender ESP32 → (LoRa air) → RYLR998 → ESP32 → USB → Pi
 *
 *  The LoRa module outputs messages like:
 *    +RCV=0,25,BIN-001,75,17.4200,78.6560,-45,12
 *
 *  This firmware simply forwards that to the Pi untouched.
 *
 *  LoRa Configuration:
 *    On first boot, this firmware auto-configures the LoRa
 *    module with the correct settings to match the sender.
 *    The onboard LED blinks to show status:
 *      - Fast blink (2Hz) = Configuring LoRa
 *      - Slow blink (every 5s) = Listening, no data yet
 *      - Quick flash on receive = Got a LoRa message
 * ============================================================
 */

#include <Arduino.h>

// ============================================================
//  CONFIGURATION — Must match the SENDER side!
// ============================================================

// LoRa Serial Pins (RYLR998 on Serial2)
// Matched to perfboard wiring: LoRa TX→D33, LoRa RX→D32
#define LORA_RX_PIN     33   // ESP32 RX2 ← LoRa TX
#define LORA_TX_PIN     32   // ESP32 TX2 → LoRa RX
#define LORA_BAUD       115200

// LoRa Radio Parameters — MUST MATCH SENDER
#define LORA_ADDRESS    0             // This module's address
#define LORA_NETWORK_ID 18            // Network ID (must match sender)
#define LORA_BAND       433000000     // Frequency in Hz (433 MHz for India)

// Onboard LED for status indication
#define LED_PIN         2             // ESP32 DevKit onboard LED

// ============================================================
//  GLOBAL STATE
// ============================================================

unsigned long lastActivityTime = 0;
bool ledState = false;

// ============================================================
//  LoRa CONFIGURATION FUNCTIONS
// ============================================================

/**
 * Sends an AT command to the LoRa module and waits for response.
 * Returns the response string.
 */
String sendATCommand(const String& command, unsigned long waitTime = 1000) {
  // Flush any pending data
  while (Serial2.available()) Serial2.read();

  Serial2.println(command);
  delay(waitTime);

  String response = "";
  while (Serial2.available()) {
    response += (char)Serial2.read();
  }
  response.trim();
  return response;
}

/**
 * Configures the LoRa module with the required parameters.
 * Blinks LED fast during configuration.
 */
bool configureLoRa() {
  Serial.println("\n--- Configuring LoRa Module ---");

  // Step 1: Basic AT test
  Serial.print("  Testing connection... ");
  String resp = sendATCommand("AT");
  if (resp.indexOf("+OK") < 0) {
    Serial.println("FAILED ❌");
    Serial.println("  → Check wiring: LoRa TX→GPIO 26, LoRa RX→GPIO 27");
    return false;
  }
  Serial.println("OK ✅");

  // Step 2: Set address
  Serial.print("  Setting address to ");
  Serial.print(LORA_ADDRESS);
  Serial.print("... ");
  resp = sendATCommand("AT+ADDRESS=" + String(LORA_ADDRESS));
  Serial.println(resp.indexOf("+OK") >= 0 ? "OK ✅" : "FAILED ❌");

  // Step 3: Set network ID
  Serial.print("  Setting Network ID to ");
  Serial.print(LORA_NETWORK_ID);
  Serial.print("... ");
  resp = sendATCommand("AT+NETWORKID=" + String(LORA_NETWORK_ID));
  Serial.println(resp.indexOf("+OK") >= 0 ? "OK ✅" : "FAILED ❌");

  // Step 4: Set frequency band
  Serial.print("  Setting frequency to ");
  Serial.print(LORA_BAND);
  Serial.print(" Hz... ");
  resp = sendATCommand("AT+BAND=" + String(LORA_BAND));
  Serial.println(resp.indexOf("+OK") >= 0 ? "OK ✅" : "FAILED ❌");

  // Step 5: Read back configuration to verify
  Serial.println("\n--- Current LoRa Configuration ---");

  resp = sendATCommand("AT+ADDRESS?");
  Serial.println("  Address:    " + resp);

  resp = sendATCommand("AT+NETWORKID?");
  Serial.println("  Network ID: " + resp);

  resp = sendATCommand("AT+BAND?");
  Serial.println("  Frequency:  " + resp);

  resp = sendATCommand("AT+PARAMETER?");
  Serial.println("  Parameters: " + resp);

  Serial.println("--- Configuration Complete ---\n");
  return true;
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
  // USB Serial — connects to Raspberry Pi
  Serial.begin(115200);
  delay(1000);

  // LED for visual feedback
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("=========================================");
  Serial.println("  Smart Waste Management");
  Serial.println("  LoRa-to-USB Bridge (Receiver)");
  Serial.println("=========================================");

  // Initialize LoRa Serial
  Serial2.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(500);

  // Configure the LoRa module (fast blink during config)
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(150);
  }

  bool loraReady = configureLoRa();

  if (loraReady) {
    Serial.println("✅ Bridge is READY — Listening for LoRa messages...");
    Serial.println("   (Messages will appear as raw +RCV=... lines)\n");
    // Solid LED for 1 second to indicate success
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_PIN, LOW);
  } else {
    Serial.println("❌ LoRa module not detected!");
    Serial.println("   Check wiring and restart.\n");
    // Rapid blink to indicate error
    while (true) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(100);
    }
  }

  lastActivityTime = millis();
}

// ============================================================
//  MAIN LOOP — Transparent Serial Bridge
// ============================================================

void loop() {

  // --- LoRa → Raspberry Pi ---
  // Forward everything from the LoRa module to USB (to the Pi)
  while (Serial2.available()) {
    char c = Serial2.read();
    Serial.write(c);

    // Quick LED flash to show data received
    digitalWrite(LED_PIN, HIGH);
    lastActivityTime = millis();
  }

  // --- Raspberry Pi → LoRa ---
  // Forward AT commands from the Pi to the LoRa module
  // (useful for remote configuration or sending test commands)
  while (Serial.available()) {
    char c = Serial.read();
    Serial2.write(c);
  }

  // --- LED Status Indicator ---
  // Turn off LED shortly after receiving data (quick flash effect)
  if (digitalRead(LED_PIN) == HIGH && millis() - lastActivityTime > 50) {
    digitalWrite(LED_PIN, LOW);
  }

  // Slow "heartbeat" blink every 5 seconds when idle
  if (millis() - lastActivityTime > 5000) {
    digitalWrite(LED_PIN, HIGH);
    delay(30);
    digitalWrite(LED_PIN, LOW);
    lastActivityTime = millis();
  }
}
