/*****************
ESP8266 + RC522 + Blynk IoT
Automatic Pet Feeder
*****************/

#define BLYNK_PRINT Serial

// Replace these with your own Blynk credentials
#define BLYNK_TEMPLATE_ID "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "SmartPetFeeder"
#define BLYNK_AUTH_TOKEN "YOUR_AUTH_TOKEN"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// ---------------- PIN CONFIG -------------------
#define SS_PIN     D4
#define RST_PIN    D3
#define SERVO_PIN  D2

#define MAX_PETS 10

MFRC522 rfid(SS_PIN, RST_PIN);
Servo feeder;
BlynkTimer timer;
BlynkTimer autoCloseTimer;

// ---------------- WIFI -------------------------
char ssid[] = "YOUR_WIFI_NAME";
char pass[] = "YOUR_WIFI_PASSWORD";

// ---------------- PET STORAGE ------------------
String petName[MAX_PETS];
String petID[MAX_PETS];
String petUID[MAX_PETS];
int petCount = 0;

// ---------------- TEMP INPUTS ------------------
String inName, inID, inUID;
String delID;

// ---------------- PET LIST ---------------------
void updatePetList() {
  String list = "Saved Pets:\n";

  for (int i = 0; i < petCount; i++) {
    list += String(i + 1) + ". " + petName[i] +
            " | ID: " + petID[i] +
            " | UID: " + petUID[i] + "\n";
  }

  if (petCount == 0)
    list += "No pets saved";

  Blynk.virtualWrite(V14, list);
}

// ---------------- SERVO ------------------------
bool servoAutoClosing = false;
int autoCloseTimerID = -1;

void autoCloseServo() {
  feeder.write(0);
  Blynk.virtualWrite(V7, 0);

  servoAutoClosing = false;
  autoCloseTimerID = -1;
}

// ---------------- MANUAL SERVO -----------------
BLYNK_WRITE(V7) {

  int val = param.asInt();

  if (val) {
    feeder.write(180);

    servoAutoClosing = true;
    autoCloseTimerID = autoCloseTimer.setTimeout(10000, autoCloseServo);

  } else {

    feeder.write(0);

    if (servoAutoClosing && autoCloseTimerID != -1) {
      autoCloseTimer.deleteTimer(autoCloseTimerID);
      servoAutoClosing = false;
      autoCloseTimerID = -1;
    }
  }
}

// ---------------- INPUTS -----------------------
BLYNK_WRITE(V11) { inName = param.asStr(); inName.trim(); }
BLYNK_WRITE(V12) { inUID  = param.asStr(); inUID.trim(); inUID.toUpperCase(); }
BLYNK_WRITE(V8)  { inID   = param.asStr(); inID.trim(); }
BLYNK_WRITE(V10) { delID  = param.asStr(); delID.trim(); }

// ---------------- SAVE PET ---------------------
BLYNK_WRITE(V13) {

  if (!param.asInt()) return;

  if (inName == "" || inID == "" || inUID == "") {
    Blynk.virtualWrite(V15, "Fill all fields");
  }
  else if (petCount >= MAX_PETS) {
    Blynk.virtualWrite(V15, "Pet limit reached");
  }
  else {

    petName[petCount] = inName;
    petID[petCount]   = inID;
    petUID[petCount]  = inUID;

    petCount++;

    Blynk.virtualWrite(V15, "Pet added");
    updatePetList();
  }

  Blynk.virtualWrite(V13, 0);
}

// ---------------- DELETE PET -------------------
BLYNK_WRITE(V17) {

  if (!param.asInt()) return;

  bool found = false;

  for (int i = 0; i < petCount; i++) {

    if (petID[i] == delID) {

      for (int j = i; j < petCount - 1; j++) {
        petName[j] = petName[j + 1];
        petID[j]   = petID[j + 1];
        petUID[j]  = petUID[j + 1];
      }

      petCount--;
      found = true;

      Blynk.virtualWrite(V15, "Pet deleted");
      updatePetList();

      break;
    }
  }

  if (!found)
    Blynk.virtualWrite(V15, "Pet ID not found");

  Blynk.virtualWrite(V17, 0);
}

// ---------------- RFID SCAN --------------------
void checkRFID() {

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }

  uid.toUpperCase();

  Blynk.virtualWrite(V9, uid);

  bool matched = false;

  for (int i = 0; i < petCount; i++) {

    if (uid == petUID[i]) {

      String msg = "Pet detected: " + petName[i];
      Blynk.virtualWrite(V15, msg);

      matched = true;
      break;
    }
  }

  if (!matched) {
    Blynk.virtualWrite(V15, "Unknown pet");
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ---------------- SETUP ------------------------
void setup() {

  Serial.begin(115200);

  feeder.attach(SERVO_PIN);
  feeder.write(0);

  SPI.begin();
  rfid.PCD_Init();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(400, checkRFID);

  updatePetList();
}

// ---------------- LOOP -------------------------
void loop() {

  Blynk.run();
  timer.run();
  autoCloseTimer.run();
}
