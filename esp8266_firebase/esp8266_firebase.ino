// Files -> Preferences
// http://arduino.esp8266.com/stable/package_esp8266com_index.json

#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <WiFiManager.h>

WiFiManager wm;

// Firebase Credentials
#define FIREBASE_HOST "voice-assistant-app-9fd53-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "SFlkEZl1bpXizLln34eYY8GdmvZU4QsZIrpRWfQ8"

// Relay Pins (Adjust as per your circuit)
#define BATHROOM_LIGHT D5  // D5
#define FAN D7             // D7
#define AC D2              // D2
#define KITCHEN_LIGHT D6   // D6

#define BUTTON_AC D1
#define BUTTON_FAN D3
#define BUTTON_BATHROOM_LIGHT D4
#define BUTTON_KITCHEN_LIGHT D0

bool buttonStates[4] = { HIGH, HIGH, HIGH, HIGH };  // Default state: not pressed
bool lastButtonStates[4] = { HIGH, HIGH, HIGH, HIGH };  // Start as not pressed
unsigned long debounceDelay = 20;                       // 50ms debounce
unsigned long lastDebounceTimes[4] = { 0, 0, 0, 0 };

FirebaseConfig config;
FirebaseAuth auth;
FirebaseData firebaseData;
FirebaseData stream;

// Device Mapping (For easy loop processing)
struct Device {
  const char* name;
  int pin;
  String path;
  String status;
};

Device devices[] = {
  { "ac", AC, "/devices/AC/status", "OFF" },
  { "fan", FAN, "/devices/fan/status", "OFF" },
  { "bathroom light", BATHROOM_LIGHT, "/devices/bathroom_light/status", "OFF" },
  { "kitchen light", KITCHEN_LIGHT, "/devices/kitchen_light/status", "OFF" }
};

const int deviceCount = sizeof(devices) / sizeof(devices[0]);

void setup() {
  Serial.begin(115200);
  // Initialize Relay Pins as OUTPUT
  for (int i = 0; i < deviceCount; i++) {
    pinMode(devices[i].pin, OUTPUT);
    digitalWrite(devices[i].pin, HIGH);
  }
  pinMode(BUTTON_AC, INPUT_PULLUP);
  pinMode(BUTTON_FAN, INPUT_PULLUP);
  pinMode(BUTTON_BATHROOM_LIGHT, INPUT_PULLUP);
  pinMode(BUTTON_KITCHEN_LIGHT, INPUT_PULLUP);

  if (!wm.autoConnect("ESP32-Config", "password")) {
    Serial.println("⚠️ Failed to connect. Restarting...");
    Serial.println("Wifi connect failed");
    Serial.println("Restarting...");
    delay(3000);
    ESP.restart();  // Try again
  }
  Serial.println("Wi-Fi connected!");
  Serial.println("Starting.....");
  Serial.println("✅ Wi-Fi connected!");
  Serial.println(WiFi.localIP());

  // Set Firebase config
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  // Initialize Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Start Firebase Streaming Listener
  if (!Firebase.beginStream(stream, "/devices")) {
    Serial.println("Failed to start Firebase stream.");
    Serial.println(stream.errorReason());
  } else {
    Serial.println("Listening for Firebase changes...");
  }
}

void loop() {
  // Check Firebase Stream for updates
  if (Firebase.readStream(stream)) {
    if (stream.streamTimeout()) {
      String errorMsg = "Firebase stream timeout, reconnecting...";
      logToFirebase(errorMsg);
      Firebase.beginStream(stream, "/devices");  // 🔥 Reconnect stream
      return;
    }

    if (stream.streamAvailable()) {
      Serial.println("Firebase update received!");
      // logToFirebase("Firebase update received!");

      // Iterate over devices and check for changes
      for (int i = 0; i < deviceCount; i++) {
        Serial.print("device path: ");
        Serial.println(devices[i].path);
        if (Firebase.getString(firebaseData, devices[i].path)) {
          String newStatus = firebaseData.stringData();
          Serial.print("firebase data: ");
          Serial.println(newStatus);
          if (newStatus != devices[i].status) {  // Only update if status changes
            devices[i].status = newStatus;

            // 🔥 Construct the message
            String logMessage = devices[i].name;
            logMessage += " changed to: ";
            logMessage += newStatus;

            // 🔥 Log to Firebase
            logToFirebase(logMessage);
            digitalWrite(devices[i].pin, (newStatus == "ON") ? LOW : HIGH);
          }
        } else {
          String errorMsg = "Error reading " + String(devices[i].name) + ": " + firebaseData.errorReason();
          logToFirebase(errorMsg);
        }
      }
    }
  } else {
    String errorMsg = "Firebase stream failed: " + stream.errorReason();
    logToFirebase(errorMsg);
  }

  handleButtons();

  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].status == "ON") {                     // Only check if it should be ON
      int targetState = LOW;                             // ON state for active LOW relay
      if (digitalRead(devices[i].pin) != targetState) {  // Relay mismatch
        logToFirebase("❌ " + String(devices[i].name) + " relay failed, retrying...");
        digitalWrite(devices[i].pin, targetState);  // Retry setting the relay
      }
    }
  }
}

void handleButtons() {
  int buttonPins[4] = { BUTTON_AC, BUTTON_FAN, BUTTON_BATHROOM_LIGHT, BUTTON_KITCHEN_LIGHT };

  for (int i = 0; i < 4; i++) {
    int reading = digitalRead(buttonPins[i]);

    if (reading != lastButtonStates[i]) {
      lastDebounceTimes[i] = millis();  // Reset debounce timer
    }

    if ((millis() - lastDebounceTimes[i]) > debounceDelay) {
      // If the state has changed
      if (reading != buttonStates[i]) {
        buttonStates[i] = reading;

        if (buttonStates[i] == LOW) {
          // Button was pressed, toggle device
          if (devices[i].status == "OFF") {
            devices[i].status = "ON";
            digitalWrite(devices[i].pin, LOW);
            Firebase.setString(firebaseData, devices[i].path, "ON");
            logToFirebase("🔁 Toggle → " + String(devices[i].name) + " ON");
          } else {
            devices[i].status = "OFF";
            digitalWrite(devices[i].pin, HIGH);
            Firebase.setString(firebaseData, devices[i].path, "OFF");
            logToFirebase("🔁 Toggle → " + String(devices[i].name) + " OFF");
          }
        }
      }
    }

    lastButtonStates[i] = reading;
  }
}

void logToFirebase(String message) {
  Serial.println(message);  // println to Serial Monitor

  // 🔥 Save the latest log
  Firebase.setString(firebaseData, "/logs/latest", message);

  // 🔥 Save log history with timestamp
  String timePath = "/logs/history/" + String(millis());  // Use millis() as timestamp
  Firebase.setString(firebaseData, timePath, message);
}

void checkWiFiAutoReconnect() {
  if (!Firebase.ready()) {
    Serial.println("Wi-Fi lost,");
    Serial.println("reconnecting...");
    WiFi.begin();  // Uses saved credentials
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(500);
      Serial.println(".");
    }
    if (Firebase.ready()) {
      Serial.println("Reconnected to Wi-Fi");
    } else {
      Serial.println("Failed to reconnect.");
      if (wm.startConfigPortal("ESP32-Config", "password")) {
        Serial.println("Opening portal.....");
        Serial.println("Connected via portal");
      }
    }
  }
}