/*
  ESP32-S3 + 74HC595 Chain Control (28 Shift Registers, 224 Relays)
*/

#include <Arduino.h>
#include <Print.h>
#include <WiFi.h>
#include <PubSubClient.h>

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
WiFiClient espClient;
PubSubClient client(espClient);

// --- WiFi and MQTT credentials ---
const char* ssid = "wifi ssid";
const char* password = "wifi password";
const char* mqtt_server = "192.168.100.165";
const int mqtt_port = 1883;

unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;
const unsigned long retry_intv = 5000; // retry every 5 seconds

// Forward declarations for functions
void setAllRelays(bool state);
void runSequence();
void flashAllRelays();
void toggleRelay(int relayId);
void setRelay(int ledId, bool state);
void updateShiftRegisters();
void maintainWiFi(bool blocking);
void maintainMQTT(bool blocking);
void onReceived(char* topic, byte* payload, unsigned int length);

void setup() {
    Serial.begin(115200);

    // Setup MQTT settings
    client.setServer(mqtt_server, 1883);
    client.setCallback(onReceived);
    client.setBufferSize(512);

    Serial.println(F(" --- System Starting ---"));

    // BLOCKING calls for setup (Wait until both are ready before starting loop)
    maintainWiFi(true);
    maintainMQTT(true);

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

    Serial.println(F(" --- System Ready ---"));
}

void loop() {
    // NON-BLOCKING calls for loop (Checks in background)
    maintainWiFi(false);
    maintainMQTT(false);

    // Your main code goes here (it will run smoothly)
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 2000) {
        lastMsg = millis();
        if (client.connected()) {
            client.publish("esp32/status", "Alive", false, 0);
        }
    }

    if (digitalRead(BUTTON_A) == LOW) {
        runSequence();
    } else if (digitalRead(BUTTON_B) == LOW) {
        flashAllRelays();
    }
}

// --- Logic: Network Connection ---
void maintainWiFi(bool blocking) {
    // 1. If we are already connected, do nothing
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    // 2. If we are NOT connected, check if it's time to try again
    unsigned long now = millis();
    if (blocking || (now - lastWifiAttempt > retry_intv)) {
        lastWifiAttempt = now;

        Serial.println(F("WiFi: Connecting..."));

        // We only need to call begin() once, but calling it again forces a re-attempt
        // Check if we are totally disconnected before calling begin again to avoid spamming
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.disconnect();
            WiFi.begin(ssid, password);
        }

        // If blocking is TRUE, we stay here until connected
        if (blocking) {
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.println(F("\nWiFi: Connected!"));
        Serial.print(F("IP Address: "));
        Serial.println(WiFi.localIP());
        }
    }
}

void maintainMQTT(bool blocking) {
    // 1. If WiFi is down, we can't do MQTT
    if (WiFi.status() != WL_CONNECTED) return;

    // 2. If already connected, do maintenance (loop) and return
    if (client.connected()) {
        client.loop();
        return;
    }

    // 3. If NOT connected, check if it's time to try again
    unsigned long now = millis();
    if (blocking || (now - lastMqttAttempt > retry_intv)) {
        lastMqttAttempt = now;
        Serial.print(F("MQTT: Attempting connection..."));

        // Create a random client ID
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);

        // Attempt to connect
        if (client.connect(clientId.c_str())) {
            Serial.println(F("connected"));
            // ON CONNECT: Resubscribe to topics here
            client.subscribe("esp32/led", 1);
        } else {
            Serial.print(F("failed, rc="));
            Serial.print(client.state()); // http://pubsubclient.knolleary.net/api.html#state

            if (blocking) {
                Serial.println(F(" retrying in 5 seconds..."));
                delay(5000);
                maintainMQTT(true); // Recursive call for blocking mode
            } else {
                Serial.println(F(" (background retry pending)"));
            }
        }
    }
}

// --- Logic: MQTT ---
void onReceived(char* topic, byte* payload, unsigned int length) {
    Serial.print(F("Message arrived ["));
    Serial.print(topic);
    Serial.print(F("]: "));

    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println(message);
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
