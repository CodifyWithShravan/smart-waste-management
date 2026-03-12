/*
 * ============================================================
 *  Smart Waste Management System — ESP32 Edge Node Firmware
 * ============================================================
 *
 *  Hardware Connections:
 *  ┌─────────────────────────────────────────────────────┐
 *  │  AJ-SR04 Ultrasonic Sensor                         │
 *  │    TRIG  →  GPIO 5                                  │
 *  │    ECHO  →  GPIO 18                                 │
 *  │    VCC   →  5V                                      │
 *  │    GND   →  GND                                     │
 *  ├─────────────────────────────────────────────────────┤
 *  │  NEO-6M GPS Module (Serial1)                       │
 *  │    TX    →  GPIO 16 (ESP32 RX1)                     │
 *  │    RX    →  GPIO 17 (ESP32 TX1)                     │
 *  │    VCC   →  3.3V                                    │
 *  │    GND   →  GND                                     │
 *  ├─────────────────────────────────────────────────────┤
 *  │  REYAX RYLR998 LoRa Module (Serial2)               │
 *  │    TX    →  GPIO 26 (ESP32 RX2)                     │
 *  │    RX    →  GPIO 27 (ESP32 TX2)                     │
 *  │    VCC   →  3.3V                                    │
 *  │    GND   →  GND                                     │
 *  └─────────────────────────────────────────────────────┘
 *
 *  Data Flow:
 *    Sensors → ESP32 → LoRa TX → (air) → LoRa RX → Raspberry Pi
 *
 *  LoRa Payload Format (CSV):
 *    BIN-001,75,17.4200,78.6560
 *    (binId, fillLevel%, latitude, longitude)
 */

#include <Arduino.h>
#include <TinyGPSPlus.h>

// ============================================================
//  CONFIGURATION — Change these to match your setup
// ============================================================

// Unique ID for this bin (change for each physical bin)
#define BIN_ID          "BIN-003"

// Ultrasonic Sensor Pins (AJ-SR04)
#define TRIG_PIN        5
#define ECHO_PIN        18

// Bin physical dimensions (in centimeters)
// AJ-SR04 effective range up to ~300 cm
#define BIN_DEPTH_CM    300.0

// GPS Serial (NEO-6M on Serial1)
#define GPS_RX_PIN      16   // ESP32 RX1 ← GPS TX
#define GPS_TX_PIN      17   // ESP32 TX1 → GPS RX
#define GPS_BAUD        9600

// LoRa Serial (RYLR998 on Serial2)
// NOTE: Pins swapped to match perfboard wiring
#define LORA_RX_PIN     27   // ESP32 RX2 ← LoRa TX
#define LORA_TX_PIN     26   // ESP32 TX2 → LoRa RX
#define LORA_BAUD       115200

// LoRa Radio Parameters — MUST MATCH RECEIVER SIDE!
#define LORA_ADDRESS    1             // This sender's address
#define LORA_TARGET_ADDR 0            // Receiver's address
#define LORA_NETWORK_ID 18            // Network ID (must match receiver)
#define LORA_BAND       433000000     // Frequency in Hz (433 MHz)

// How often to send data (in milliseconds)
#define SEND_INTERVAL   30000  // 30 seconds

// Number of ultrasonic readings to average (reduces noise)
#define NUM_READINGS    5

// Onboard LED for status
#define LED_PIN         2

// ============================================================
//  GLOBAL OBJECTS
// ============================================================

TinyGPSPlus gps;

// Store last known GPS coordinates (GPS may take time to get a fix)
double lastLatitude  = 0.0;
double lastLongitude = 0.0;
bool   gpsFixObtained = false;

unsigned long lastSendTime = 0;

// ============================================================
//  ULTRASONIC SENSOR FUNCTIONS
// ============================================================

/**
 * Takes a single distance measurement from the AJ-SR04 sensor.
 * Returns distance in centimeters, or -1.0 if no echo received.
 */
float measureDistanceCM() {
  // Send a 10µs HIGH pulse to trigger the sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read the echo pulse duration (timeout after 30ms = ~500cm max)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1.0;  // No echo received
  }

  // Speed of sound = 343 m/s = 0.0343 cm/µs
  // Distance = (duration * 0.0343) / 2 (round trip)
  float distance = (duration * 0.0343) / 2.0;
  return distance;
}

/**
 * Takes multiple readings and returns the averaged distance.
 * Ignores failed readings (-1).
 */
float getAverageDistance() {
  float total = 0;
  int   validReadings = 0;

  for (int i = 0; i < NUM_READINGS; i++) {
    float d = measureDistanceCM();
    if (d > 0) {
      total += d;
      validReadings++;
    }
    delay(50);  // Small delay between readings
  }

  if (validReadings == 0) {
    return -1.0;  // Sensor error
  }

  return total / validReadings;
}

/**
 * Converts distance (cm) to fill level percentage.
 *   - Sensor at top: small distance = full, large distance = empty
 *   - 0% = bin is empty (distance ≈ BIN_DEPTH_CM)
 *   - 100% = bin is full (distance ≈ 0 cm)
 */
int calculateFillLevel(float distanceCM) {
  if (distanceCM < 0) {
    return -1;  // Sensor error
  }

  // Clamp distance to valid range
  if (distanceCM > BIN_DEPTH_CM) distanceCM = BIN_DEPTH_CM;
  if (distanceCM < 2.0) distanceCM = 2.0;  // Minimum sensor range

  float fillPercent = ((BIN_DEPTH_CM - distanceCM) / BIN_DEPTH_CM) * 100.0;

  // Clamp to 0-100 range
  if (fillPercent < 0) fillPercent = 0;
  if (fillPercent > 100) fillPercent = 100;

  return (int)fillPercent;
}

// ============================================================
//  GPS FUNCTIONS
// ============================================================

/**
 * Reads available GPS data from Serial1 and feeds it to TinyGPSPlus.
 * Updates lastLatitude/lastLongitude when a valid fix is obtained.
 */
void updateGPS() {
  while (Serial1.available() > 0) {
    char c = Serial1.read();
    gps.encode(c);
  }

  if (gps.location.isValid() && gps.location.isUpdated()) {
    lastLatitude  = gps.location.lat();
    lastLongitude = gps.location.lng();
    gpsFixObtained = true;
  }
}

// ============================================================
//  LoRa FUNCTIONS
// ============================================================

/**
 * Sends a data payload via the RYLR998 LoRa module using AT commands.
 *
 * AT+SEND command format:
 *   AT+SEND=<Address>,<PayloadLength>,<Payload>\r\n
 *
 * Example:
 *   AT+SEND=0,25,BIN-001,75,17.4200,78.6560
 */
void sendLoRaData(const char* payload) {
  int len = strlen(payload);

  // Build the AT+SEND command
  String command = "AT+SEND=";
  command += LORA_TARGET_ADDR;
  command += ",";
  command += len;
  command += ",";
  command += payload;

  Serial2.println(command);

  // Wait for response from LoRa module
  delay(200);
  String response = "";
  while (Serial2.available()) {
    response += (char)Serial2.read();
  }

  // Log the result
  if (response.indexOf("+OK") >= 0) {
    Serial.println("📡 LoRa: Sent successfully");
  } else {
    Serial.print("⚠️  LoRa response: ");
    Serial.println(response);
  }
}

/**
 * Sends an AT command and returns the response.
 */
String sendATCommand(const String& command, unsigned long waitTime = 1000) {
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
 * Tries to detect the LoRa module at a specific baud rate and pin config.
 */
bool tryLoRaConfig(int rxPin, int txPin, long baud) {
  Serial2.end();
  delay(100);
  Serial2.begin(baud, SERIAL_8N1, rxPin, txPin);
  delay(300);

  // Flush any garbage
  while (Serial2.available()) Serial2.read();

  // Send AT command
  Serial2.println("AT");
  delay(500);

  String response = "";
  while (Serial2.available()) {
    response += (char)Serial2.read();
  }

  return (response.indexOf("+OK") >= 0);
}

/**
 * Auto-scans all baud rates and pin combos to find the LoRa module.
 * Returns true if found, and leaves Serial2 configured correctly.
 */
bool autoDetectLoRa() {
  Serial.println("\n--- Auto-Scanning for LoRa Module ---");

  // Baud rates to try (most common for RYLR998)
  long baudRates[] = {115200, 9600, 57600};
  int numBauds = 3;

  // Pin combos to try (user confirmed LoRa TX→D33, RX→D32)
  int pinConfigs[][2] = {
    {33, 32},  // LoRa TX=D33→RX, LoRa RX=D32→TX (most likely!)
    {32, 33},  // Swapped version
    {27, 26},  // Original design pins
    {26, 27},  // Swapped
  };
  int numPinConfigs = 4;

  for (int b = 0; b < numBauds; b++) {
    for (int p = 0; p < numPinConfigs; p++) {
      int rxPin = pinConfigs[p][0];
      int txPin = pinConfigs[p][1];
      long baud = baudRates[b];

      Serial.print("  Trying RX=GPIO");
      Serial.print(rxPin);
      Serial.print(" TX=GPIO");
      Serial.print(txPin);
      Serial.print(" @ ");
      Serial.print(baud);
      Serial.print(" baud... ");

      if (tryLoRaConfig(rxPin, txPin, baud)) {
        Serial.println("FOUND! ✅");
        Serial.print("  → LoRa is on RX=GPIO");
        Serial.print(rxPin);
        Serial.print(", TX=GPIO");
        Serial.print(txPin);
        Serial.print(" at ");
        Serial.print(baud);
        Serial.println(" baud");
        return true;
      } else {
        Serial.println("no response");
      }
    }
  }
  return false;
}

/**
 * Configures the LoRa module after it has been detected.
 */
bool configureLoRa() {
  // Step 1: Auto-detect the module
  if (!autoDetectLoRa()) {
    Serial.println("\n  ❌ LoRa module not found on any pin/baud combo!");
    Serial.println("  → Check that VCC is connected to 3.3V");
    Serial.println("  → Check that GND is connected");
    Serial.println("  → Make sure the module has power\n");
    return false;
  }

  // Step 2: Configure radio parameters
  Serial.println("\n--- Configuring LoRa Radio ---");

  String resp;

  // Set baud to 115200 if it was found at a different rate
  // (Skip this — just use whatever baud it was found at)

  resp = sendATCommand("AT+ADDRESS=" + String(LORA_ADDRESS));
  Serial.println("  Address set to " + String(LORA_ADDRESS));

  resp = sendATCommand("AT+NETWORKID=" + String(LORA_NETWORK_ID));
  Serial.println("  Network ID set to " + String(LORA_NETWORK_ID));

  resp = sendATCommand("AT+BAND=" + String(LORA_BAND));
  Serial.println("  Frequency set to " + String(LORA_BAND) + " Hz");

  Serial.println("--- LoRa Configuration Done ---\n");
  return true;
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
  // USB Serial for debug output
  Serial.begin(115200);
  delay(1000);

  // LED for status
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("=========================================");
  Serial.println("  Smart Waste Management - Edge Node");
  Serial.println("  Bin ID: " BIN_ID);
  Serial.println("=========================================");

  // Configure ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("✅ Ultrasonic sensor initialized");

  // Initialize GPS Serial
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("✅ GPS serial initialized (waiting for fix...)");

  // Auto-detect and configure LoRa module
  if (configureLoRa()) {
    Serial.println("✅ LoRa module ready");
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
  } else {
    Serial.println("⚠️  LoRa module not responding (check wiring)");
  }

  Serial.println("-----------------------------------------");
  Serial.println("Starting sensor readings...\n");
}

// ============================================================
//  MAIN LOOP
// ============================================================

void loop() {
  // Continuously feed GPS data to the parser
  updateGPS();

  // Check if it's time to send data
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime < SEND_INTERVAL && lastSendTime != 0) {
    return;  // Not time yet
  }
  lastSendTime = currentTime;

  // --- Read Ultrasonic Sensor ---
  float distance = getAverageDistance();
  int fillLevel = calculateFillLevel(distance);

  Serial.print("📏 Distance: ");
  if (distance > 0) {
    Serial.print(distance, 1);
    Serial.print(" cm → Fill Level: ");
    Serial.print(fillLevel);
    Serial.println("%");
  } else {
    Serial.println("SENSOR ERROR");
  }

  // --- Read GPS ---
  Serial.print("📍 GPS: ");
  if (gpsFixObtained) {
    Serial.print(lastLatitude, 4);
    Serial.print(", ");
    Serial.println(lastLongitude, 4);
  } else {
    Serial.println("Waiting for fix... (using 0,0)");
  }

  // --- Build Payload ---
  // Format: BIN-001,75,17.4200,78.6560
  char payload[64];
  snprintf(payload, sizeof(payload), "%s,%d,%.4f,%.4f",
    BIN_ID,
    (fillLevel >= 0) ? fillLevel : 0,
    lastLatitude,
    lastLongitude
  );

  Serial.print("📦 Payload: ");
  Serial.println(payload);

  // --- Send via LoRa ---
  sendLoRaData(payload);

  Serial.println("-----------------------------------------\n");
}