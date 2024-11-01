#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 4);

#define SS_PIN 10  // RFID Reader SDA pin
#define RST_PIN 9   // RFID Reader RST pin
#define valvePin 8  // Pin for the water valve

// Initialize RFID reader
MFRC522 rfid(SS_PIN, RST_PIN);

// Define Keypad
const byte ROWS = 4;  // 4 rows for the numeric keypad
const byte COLS = 3;  // 3 columns for the numeric keypad
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {32, 33, 34, 35};  // Row pins for the keypad
byte colPins[COLS] = {40, 41, 42};      // Column pins for the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Define multiple authorized UIDs with associated names and EEPROM addresses
struct AuthorizedCard {
  byte uid[4];
  const char* name;
  int balanceAddress;  // EEPROM address for storing this card's balance
};

// Initial authorized cards with names and EEPROM addresses for balance storage
AuthorizedCard authorizedCards[] = {
  {{0xB3, 0x46, 0xD8, 0xA9}, "Francis Masanja", 0},
  {{0x63, 0xE9, 0x7C, 0xA9}, "Emanuel Ricky", 4},
  {{0x03, 0xDD, 0xB3, 0xA9}, "Brac'ha Marcos", 8},
  {{0x23, 0x08, 0x35, 0xAA}, "Arthur Daniel", 12},
  {{0x33, 0xC5, 0x0E, 0xAA}, "Gracious Wilfred", 16},
  {{0x03, 0x85, 0x92, 0xA9}, "Lidya Samson", 20}
};
const int numCards = sizeof(authorizedCards) / sizeof(authorizedCards[0]);

// Variables
unsigned long lastTime = 0;
unsigned long sessionStartTime = 0;
const unsigned long sessionTimeout = 30000;  // 30-second timeout for inactivity
const unsigned long dispenseInterval = 1000;  // 1 second = 1 unit of balance
bool isAuthorized = false;  // Variable to track authorization state
int currentBalance = 0;
int currentCardIndex = -1;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(valvePin, OUTPUT);
  digitalWrite(valvePin, LOW);  // Valve initially off

  lcd.init();
  lcd.clear();
  lcd.backlight();

  Serial.println("System ready. Please scan authorized card to activate keypad...");
  lcd.setCursor(0, 0);
  lcd.print("Please scan card");
  lcd.setCursor(0, 1);
  lcd.print("to activate keypad...");
}

void loop() {
  // Card reading
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    handleCard();
    rfid.PICC_HaltA();
  }

  // Timeout handling
  if (isAuthorized && millis() - sessionStartTime > sessionTimeout) {
    logout();
  }

  // Keypad input for dispensing water (only if authorized)
  if (isAuthorized) {
    char key = keypad.getKey();
    if (key) {
      Serial.print("Key Pressed: ");
      Serial.println(key);

      if (currentBalance > 0) {
        if (key >= '0' && key <= '9') {
          int dispenseTime = (key - '0') * 1000;  // Convert keypress to dispensing time in ms (1-9 seconds)
          dispenseWater(dispenseTime);
          logout();  // Log out user after one dispensing session
        } else if (key == '#') {
          Serial.println("Water dispensing canceled.");
          display("Water dispensing canceled.");
          digitalWrite(valvePin, LOW);
          delay(2000);
          logout();  // Reset the system
        }
      } else {
        Serial.println("Insufficient balance.");
        display("Insufficient balance.");
        logout();
      }
    }
  }
}

// Handle the RFID card
void handleCard() {
  Serial.print("Card UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Check if the card UID matches any authorized cards
  isAuthorized = false;
  currentCardIndex = -1;
  for (int i = 0; i < numCards; i++) {
    if (compareUID(rfid.uid.uidByte, rfid.uid.size, authorizedCards[i].uid)) {
      currentCardIndex = i;
      int address = authorizedCards[i].balanceAddress;
      
      // Check and initialize balance if necessary
      currentBalance = EEPROM.read(address);
      if (currentBalance == 0 || currentBalance == 255) {  // EEPROM unwritten or needs initializing
        currentBalance = 1000;
        EEPROM.write(address, currentBalance);
      }

      Serial.print("Access granted to: ");
      Serial.println(authorizedCards[i].name);
      key("Welcome", 0);
      lcd.setCursor(0, 1);
      lcd.print(authorizedCards[i].name);
      lcd.setCursor(0, 3);
      lcd.print("Balance: ");
      lcd.print(currentBalance);
      isAuthorized = true;
      sessionStartTime = millis();  // Start session timer
      break;
    }
  }

  if (!isAuthorized) {
    Serial.println("Access denied: Unauthorized card");
    key("Access denied", 1);
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Please scan card");
    lcd.setCursor(0, 1);
    lcd.print("to activate keypad...");
  }
}

// Dispense water for a given duration
void dispenseWater(int duration) {
  Serial.print("Dispensing water for ");
  lcd.setCursor(0, 0);
  lcd.print("Dispensing water ...");
  Serial.print(duration / 1000);
  Serial.println(" seconds.");

  digitalWrite(valvePin, HIGH);  // Open the valve
  delay(duration);  // Dispense for the given time
  digitalWrite(valvePin, LOW);   // Close the valve

  // Update the balance
  int balanceDeduction = duration / dispenseInterval;
  currentBalance -= balanceDeduction;
  Serial.print("Remaining Balance: ");
  Serial.println(currentBalance);
  key("Remaining Balance: ", 2);
  lcd.setCursor(0, 3);
  lcd.print(currentBalance);

  // Save updated balance to EEPROM
  if (currentCardIndex != -1) {
    EEPROM.write(authorizedCards[currentCardIndex].balanceAddress, currentBalance);
  }
}

// Compare the RFID card UID with an authorized UID
bool compareUID(byte *uid, byte uidSize, byte *authorizedUID) {
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] != authorizedUID[i]) {
      return false;  // UIDs don't match
    }
  }
  return true;  // UIDs match
}

void display(String value) {
  lcd.clear();
  lcd.setCursor(4, 1);
  lcd.print(value);
}

void key(String capt, int post) {
  lcd.clear();
  lcd.setCursor(2, post);
  lcd.print(capt);
}

void logout() {
  isAuthorized = false;
  currentCardIndex = -1;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Session ended.");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Please scan card");
  lcd.setCursor(0, 1);
  lcd.print("to activate keypad...");
}
