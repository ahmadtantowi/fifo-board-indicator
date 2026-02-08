/*
  ESP32-S3 + 74HC595 Chain Control (28 Shift Registers, 224 Relays)
*/

#include <Arduino.h>
#include <Print.h>
#include <WiFi.h>
#include <esp_wifi.h>
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
// Use explicit integer type for clarity
uint8_t relayStates[NUM_SHIFT_REGISTERS];
WiFiClient espClient;
PubSubClient client(espClient);

// --- WiFi and MQTT credentials ---
const char* WIFI_SSID = "BUDI UTAMI";
const char* WIFI_PASSWORD = "SugandaCRT21";
const char* MQTT_SERVER = "192.168.100.165";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "lamp/+/set"; // lamp/{id}/set

unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;
// Retry/backoff settings
const unsigned long RETRY_INTERVAL_INITIAL = 5000; // initial retry interval (ms)
const unsigned long RETRY_INTERVAL_MAX = 60000; // cap backoff at 60s
unsigned long wifiRetryInterval = RETRY_INTERVAL_INITIAL;
unsigned long mqttRetryInterval = RETRY_INTERVAL_INITIAL;
// Blocking connect timeouts to avoid infinite hangs in setup
const unsigned long WIFI_BLOCK_TIMEOUT = 20000; // 20s
const unsigned long MQTT_BLOCK_TIMEOUT = 20000; // 20s
// Track connection transition to print one-time connect message
bool wifiConnectedState = false;

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
void handleRelayMessage(const String& topic, const String& payload);
bool tryParseState(const String& payload, bool* stateOut);
int extractId(const String& topic);

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    // Setup MQTT settings
    client.setServer(MQTT_SERVER, 1883);
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

    if (WiFi.status() == WL_CONNECTED) {
        runSequence();
    } else {
        flashAllRelays();
    }

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
    // 1. If we are already connected, print one-time message, reset backoff and do nothing
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiConnectedState) {
            Serial.println(F("WiFi: Connected!"));
            Serial.print(F("IP Address: "));
            Serial.println(WiFi.localIP());
            wifiConnectedState = true;
        }
        wifiRetryInterval = RETRY_INTERVAL_INITIAL;
        return;
    } else {
        wifiConnectedState = false;
    }

    // 2. If we are NOT connected, check if it's time to try again
    unsigned long now = millis();
    if (blocking || (now - lastWifiAttempt > wifiRetryInterval)) {
        lastWifiAttempt = now;

        Serial.println(F("WiFi: Connecting..."));

        // Attempt to connect (disconnect first to force a fresh attempt)
        WiFi.disconnect();
        esp_wifi_set_max_tx_power(8.5);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        // If blocking is TRUE, wait up to WIFI_BLOCK_TIMEOUT for connection
        if (blocking) {
            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_BLOCK_TIMEOUT) {
                digitalWrite(LED_BUILTIN, HIGH);
                delay(500);
                Serial.print('.');
                digitalWrite(LED_BUILTIN, LOW);
                delay(100);
            }
            if (WiFi.status() == WL_CONNECTED) {
                wifiConnectedState = true;
                wifiRetryInterval = RETRY_INTERVAL_INITIAL;
            } else {
                Serial.println(F("\nWiFi: Failed to connect (timeout)"));
                // bump backoff for next non-blocking attempt
                wifiRetryInterval = min(wifiRetryInterval * 2, RETRY_INTERVAL_MAX);
            }
        } else {
            // Non-blocking: if connect didn't happen quickly, increase backoff
            if (WiFi.status() != WL_CONNECTED) {
                wifiRetryInterval = min(wifiRetryInterval * 2, RETRY_INTERVAL_MAX);
                Serial.println(F("WiFi: Background retry scheduled"));
            }
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
    if (blocking) {
        // Blocking mode: try repeatedly but bound total wait time
        unsigned long start = millis();
        while (!client.connected() && (millis() - start) < MQTT_BLOCK_TIMEOUT) {
            Serial.print(F("MQTT: Attempting connection..."));
            // Create a random client ID
            String clientId = "ESP32Client-";
            clientId += String(random(0xffff), HEX);

            if (client.connect(clientId.c_str())) {
                Serial.println(F(" connected"));
                client.subscribe(MQTT_TOPIC, 1);
                mqttRetryInterval = RETRY_INTERVAL_INITIAL;
                break;
            } else {
                Serial.print(F(" failed, rc="));
                Serial.print(client.state());
                Serial.println(F("; retrying..."));
                delay(1000);
            }
        }
        if (!client.connected()) {
            Serial.println(F("MQTT: Failed to connect within blocking timeout"));
            mqttRetryInterval = min(mqttRetryInterval * 2, RETRY_INTERVAL_MAX);
        }
    } else {
        // Non-blocking: attempt only when interval elapsed
        if (now - lastMqttAttempt > mqttRetryInterval) {
            lastMqttAttempt = now;
            Serial.print(F("MQTT: Attempting connection..."));

            String clientId = "ESP32Client-";
            clientId += String(random(0xffff), HEX);

            if (client.connect(clientId.c_str())) {
                Serial.println(F("connected"));
                client.subscribe(MQTT_TOPIC, 1);
                mqttRetryInterval = RETRY_INTERVAL_INITIAL;
            } else {
                Serial.print(F("failed, rc="));
                Serial.print(client.state());
                Serial.println(F(" (background retry pending)"));
                mqttRetryInterval = min(mqttRetryInterval * 2, RETRY_INTERVAL_MAX);
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
    handleRelayMessage(topic, message);
}

void handleRelayMessage(const String& topic, const String& payload) {
    bool desiredState = false;
    if (!tryParseState(payload, &desiredState)) return;

    int id = extractId(topic);
    if (id < 0){
        setAllRelays(desiredState);
        Serial.print(F("Set ALL relay to "));
        Serial.println(desiredState ? "ON" : "OFF");
        return;
    } else if (id >= NUM_LEDS){
        Serial.println(F("Invalid relay ID"));
        return;
    }
    setRelay(id, desiredState);
    updateShiftRegisters();
    Serial.print(F("Relay "));
    Serial.print(id);
    Serial.print(F(" state: "));
    Serial.println(desiredState ? F("ON") : F("OFF"));
}

bool tryParseState(const String& payload, bool* stateOut) {
    if (!stateOut) return false;

    String normalized = payload;
    normalized.trim();
    normalized.toLowerCase();
    if (normalized == "on" || normalized == "1" || normalized == "true") {
        *stateOut = true;
        return true;
    }
    if (normalized == "off" || normalized == "0" || normalized == "false") {
        *stateOut = false;
        return true;
    }
    return false;
}

int extractId(const String& topic) {
    if (!topic.startsWith("lamp/")) return -1;
    int secondSlash = topic.indexOf('/', 5);
    if (secondSlash < 0) return -1;
    if (topic.substring(secondSlash) != "/set") return -1;

    String idPart = topic.substring(5, secondSlash);
    if (idPart.length() == 0) return -1;
    for (unsigned int i = 0; i < idPart.length(); i++) {
        if (!isDigit(idPart[i])) return -1;
    }
    return idPart.toInt();
}

// --- Logic: Special Functions ---

// Helper to set ALL relays ON or OFF at once
void setAllRelays(bool state) {
    byte fillValue = state ? 0xFF : 0x00; // 0xFF is 11111111, 0x00 is 00000000
    for (int i = 0; i < NUM_SHIFT_REGISTERS; i++) {
        relayStates[i] = fillValue;
    }
    updateShiftRegisters();
    client.publish("lamp/-1/state", state ? "ON" : "OFF", 1, false);
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
    client.publish("lamp/sequence", "completed", 1, false);
}

// Flash all relays 3 times
void flashAllRelays() {
    for(int k=0; k<3; k++) {
        setAllRelays(true);
        delay(200);
        setAllRelays(false);
        delay(200);
    }
    client.publish("lamp/flash", "completed", 1, false);
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
    String topic = "lamp/" + String(ledId) + "/state";
    client.publish(topic.c_str(), state ? "ON" : "OFF", 1, false);
}

// Push data to chips
void updateShiftRegisters() {
    digitalWrite(LATCH_PIN, LOW);
    for (int i = NUM_SHIFT_REGISTERS - 1; i >= 0; i--) {
        shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, relayStates[i]);
    }
    digitalWrite(LATCH_PIN, HIGH);
}
