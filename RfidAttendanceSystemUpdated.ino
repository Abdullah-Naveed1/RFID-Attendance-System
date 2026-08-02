#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

class RFIDAttendanceSystem {
  private:
    MFRC522 rfid;
    LiquidCrystal_I2C lcd;
    uint8_t ledGreenPin, ledRedPin, buzzerPin;

    static const int MAX_USERS = 10;
    static const int UID_SIZE = 12;

    String authorizedUIDs[MAX_USERS];
    int authorizedCount;

    void buzz(int durationMs) {
      tone(buzzerPin, 500);
      delay(durationMs);
      noTone(buzzerPin);
    }

    String readUID() {
      String uidStr = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uidStr += " 0";
        else uidStr += " ";
        uidStr += String(rfid.uid.uidByte[i], HEX);
      }
      uidStr.toUpperCase();
      return uidStr;
    }

    bool isAuthorized(String uid) {
      for (int i = 0; i < authorizedCount; i++) {
        if (authorizedUIDs[i] == uid) return true;
      }
      return false;
    }

    void addUID(String uid) {
      if (authorizedCount < MAX_USERS) {
        authorizedUIDs[authorizedCount] = uid;
        saveUIDToEEPROM(authorizedCount, uid);
        authorizedCount++;
        EEPROM.update(0, authorizedCount);
        Serial.println("UID added successfully.");
      } else {
        Serial.println("UID list full!");
      }
    }

    void displayMessage(String l1, String l2, bool green=false, bool red=false, int buzzMs=0) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(l1);
      lcd.setCursor(0,1);
      lcd.print(l2);

      digitalWrite(ledGreenPin, green ? HIGH : LOW);
      digitalWrite(ledRedPin, red ? HIGH : LOW);

      if (buzzMs > 0) buzz(buzzMs);
    }

    bool askToEnroll(String uid) {
      Serial.println("Unknown UID detected:");
      Serial.println(uid);
      Serial.println("Add this UID? (Y/N)");

      unsigned long start = millis();
      while (millis() - start < 8000) {
        if (Serial.available()) {
          char c = toupper(Serial.read());
          if (c == 'Y') return true;
          if (c == 'N') return false;
        }
      }

      Serial.println("No response received. Enrollment cancelled.");
      return false;
    }

    int getStudentNumber(String uid) {
      for (int i = 0; i < authorizedCount; i++) {
        if (authorizedUIDs[i] == uid) return i + 1;
      }
      return -1;
    }

    void listStudents() {
      if (authorizedCount == 0) {
        Serial.println("No students enrolled.");
        return;
      }
      
      Serial.println("---- Enrolled Students ----");
      for (int i = 0; i < authorizedCount; i++) {
        Serial.print("STUDENT ");
        Serial.print(i + 1);
        Serial.print(" -> UID:");
        Serial.println(authorizedUIDs[i]);
      }
      Serial.println("---------------------------");
    }

    // ---- EEPROM Helpers ----

    void saveUIDToEEPROM(int index, String uid) {
      int base = 1 + index * UID_SIZE;
      for (int i = 0; i < UID_SIZE; i++) {
        byte b = (i < uid.length()) ? uid[i] : 0;
        EEPROM.update(base + i, b);
      }
    }

    void loadFromEEPROM() {
      authorizedCount = EEPROM.read(0);

      if (authorizedCount > MAX_USERS) authorizedCount = 0;

      for (int i = 0; i < authorizedCount; i++) {
        int base = 1 + i * UID_SIZE;
        String uid = "";
        for (int j = 0; j < UID_SIZE; j++) {
          char c = EEPROM.read(base + j);
          if (c != 0) uid += c;
        }
        authorizedUIDs[i] = uid;
      }

      if (authorizedCount > 0) {
        Serial.println("Loaded from EEPROM:");
        listStudents();
      }
    }

  public:
    RFIDAttendanceSystem(uint8_t ss, uint8_t rst, uint8_t ledG, uint8_t ledR, uint8_t buz, uint8_t lcdAddr = 0x27)
      : rfid(ss, rst), lcd(lcdAddr, 16, 2),
        ledGreenPin(ledG), ledRedPin(ledR), buzzerPin(buz) {
      authorizedCount = 0;
    }

    void init() {
      Serial.begin(9600);
      SPI.begin();
      rfid.PCD_Init();

      lcd.init();
      lcd.backlight();

      pinMode(ledGreenPin, OUTPUT);
      pinMode(ledRedPin, OUTPUT);
      pinMode(buzzerPin, OUTPUT);
      noTone(buzzerPin);

      loadFromEEPROM();

      lcd.clear();
      lcd.print("RFID System Ready");
      delay(1200);
      lcd.clear();
      lcd.print("Scan Card...");
    }

    void loop() {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == 'L' || c == 'l') {
          listStudents();
        }
      }

      if (!rfid.PICC_IsNewCardPresent()) return;
      if (!rfid.PICC_ReadCardSerial()) return;

      String uid = readUID();
      Serial.println("Scanned UID:" + uid);

      if (isAuthorized(uid)) {
        int studentNo = getStudentNumber(uid);

        displayMessage("STUDENT " + String(studentNo), "PRESENT", true, false, 300);

        Serial.print("Attendance marked for STUDENT ");
        Serial.print(studentNo);
        Serial.print(" | UID:");
        Serial.println(uid);

        delay(2500);

      } else {
        displayMessage("UNKNOWN CARD", "CHECK SERIAL", false, true, 400);

        if (askToEnroll(uid)) {
          addUID(uid);
          int studentNo = authorizedCount;

          displayMessage("STUDENT " + String(studentNo), "PRESENT", true, false, 500);

          Serial.print("New STUDENT ");
          Serial.print(studentNo);
          Serial.print(" enrolled | UID:");
          Serial.println(uid);

          Serial.println("Attendance marked immediately.");

          delay(2500);

        } else {
          displayMessage("ACCESS", "DENIED", false, true, 1200);
          Serial.println("UID not added");
          delay(2000);
        }
      }

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();

      lcd.clear();
      lcd.print("Scan Card...");
      digitalWrite(ledGreenPin, LOW);
      digitalWrite(ledRedPin, LOW);
    }
};

RFIDAttendanceSystem attendance(10, 9, 2, 3, 4);

void setup() {
  attendance.init();
}

void loop() {
  attendance.loop();
}
