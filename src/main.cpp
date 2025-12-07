/*
  ESP32-S3 + 74HC595 Chain Control (28 Shift Registers, 224 Relays)
  + Keypad Control (A,B,C,D Features added)
  + OLED Status Display
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

// --- Config: Shift Registers ---
#define DATA_PIN  12
#define CLOCK_PIN 13
#define LATCH_PIN 14
#define NUM_SHIFT_REGISTERS 28
#define NUM_LEDS (NUM_SHIFT_REGISTERS * 8)

// --- Config: OLED Display ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
#define OLED_SDA 8
#define OLED_SCL 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Config: Keypad ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {4, 5, 6, 7};
byte colPins[COLS] = {15, 16, 17, 18};

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// --- Global Variables ---
byte relayStates[NUM_SHIFT_REGISTERS];
String inputBuffer = "";

// Forward declarations for functions
void handleKeypress(char key);
void setAllRelays(bool state);
void runSequence();
void flashAllRelays();
void toggleRelay(int relayId);
void setRelay(int ledId, bool state);
void updateShiftRegisters();
void updateStatusScreen();
void updateDisplay(String line1, String line2);

void setup() {
  Serial.begin(115200);

  // 1. Initialize Shift Register Pins
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);

  // Clear all LEDs initially
  setAllRelays(false);

  // 2. Initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Show Startup Screen
  updateDisplay("Ready", "Type ID#");
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    handleKeypress(key);
  }
}

// --- Logic: Keypad Handling ---
void handleKeypress(char key) {
  // If user types a number (0-9)
  if (key >= '0' && key <= '9') {
    if (inputBuffer.length() < 3) {
      inputBuffer += key;
    }
    updateStatusScreen();
  }
  // If user presses '*' -> Clear Input
  else if (key == '*') {
    inputBuffer = "";
    updateDisplay("Cleared", "Type ID#");
  }
  // If user presses '#' -> Toggle the specific Relay ID
  else if (key == '#') {
    if (inputBuffer.length() > 0) {
      int id = inputBuffer.toInt();
      toggleRelay(id);
      inputBuffer = "";
    }
  }
  // --- NEW FEATURES ---
  else if (key == 'A') {
    inputBuffer = "";
    updateDisplay("Action:", "All OFF");
    setAllRelays(false); // Turn all OFF
  }
  else if (key == 'B') {
    inputBuffer = "";
    updateDisplay("Action:", "Sequence...");
    runSequence(); // Run sequential chaser
  }
  else if (key == 'C') {
    inputBuffer = "";
    updateDisplay("Action:", "Flashing!");
    flashAllRelays(); // Flash 3 times
  }
  else if (key == 'D') {
    inputBuffer = "";
    updateDisplay("Action:", "All ON");
    setAllRelays(true); // Turn all ON
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

// Keypad B: Run relays sequentially (Chaser effect)
void runSequence() {
  // Ensure everything is off first
  setAllRelays(false);

  for (int i = 0; i < NUM_LEDS; i++) {
    // Check if user wants to abort sequence by pressing '*'
    char key = keypad.getKey();
    if (key != NO_KEY && key == '*') {
      updateDisplay("Stopped", "Ready");
      setAllRelays(false);
      return;
    }

    setRelay(i, true);
    updateShiftRegisters();
    delay(30); // Speed of the sequence (adjust as needed)
    setRelay(i, false); // Turn it off to create a "moving dot" effect
    // Note: Remove the line above if you want them to "fill up" instead of "chase"
  }
  // Ensure last one is off
  updateShiftRegisters();
  updateDisplay("Done", "Ready");
}

// Keypad C: Flash all relays 3 times
void flashAllRelays() {
  for(int k=0; k<3; k++) {
    setAllRelays(true);
    delay(200);
    setAllRelays(false);
    delay(200);
  }
  updateDisplay("Done", "Ready");
}

// --- Logic: Relay Control ---

// Toggle a specific relay ID
void toggleRelay(int relayId) {
  if (relayId < 0 || relayId >= NUM_LEDS) {
    updateDisplay("Error", "Invalid ID");
    return;
  }

  int registerIndex = relayId / 8;
  int bitIndex = relayId % 8;

  bool isOne = (relayStates[registerIndex] & (1 << bitIndex));

  setRelay(relayId, !isOne);
  updateShiftRegisters();

  String statusMsg = "Relay " + String(relayId);
  String stateMsg = !isOne ? "ON" : "OFF";
  updateDisplay(statusMsg, stateMsg);
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

// --- Logic: Display Helper ---

void updateStatusScreen() {
  int id = inputBuffer.toInt();
  String top = "Target: " + inputBuffer;

  String bottom = "";
  if (id >= 0 && id < NUM_LEDS) {
    int registerIndex = id / 8;
    int bitIndex = id % 8;
    bool isOn = (relayStates[registerIndex] & (1 << bitIndex));
    bottom = "Curr: " + String(isOn ? "ON" : "OFF");
  } else {
    bottom = "Inv Range";
  }

  updateDisplay(top, bottom);
}

void updateDisplay(String line1, String line2) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(line1);
  display.setCursor(0, 32);
  display.println(line2);
  display.display();
}
