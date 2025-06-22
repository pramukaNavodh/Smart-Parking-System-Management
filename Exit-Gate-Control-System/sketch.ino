#include <WiFi.h>
#include <ESP32Servo.h>
#include <NewPing.h>
#include <Keypad.h>
#include <FirebaseESP32.h>
#include <LiquidCrystal_I2C.h>
#include <TimeLib.h>

// Wi-Fi and Firebase credentials
const char* ssid = "Wokwi-GUEST";
const char* password = "";
#define FIREBASE_API_KEY "####"
#define FIREBASE_DATABASE_URL "https://traffic-light-system-cf753-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Pin definitions
const int PIR_PIN = 13;
const int TRIG_PIN = 2;
const int ECHO_PIN = 4;
const int SERVO_PIN = 14;

// Servo motor configuration
Servo gateServo;
const int GATE_CLOSED_ANGLE = 0;
const int GATE_OPEN_ANGLE = 90;
const int SERVO_SPEED_DELAY = 15;

// Ultrasonic sensor configuration
#define MAX_DISTANCE 200
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
const int DISTANCE_THRESHOLD = 30;

// Gate variables
bool gateOpen = false;
unsigned long gateOpenTime = 0;
const unsigned long GATE_OPEN_DURATION = 10000;

// Keypad init
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

// Screen init
LiquidCrystal_I2C lcd(0x27, 16, 2);

// License plate variables
String licensePlate = "";
bool plateCaptured = false;

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
  lcd.print("Exit Gate Ready");
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
  if (!gateOpen && digitalRead(PIR_PIN) == HIGH) {
    unsigned int distance = sonar.ping_cm();
    if (distance > 0 && distance <= DISTANCE_THRESHOLD) {
      licensePlate = "";
      plateCaptured = false;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter License:");

      while (!plateCaptured) {
        char key = keypad.getKey();
        if (key) {
          if (key == '#') {
            plateCaptured = true;
            String path = "/GateLogs/" + licensePlate + "/time";
            if (Firebase.getString(fbdo, path)) {
              unsigned long arrivalMillis = fbdo.stringData().toInt();
              unsigned long duration = millis() - arrivalMillis;
              int hours = ceil((float)duration / (1000 * 60 * 60));
              int amount = hours * 100;

              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Fee: Rs." + String(amount));
              delay(4000);

              Firebase.setString(fbdo, "/GateLogs/" + licensePlate + "/status", "Exited");
              Firebase.setString(fbdo, "/GateLogs/" + licensePlate + "/payment", String(amount));

              openGate();
              gateOpenTime = millis();
            } else {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Not Found");
              delay(2000);
            }
          } else if (key == '*') {
            licensePlate = "";
            lcd.setCursor(0, 0);
            lcd.print("Enter License:");
            lcd.setCursor(0, 1);
            lcd.print("                ");
          } else {
            licensePlate += key;
            lcd.setCursor(0, 1);
            lcd.print(licensePlate);
          }
        }
      }
    }
  }

  if (gateOpen && (millis() - gateOpenTime >= GATE_OPEN_DURATION)) {
    if (!(sonar.ping_cm() > 0 && sonar.ping_cm() <= DISTANCE_THRESHOLD)) {
      closeGate();
      plateCaptured = false;
    }
  }
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
