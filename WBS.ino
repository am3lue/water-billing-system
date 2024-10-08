#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 3    // RFID Reader SDA (GPIO 5)
#define RST_PIN 4   // RFID Reader RST (GPIO 4)
#define VALVE_PIN 2 // Pin to control the water valve (relay)

MFRC522 rfid(SS_PIN, RST_PIN);  // Create MFRC522 instance for RFID Reader

// Wi-Fi credentials
const char* ssid = "ARUSHA_SCIENCE";
const char* password = "science2022";

const int initialBalance = 1000;  // Default balance for all new cards
const unsigned long dispenseInterval = 1000;  // 1-second interval for balance deduction
unsigned long lastDispenseTime = 0;

ESP8266WebServer server(8000);  // Create a server at port 80

// Define EEPROM locations for two cards
#define CARD_1_UID_ADDR 0
#define CARD_1_BALANCE_ADDR (CARD_1_UID_ADDR + 4) // UID is 4 bytes
#define CARD_2_UID_ADDR (CARD_1_BALANCE_ADDR + sizeof(int))
#define CARD_2_BALANCE_ADDR (CARD_2_UID_ADDR + 4)

void setup() {
  Serial.begin(9600);

  // Initialize EEPROM (size is sufficient for two cards)
  EEPROM.begin(512);

  // Initialize RFID reader
  SPI.begin();  
  rfid.PCD_Init();

  // Set valve pin as output
  pinMode(VALVE_PIN, OUTPUT);
  digitalWrite(VALVE_PIN, LOW);  // Ensure the valve is off initially

  // Connect to Wi-Fi
  connectToWiFi();

  // Start the server
  server.on("/", handleRoot);
  server.on("/update", handleUpdate);
  server.begin();
  Serial.println("Server started.");
}

void loop() {
  server.handleClient();

  // Check for RFID card presence and handle water dispensing
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String cardUid = getCardUID(rfid.uid.uidByte, rfid.uid.size);
    Serial.print("Card UID: ");
    Serial.println(cardUid);
    handleWaterDispensing(cardUid);
    rfid.PICC_HaltA();  // Stop reading card
  }
}

// Connect to Wi-Fi
void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Handle the root page
void handleRoot() {
  String html = R"=====(
    <!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Water Billing System</title>
  <style>
    body {
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      margin: 0;
      background-color: #f2f2f2;
      font-family: Arial, sans-serif;
    }
    .container {
      width: 80%;
      max-width: 400px;
      padding: 20px;
      background-color: #fff;
      box-shadow: 0px 4px 12px rgba(0, 0, 0, 0.1);
      border-radius: 8px;
    }
    .input-container {
      margin-bottom: 20px;
    }
    label {
      display: block;
      font-weight: bold;
      margin-bottom: 5px;
    }
    input[type="number"] {
      width: 80%;
      padding: 10px;
      border: 1px solid #ccc;
      border-radius: 4px;
      font-size: 16px;
    }
    input[type="text"] {
      width: 80%;
      padding: 10px;
      border: 1px solid #ccc;
      border-radius: 4px;
      font-size: 16px;
    }
    .balance-container {
      text-align: left;
    }
    input[type="submit"] {
      width: 100%;
      padding: 10px;
      background-color: #2238a3;
      color: white;
      border: none;
      border-radius: 4px;
      cursor: pointer;
      font-size: 16px;
    }
    input[type="submit"]:hover {
      background-color: #3492eb;
    }
  </style>
</head>
<body>
  

  <div class="container">
<h1>Water Billing System Refilling Form</h1>
    <div class="input-container">
      <label for="card-id">Card ID:</label>
      <input type="number" id="card-id" name="card-id">
    </div>
    <div class="input-container">
      <label for="token">Token:</label>
      <input type="text" id="token" name="token">
    </div>
    <div class="input-container balance-container">
      <label for="balance">Balance:</label>
      <input type="number" id="balance" name="balance">
    </div>
    <input type="submit" value="Submit">
  </div>
</body>
</html>

  )=====";
  server.send(200, "text/html", html);
}

// Handle balance update
void handleUpdate() {
  if (server.hasArg("uid") && server.hasArg("balance")) {
    String cardUid = server.arg("uid");
    int newBalance = server.arg("balance").toInt();

    updateBalance(cardUid, newBalance);

    server.send(200, "text/html", "<html><body>Balance updated successfully!</body></html>");

    Serial.println("Balance updated via Wi-Fi.");
  } else {
    server.send(400, "text/html", "<html><body>Invalid parameters.</body></html>");
  }
}

// Handle RFID card detection and water dispensing
void handleWaterDispensing(String cardUid) {
  int balance = getBalance(cardUid);

  if (balance == -1) {
    balance = initialBalance;
    updateBalance(cardUid, balance);
    Serial.println("New card detected. Initial balance set to 1000.");
  }

  Serial.print("Current balance: ");
  Serial.println(balance);

  if (balance > 0) {
    Serial.println("Water dispensing...");
    digitalWrite(VALVE_PIN, HIGH);

    while (balance > 0) {
      unsigned long currentTime = millis();
      if (currentTime - lastDispenseTime >= dispenseInterval) {
        balance--;
        updateBalance(cardUid, balance);
        lastDispenseTime = currentTime;

        Serial.print("Remaining balance: ");
        Serial.println(balance);
      }

      if (!isCardStillPresent()) {
        Serial.println("Card removed. Stopping water.");
        break;
      }
    }

    digitalWrite(VALVE_PIN, LOW);
    Serial.println("Water stopped.");
  } else {
    Serial.println("Insufficient balance.");
  }
}

// Check if card is still present
bool isCardStillPresent() {
  return rfid.PICC_IsNewCardPresent();
}

// Retrieve the card UID as a string
String getCardUID(byte *uid, byte size) {
  String cardUid = "";
  for (byte i = 0; i < size; i++) {
    if (uid[i] < 0x10) {
      cardUid += "0";
    }
    cardUid += String(uid[i], HEX);
  }
  cardUid.toUpperCase();
  return cardUid;
}

// Retrieve the card's balance from EEPROM
int getBalance(String cardUid) {
  if (cardUid == readCardUID(CARD_1_UID_ADDR)) {
    return EEPROM.read(CARD_1_BALANCE_ADDR);
  } else if (cardUid == readCardUID(CARD_2_UID_ADDR)) {
    return EEPROM.read(CARD_2_BALANCE_ADDR);
  }
  return -1; // Card not found
}

// Update the balance of the card in EEPROM
void updateBalance(String cardUid, int balance) {
  if (cardUid == readCardUID(CARD_1_UID_ADDR)) {
    EEPROM.put(CARD_1_BALANCE_ADDR, balance);
  } else if (cardUid == readCardUID(CARD_2_UID_ADDR)) {
    EEPROM.put(CARD_2_BALANCE_ADDR, balance);
  } else {
    // New card: if empty slot, store in first available slot
    if (readCardUID(CARD_1_UID_ADDR) == "") {
      writeCardUID(CARD_1_UID_ADDR, cardUid);
      EEPROM.put(CARD_1_BALANCE_ADDR, balance);
    } else if (readCardUID(CARD_2_UID_ADDR) == "") {
      writeCardUID(CARD_2_UID_ADDR, cardUid);
      EEPROM.put(CARD_2_BALANCE_ADDR, balance);
    } else {
      Serial.println("No available slots for new cards.");
    }
  }
  EEPROM.commit();
  Serial.print("Balance updated: ");
  Serial.println(balance);
}

// Read card UID from EEPROM
String readCardUID(int addr) {
  String uid = "";
  for (int i = 0; i < 4; i++) {
    byte b = EEPROM.read(addr + i);
    if (b == 0xFF) return ""; // Empty slot
    if (b < 0x10) {
      uid += "0";
    }
    uid += String(b, HEX);
  }
  uid.toUpperCase();
  return uid;
}

// Write card UID to EEPROM
void writeCardUID(int addr, String cardUid) {
  for (int i = 0; i < 4; i++) {
    byte uidByte = strtoul(cardUid.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
    EEPROM.write(addr + i, uidByte);
  }
}
