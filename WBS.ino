#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define SS_PIN_1 10  // RFID Reader 1 SDA
#define RST_PIN_1 9  // RFID Reader 1 RST
#define SS_PIN_2 8   // RFID Reader 2 SDA
#define RST_PIN_2 7  // RFID Reader 2 RST

MFRC522 rfid1(SS_PIN_1, RST_PIN_1);  // Instance for RFID Reader 1
MFRC522 rfid2(SS_PIN_2, RST_PIN_2);  // Instance for RFID Reader 2

int balanceAddressBase = 0;  // EEPROM base address for balance storage
int valvePin = 8;  // Pin to control the water valve (relay)
int initialBalance = 1000;  // Initial balance for all cards

unsigned long lastTime = 0;
const unsigned long dispenseInterval = 1000;  // 1 second interval for balance deduction

void setup() {
  Serial.begin(9600);
  SPI.begin();  // Initialize SPI bus
  rfid1.PCD_Init();  // Initialize RFID Reader 1
  rfid2.PCD_Init();  // Initialize RFID Reader 2
  pinMode(valvePin, OUTPUT);
  digitalWrite(valvePin, LOW);  // Ensure the valve is off by default

  Serial.println("Place your card on a reader...");
}

void loop() {
  // Check for a card on Reader 1
  if (rfid1.PICC_IsNewCardPresent() && rfid1.PICC_ReadCardSerial()) {
    handleCard(rfid1.uid.uidByte, rfid1.uid.size, "Reader 1", rfid1);
    rfid1.PICC_HaltA();  // Stop reading
  }

  // Check for a card on Reader 2
  if (rfid2.PICC_IsNewCardPresent() && rfid2.PICC_ReadCardSerial()) {
    handleCard(rfid2.uid.uidByte, rfid2.uid.size, "Reader 2", rfid2);
    rfid2.PICC_HaltA();  // Stop reading
  }
}

void handleCard(byte *uid, byte size, String readerName, MFRC522 &rfidReader) {
  Serial.print(readerName + ": Card UID: ");
  String cardUid = "";
  for (byte i = 0; i < size; i++) {
    Serial.print(uid[i], HEX);
    cardUid += String(uid[i], HEX);  // Generate UID string
  }
  Serial.println();

  // Retrieve the balance for this card, initializing if it's the first time
  int balance = retrieveCardBalance(uid, size);
  if (balance == -1) {  // Balance is not set yet (initial state)
    balance = initialBalance;  // Set initial balance
    storeCardBalance(uid, size, balance);  // Store the initial balance
    Serial.println("Initial balance set to 1000.");
  }

  Serial.print("Current Balance: ");
  Serial.println(balance);

  // Start dispensing water if balance > 0
  if (balance > 0) {
    digitalWrite(valvePin, HIGH);  // Turn on the valve
    Serial.println("Water dispensing...");

    // Continue dispensing until balance runs out or card is removed
    while (balance > 0) {
      unsigned long currentTime = millis();
      if (currentTime - lastTime >= dispenseInterval) {
        balance--;  // Deduct 1 unit of balance per second
        storeCardBalance(uid, size, balance);  // Update balance in EEPROM
        lastTime = currentTime;
        Serial.print("Remaining Balance: ");
        Serial.println(balance);
        delay (1000);
      }

      // Check if the card is still present
      if (!cardIsStillPresent(rfidReader)) {
        Serial.println("Card removed. Stopping water.");
        break;  // Stop dispensing water if the card is removed
      }
    }

    digitalWrite(valvePin, LOW);  // Turn off the valve when done
    Serial.println("Water stopped.");
  } else {
    Serial.println("Insufficient balance.");
  }
}

// Function to check if the card is still present
bool cardIsStillPresent(MFRC522 &rfidReader) {
  return rfidReader.PICC_IsNewCardPresent() && rfidReader.PICC_ReadCardSerial();
}

int getCardIndex(byte *uid, byte size) {
  // Hashing based on the UID to return a unique index (for 5 cards)
  int hash = 0;
  for (byte i = 0; i < size; i++) {
    hash += uid[i];
  }
  return hash % 5;  // Assume 5 cards maximum
}

void storeCardBalance(byte *uid, byte size, int balance) {
  int cardIndex = getCardIndex(uid, size);
  int address = balanceAddressBase + cardIndex * sizeof(int);
  EEPROM.put(address, balance);
}

int retrieveCardBalance(byte *uid, byte size) {
  int cardIndex = getCardIndex(uid, size);
  int address = balanceAddressBase + cardIndex * sizeof(int);
  int balance;
  EEPROM.get(address, balance);
  if (balance < 0 || balance > 100000) {  // Invalid balance, assume it's uninitialized
    return -1;  // Return -1 to signal that balance is uninitialized
  }
  return balance;
}
