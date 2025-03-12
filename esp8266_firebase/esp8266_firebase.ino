// Files -> Preferences
// http://arduino.esp8266.com/stable/package_esp8266com_index.json

#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

// Wi-Fi Credentials
#define WIFI_SSID "Indicator1"
#define WIFI_PASSWORD "Pass135791234."

// Firebase Credentials
#define FIREBASE_HOST "voice-assistant-app-9fd53-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "SFlkEZl1bpXizLln34eYY8GdmvZU4QsZIrpRWfQ8"

// Relay Pins (Adjust as per your circuit)
#define BATHROOM_LIGHT  5   // D1
#define FAN             4   // D2
#define AC             0   // D3
#define KITCHEN_LIGHT   14  // D5

FirebaseConfig config;
FirebaseAuth auth;
FirebaseData firebaseData;
FirebaseData stream; // 🔥 Corrected: Declare FirebaseData object for streaming

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
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  
  Serial.println("\nConnected to WiFi!");

  // Set Firebase config
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  // Initialize Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize Relay Pins as OUTPUT
  for (int i = 0; i < deviceCount; i++) {
    pinMode(devices[i].pin, OUTPUT);
    digitalWrite(devices[i].pin, LOW); // Start OFF
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
            digitalWrite(devices[i].pin, (newStatus == "ON") ? HIGH : LOW);
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
}

void logToFirebase(String message) {
  Serial.println(message); // Print to Serial Monitor

  // 🔥 Save the latest log
  Firebase.setString(firebaseData, "/logs/latest", message);

  // 🔥 Save log history with timestamp
  String timePath = "/logs/history/" + String(millis()); // Use millis() as timestamp
  Firebase.setString(firebaseData, timePath, message);
}