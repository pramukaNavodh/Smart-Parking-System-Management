#include <WiFi.h>
#include <FirebaseESP32.h> // This actually loads Firebase_ESP_Client under the hood now

// WiFi credentials
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD "######"

// Firebase credentials
#define FIREBASE_API_KEY "#####"
#define FIREBASE_PROJECT_ID "traffic-light-system-cf753"
#define FIREBASE_DATABASE_URL "https://traffic-light-system-cf753-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define THRESHOLD_CM 15

struct ParkingSystem {
  int trigPin;
  int echoPin;
  int redLedPin;
  int greenLedPin;
  String systemName;
};

ParkingSystem systems[5] = {
  {16, 17, 18, 19, "System1"},
  {21, 22, 23, 25, "System2"},
  {26, 27, 32, 33, "System3"},
  {4, 2, 12, 13, "System4"},
  {14, 34, 15, 35, "System5"}
};

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println(" Connected!");

  // Set Firebase config
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DATABASE_URL;

  // Anonymous auth
  auth.user.email = "";
  auth.user.password = "";

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  for (int i = 0; i < 5; i++) {
    pinMode(systems[i].trigPin, OUTPUT);
    pinMode(systems[i].echoPin, INPUT);
    pinMode(systems[i].redLedPin, OUTPUT);
    pinMode(systems[i].greenLedPin, OUTPUT);
  }
}

long readDistanceCM(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  long distance = duration * 0.034 / 2;
  return distance;
}

void loop() {
  for (int i = 0; i < 5; i++) {
    long distance = readDistanceCM(systems[i].trigPin, systems[i].echoPin);
    String status;

    if (distance > 0 && distance < THRESHOLD_CM) {
      digitalWrite(systems[i].redLedPin, HIGH);
      digitalWrite(systems[i].greenLedPin, LOW);
      status = "Occupied";
    } else {
      digitalWrite(systems[i].redLedPin, LOW);
      digitalWrite(systems[i].greenLedPin, HIGH);
      status = "Available";
    }

    Serial.printf("%s: %s (%.1f cm)\n", systems[i].systemName.c_str(), status.c_str(), distance);

    String path = "/Parking/" + systems[i].systemName;
    if (!Firebase.setString(fbdo, path.c_str(), status)) {
      Serial.println("Firebase Error: " + fbdo.errorReason());
    }

    delay(100);
  }

  Serial.println("----------------------------");
  delay(1000);
}
