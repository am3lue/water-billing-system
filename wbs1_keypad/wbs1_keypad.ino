/* 
made by blue && narka 

*/
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 4);

#define SS_PIN 10  // RFID Reader SDA pin
#define RST_PIN 9  // RFID Reader RST pin
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

byte authorizedCardUID[] = {0xD3, 0x24, 0x01, 0xC8};  // Replace with actual card UID

int balance = 1000;  // Example balance
unsigned long lastTime = 0;
const unsigned long dispenseInterval = 1000;  // 1 second = 1 unit of balance

bool isAuthorized = false;  // New variable to track authorization state

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
 
  // Keypad input for dispensing water (only if authorized)
  if (isAuthorized) {
    char key = keypad.getKey();
    if (key) {
      Serial.print("Key Pressed: ");
      Serial.println(key);
     
      if (balance > 0) {
        if (key >= '0' && key <= '9') {
          int dispenseTime = (key - '0') * 1000;  // Convert keypress to dispensing time in ms (1-9 seconds)
          dispenseWater(dispenseTime);
        } else if (key == '#') {
          Serial.println("Water dispensing canceled.");
          display("Water dispensing canceled.");
          digitalWrite(valvePin, LOW);
          delay(2000);
          lcd.setCursor(0, 0);
          lcd.print("Please scan card");
          lcd.setCursor(0, 1);
          lcd.print("to activate keypad...");
          rfid.PICC_HaltA();  // Stop the valve
        }
      } else {
        Serial.println("Insufficient balance.");
        display("insufficient balance.");
        rfid.PICC_HaltA();
        lcd.setCursor(0, 0);
        lcd.print("Please scan card");
        lcd.setCursor(0, 1);
        lcd.print("to activate keypad...");
      }
    }
  }
  rfid.PICC_HaltA();
}

// Handle the RFID card
void handleCard() {
  String cardUid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    cardUid += String(rfid.uid.uidByte[i], HEX);  // Generate UID string
  }

  Serial.print("Card UID: ");
  Serial.println(cardUid);

  if (compareUID(rfid.uid.uidByte, rfid.uid.size, authorizedCardUID)) {
    Serial.println("Card recognized. Keypad activated.");
    key("Keypad activated.",2);
    Serial.print("Current Balance: ");
    key("Current Balance: ",2);
    lcd.setCursor(0, 3);
    lcd.print(balance);
    Serial.println(balance);
    isAuthorized = true;  // Set the authorization state to true
  } else {
    Serial.println("Access denied. Please use an authorized card.");
    key("Access denied",3);
    delay(2000);
    lcd.setCursor(0, 0);
    lcd.print("Please scan card");
    lcd.setCursor(0, 1);
    lcd.print("to activate keypad...");
    isAuthorized = false;  // Ensure unauthorized cards deactivate the keypad
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
  balance -= balanceDeduction;
  Serial.print("Remaining Balance: ");
  Serial.println(balance);
  key("Remaining Balance: ",2);
  lcd.setCursor(0, 3);
  lcd.print(balance);
}

// Compare the RFID card UID
bool compareUID(byte *uid, byte uidSize, byte *authorizedUID) {
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] != authorizedUID[i]) {
      return false;  // UIDs don't match
    }
  }
  return true;  // UIDs match
}

void display(String value){
  lcd.clear();
  lcd.setCursor(4, 1);
  lcd.print(value);
}
void key(String capt, int post){
  lcd.clear();
  lcd.setCursor(2, post);
  lcd.print(capt);
}
