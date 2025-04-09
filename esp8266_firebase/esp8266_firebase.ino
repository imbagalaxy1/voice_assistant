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
#define BATHROOM_LIGHT  D1   // D1
#define FAN             D2   // D2
#define AC             D5   // D5
#define KITCHEN_LIGHT   D6  // D6

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
  {"ac", AC, "/devices/AC/status", "OFF"},
  {"fan", FAN, "/devices/fan/status", "OFF"},
  {"bathroom light", BATHROOM_LIGHT, "/devices/bathroom_light/status", "OFF"},
  {"kitchen light", KITCHEN_LIGHT, "/devices/kitchen_light/status", "OFF"}
};

const int deviceCount = sizeof(devices) / sizeof(devices[0]);

void setup() {
  Serial.begin(115200);
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

  // Initialize Relay Pins as OUTPUT
  for (int i = 0; i < deviceCount; i++) {
    pinMode(devices[i].pin, OUTPUT);
    digitalWrite(devices[i].pin, HIGH); // Start OFF, HIGH since active low
  }

  // Start Firebase Streaming Listener
  if (!Firebase.beginStream(stream, "/devices")) {
    Serial.println("Failed to start Firebase stream.");
    Serial.println(stream.errorReason()); // 🔥 Corrected: Using `stream` instead of `firebaseData`
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
      Firebase.beginStream(stream, "/devices"); // 🔥 Reconnect stream
      return;
    }

    if (stream.streamAvailable()) {
      Serial.println("Firebase update received!");
      // logToFirebase("Firebase update received!");

      // Iterate over devices and check for changes
      for (int i = 0; i < deviceCount; i++) {
        Serial.print("device path: ");
        Serial.println(devices[i].path);
        if (Firebase.getString(firebaseData, devices[i].path)) { // 🔥 Corrected: Use `firebaseData`
          String newStatus = firebaseData.stringData();
          Serial.print("firebase data: ");
          Serial.println(newStatus);
          if (newStatus != devices[i].status) { // Only update if status changes
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

  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].status == "ON") {  // Only check if it should be ON
      int targetState = LOW; // ON state for active LOW relay
      if (digitalRead(devices[i].pin) != targetState) { // Relay mismatch
        logToFirebase("❌ " + String(devices[i].name) + " relay failed, retrying...");
        digitalWrite(devices[i].pin, targetState); // Retry setting the relay
      }
    }
  }
}

void logToFirebase(String message) {
  Serial.println(message); // println to Serial Monitor

  // 🔥 Save the latest log
  Firebase.setString(firebaseData, "/logs/latest", message);

  // 🔥 Save log history with timestamp
  String timePath = "/logs/history/" + String(millis()); // Use millis() as timestamp
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
      if(wm.startConfigPortal("ESP32-Config", "password")){
        Serial.println("Opening portal.....");
        Serial.println("Connected via portal");
      }
    }
  }
}