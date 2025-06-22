#include <WiFi.h>
#include <ESP32Servo.h>
#include <NewPing.h>
#include <Keypad.h>
#include <FirebaseESP32.h>
#include <LiquidCrystal_I2C.h>
#include <TimeLib.h>

//wifi and firebase credentials
const char* ssid = "Wokwi-GUEST";
const char* password = "";
#define FIREBASE_API_KEY "####"
#define FIREBASE_DATABASE_URL "https://traffic-light-system-cf753-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// pin defintions
const int PIR_PIN = 13;
const int TRIG_PIN = 2;
const int ECHO_PIN = 4;
const int SERVO_PIN = 14;

// servo motor configuration
Servo gateServo;
const int GATE_CLOSED_ANGLE = 0;
const int GATE_OPEN_ANGLE = 90;
const int SERVO_SPEED_DELAY = 15;

// ultrasonic sensor configuration
#define MAX_DISTANCE 200
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
const int DISTANCE_THRESHOLD = 30;

// gate variables
bool gateOpen = false;
unsigned long gateOpenTime = 0;
const unsigned long GATE_OPEN_DURATION = 10000;

// pir variables
int pirState = LOW;
int lastPirState = LOW;

// keypad init
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {23, 22, 21, 19};
byte colPins[COLS] = {18, 5, 17, 16};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// screen init
LiquidCrystal_I2C lcd(0x27, 16, 2);

//license plate variables
String licensePlate = "";
bool plateCaptured = false;

bool checkIfParkingFull() {
  String systems[] = {"System1", "System2", "System3"};
  for (String system : systems) {
    String path = "/Parking/" + system;
    if (Firebase.getString(fbdo, path)) {
      String slotStatus = fbdo.stringData();
      if (slotStatus != "Occupied") {
        return false;
      }
    }
  }
  return true;
}

void incrementAnalytics(String type) {
  String path = "/Analytics/VehicleType/" + type;
  int current = 0;
  if (Firebase.getInt(fbdo, path)) {
    current = fbdo.intData();
  }
  Firebase.setInt(fbdo, path, current + 1);
}

void logPeakHour() {
  int hour = hourFormat12();
  String hourPath = "/Analytics/Arrivals/" + String(hour);
  int count = 0;
  if (Firebase.getInt(fbdo, hourPath)) {
    count = fbdo.intData();
  }
  Firebase.setInt(fbdo, hourPath, count + 1);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  gateServo.setPeriodHertz(50);
  gateServo.attach(SERVO_PIN, 500, 2500);
  closeGate();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Parking");
  delay(2000);
  lcd.clear();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DATABASE_URL;
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);

  lcd.setCursor(0, 0);
  lcd.print("Enter License:");
}

void loop() {
  pirState = digitalRead(PIR_PIN);
  if (pirState == HIGH && !gateOpen) {
    unsigned int distance = sonar.ping_cm();
    if (distance > 0 && distance <= DISTANCE_THRESHOLD) {

      if (checkIfParkingFull()) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Parking FULL");
        delay(3000);
        lcd.clear();
        return;
      }

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter License:");
      licensePlate = "";
      plateCaptured = false;

      while (!plateCaptured) {
        char key = keypad.getKey();
        if (key) {
          if (key == '#') {
            plateCaptured = true;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("1:EV 2:Access");
            lcd.setCursor(0, 1);
            lcd.print("#=Normal Slot");

            String slotType = "Normal";
            bool typeSelected = false;
            while (!typeSelected) {
              char k = keypad.getKey();
              if (k == '1') { slotType = "EV"; typeSelected = true; }
              else if (k == '2') { slotType = "Accessible"; typeSelected = true; }
              else if (k == '#') { typeSelected = true; }
            }

            String path = "/GateLogs/" + licensePlate;
            Firebase.setString(fbdo, path + "/status", "Arrived");
            Firebase.setString(fbdo, path + "/slot_type", slotType);
            Firebase.setString(fbdo, path + "/time", String(millis()));

            incrementAnalytics(slotType);
            logPeakHour();

            openGate();
            gateOpenTime = millis();
          }
          else if (key == '*') {
            licensePlate = "";
            lcd.setCursor(0, 0);
            lcd.print("Enter License:");
          }
          else {
            licensePlate += key;
            lcd.setCursor(0, 1);
            lcd.print(licensePlate);
          }
        }
      }
    }
  }

  if (gateOpen && (millis() - gateOpenTime >= GATE_OPEN_DURATION)) {
    unsigned int distance = sonar.ping_cm();
    if (!(distance > 0 && distance <= DISTANCE_THRESHOLD)) {
      closeGate();
      plateCaptured = false;
    }
  }
  lastPirState = pirState;
}

void openGate() {
  for (int pos = GATE_CLOSED_ANGLE; pos <= GATE_OPEN_ANGLE; pos++) {
    gateServo.write(pos);
    delay(SERVO_SPEED_DELAY);
  }
  gateOpen = true;
  lcd.clear();
  lcd.print("Gate OPEN");
}

void closeGate() {
  for (int pos = GATE_OPEN_ANGLE; pos >= GATE_CLOSED_ANGLE; pos--) {
    gateServo.write(pos);
    delay(SERVO_SPEED_DELAY);
  }
  gateOpen = false;
  lcd.clear();
  lcd.print("Gate CLOSED");
}
