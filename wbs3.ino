#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 4);

#define SS_PIN 10      // RFID Reader SDA pin
#define RST_PIN 9      // RFID Reader RST pin
#define valvePin 8     // Pin for the water valve

// Initialize RFID reader
MFRC522 rfid(SS_PIN, RST_PIN);

// Define Keypad
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {32, 33, 34, 35};
byte colPins[COLS] = {40, 41, 42};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Define multiple authorized UIDs with associated names and EEPROM addresses
struct AuthorizedCard {
  byte uid[4];
  const char* name;
  int balanceAddress;
};

// Authorized cards with names and EEPROM addresses for balance storage
AuthorizedCard authorizedCards[] = {
  {{0xB3, 0x46, 0xD8, 0xA9}, "Mackline Yanet", 0},
  {{0x63, 0xE9, 0x7C, 0xA9}, "Mary Mbise", 4},
  {{0x03, 0xDD, 0xB3, 0xA9}, "Juliana Mwenda", 8},
  {{0x23, 0x08, 0x35, 0xAA}, "Arthur Daniel", 12},
  {{0x33, 0xC5, 0x0E, 0xAA}, "PROF HATIBU", 16},
  {{0x03, 0x85, 0x92, 0xA9}, "Mariki_jr", 20},
  {{0x7C, 0x58, 0x38, 0x22}, "3lue.jl", 24},
  {{0x93, 0x77, 0x05, 0xC8}, "!|", 28},
  {{0x23, 0x6D, 0x7F, 0x11}, "Ubuntu.js", 32},
  {{0x55, 0xD9, 0xAE, 0x2A}, "TEAM...", 36}
};
const int numCards = sizeof(authorizedCards) / sizeof(authorizedCards[0]);

// Variables
unsigned long sessionStartTime = 0;
const unsigned long sessionTimeout = 30000;
const unsigned long dispenseInterval = 1000;
bool isAuthorized = false;
int currentBalance = 0;
int currentCardIndex = -1;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(valvePin, OUTPUT);
  digitalWrite(valvePin, LOW);

  lcd.init();
  lcd.clear();
  lcd.backlight();

  Serial.println("System ready. Please scan authorized card...");
  lcd.setCursor(0, 0);
  lcd.print("Please scan card");
  lcd.setCursor(0, 1);
  lcd.print("to activate keypad...");

  // Initialize balances in EEPROM if not set
  for (int i = 0; i < numCards; i++) {
    initializeBalance(authorizedCards[i].balanceAddress);
  }
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

  // Keypad input for dispensing water
  if (isAuthorized) {
    char key = keypad.getKey();
    if (key) {
      handleKeypadInput(key);
    }
  }
}

// Initialize balance in EEPROM if not already set
void initializeBalance(int address) {
  int balance = EEPROM.read(address);
  if (balance == 0 || balance == 255) {
    balance = 1000;
    EEPROM.write(address, balance);
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
      currentBalance = EEPROM.read(address);

      if (currentBalance == 0 || currentBalance == 255) {
        currentBalance = 1000;
        EEPROM.write(address, currentBalance);
      }

      Serial.print("Access granted to: ");
      Serial.println(authorizedCards[i].name);
      display("Welcome", authorizedCards[i].name, currentBalance);
      isAuthorized = true;
      sessionStartTime = millis();
      return;
    }
  }
  
  Serial.println("Access denied: Unauthorized card");
  display("Access Denied", "", 0);
}

// Handle Keypad Input
void handleKeypadInput(char key) {
  Serial.print("Key Pressed: ");
  Serial.println(key);

  if (key >= '0' && key <= '9' && currentBalance > 0) {
    int dispenseTime = (key - '0') * 1000;
    dispenseWater(dispenseTime);
    logout();
  } else if (key == '#') {
    display("Dispense Canceled", "", 0);
    logout();
  } else {
    display("Insufficient Balance", "", 0);
    logout();
  }
}

// Dispense water for a given duration
void dispenseWater(int duration) {
  Serial.print("Dispensing water for ");
  Serial.print(duration / 1000);
  Serial.println(" seconds.");

  digitalWrite(valvePin, HIGH);
  delay(duration*14);
  digitalWrite(valvePin, LOW);

  int balanceDeduction = duration / dispenseInterval;
  currentBalance -= balanceDeduction;
  Serial.print("Remaining Balance: ");
  Serial.println(currentBalance);

  if (currentCardIndex != -1) {
    EEPROM.write(authorizedCards[currentCardIndex].balanceAddress, currentBalance);
  }
}

// Compare the RFID card UID with an authorized UID
bool compareUID(byte *uid, byte uidSize, byte *authorizedUID) {
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] != authorizedUID[i]) {
      return false;
    }
  }
  return true;
}

// Display function to update LCD
void display(String message, String name, int balance) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
  if (name != "") {
    lcd.setCursor(0, 1);
    lcd.print(name);
    lcd.setCursor(0, 2);
    lcd.print("Balance: ");
    lcd.print(balance);
  }
}

// Logout function
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
