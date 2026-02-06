/*
  ESP32-S3 + 74HC595 Chain Control (28 Shift Registers, 224 Relays)
  + Keypad Control (A,B,C,D Features added)
  + OLED Status Display
*/

#include <Arduino.h>

#ifdef BOARD_C3
    #define BUTTON_A 5
    #define BUTTON_B 6

    #define CLOCK_PIN 2
    #define LATCH_PIN 3
    #define OE_PIN    0
    #define DATA_PIN  1
#elif BOARD_S3
    #define BUTTON_A 28
    #define BUTTON_B 29

    #define CLOCK_PIN 4
    #define LATCH_PIN 5
    #define OE_PIN    6
    #define DATA_PIN  7
#endif

#define NUM_SHIFT_REGISTERS 28
#define NUM_LEDS (NUM_SHIFT_REGISTERS * 8)

// --- Global Variables ---
byte relayStates[NUM_SHIFT_REGISTERS];
String inputBuffer = "";

// Forward declarations for functions
void setAllRelays(bool state);
void runSequence();
void flashAllRelays();
void toggleRelay(int relayId);
void setRelay(int ledId, bool state);
void updateShiftRegisters();

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);

  // Initialize Shift Register Pins
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(OE_PIN, OUTPUT);
  digitalWrite(OE_PIN, HIGH); // Disable outputs at startup

  // Clear all LEDs initially
  setAllRelays(false);

  // Enable outputs after startup init completes
  digitalWrite(OE_PIN, LOW);

  flashAllRelays(); // Run a register sweep once after flash
}

void loop() {
  if (digitalRead(BUTTON_A) == LOW) {
      runSequence();
  } else if (digitalRead(BUTTON_B) == LOW) {
      flashAllRelays();
  }
}

// --- Logic: Special Functions ---

// Helper to set ALL relays ON or OFF at once
void setAllRelays(bool state) {
  byte fillValue = state ? 0xFF : 0x00; // 0xFF is 11111111, 0x00 is 00000000
  for (int i = 0; i < NUM_SHIFT_REGISTERS; i++) {
    relayStates[i] = fillValue;
  }
  updateShiftRegisters();
}

// Run relays sequentially for each shift register
void runSequence() {
  setAllRelays(false);
  for (int bit = 0; bit < 8; bit++) {
    for (int reg = 0; reg < NUM_SHIFT_REGISTERS; reg++) {
      relayStates[reg] |= (1 << bit);
    }
    updateShiftRegisters();
    delay(200);
    for (int reg = 0; reg < NUM_SHIFT_REGISTERS; reg++) {
      relayStates[reg] &= ~(1 << bit);
    }
  }
  setAllRelays(false);
  Serial.println(F("Run sequence Done"));
}

// Flash all relays 3 times
void flashAllRelays() {
  for(int k=0; k<3; k++) {
    setAllRelays(true);
    delay(200);
    setAllRelays(false);
    delay(200);
  }
  Serial.println(F("Flash all Done"));
}

// --- Logic: Relay Control ---

// Toggle a specific relay ID
void toggleRelay(int relayId) {
  if (relayId < 0 || relayId >= NUM_LEDS) {
    Serial.println(F("Error: Invalid ID"));
    return;
  }

  int registerIndex = relayId / 8;
  int bitIndex = relayId % 8;

  bool isOne = (relayStates[registerIndex] & (1 << bitIndex));

  setRelay(relayId, !isOne);
  updateShiftRegisters();

  String statusMsg = "Relay " + String(relayId);
  String stateMsg = !isOne ? "ON" : "OFF";
  Serial.println(statusMsg + " " + stateMsg);
}

// Basic set relay function (updates array only)
void setRelay(int ledId, bool state) {
  if (ledId < 0 || ledId >= NUM_LEDS) return;
  int registerIndex = ledId / 8;
  int bitIndex = ledId % 8;

  if (state) {
    relayStates[registerIndex] |= (1 << bitIndex);
  } else {
    relayStates[registerIndex] &= ~(1 << bitIndex);
  }
}

// Push data to chips
void updateShiftRegisters() {
  digitalWrite(LATCH_PIN, LOW);
  for (int i = NUM_SHIFT_REGISTERS - 1; i >= 0; i--) {
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, relayStates[i]);
  }
  digitalWrite(LATCH_PIN, HIGH);
}
