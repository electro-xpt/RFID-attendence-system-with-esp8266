#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- WiFi ----------
const char* ssid     = "<wifi name>"; //add your wifi name
const char* password = "<password>";  // add your wifi password

// ---------- Google Apps Script ----------
String GOOGLE_SCRIPT_URL = "<script url>";  // paste your google script url here

// ---------- RFID ----------
#define RST_PIN D3  
#define SS_PIN  D4   
MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;  // Default key

// ---------- LCD ----------
LiquidCrystal_I2C lcd(0x27,16,2);

// ---------- Buzzer ----------
#define BUZZER_PIN D8

// Blocks for Name, Roll, Dept
const byte BLOCK_NAME = 4;
const byte BLOCK_ROLL = 5;
const byte BLOCK_DEPT = 6;

// Mode
bool writeMode = false;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi...");
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);  // faster WiFi wait
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  lcd.setCursor(0,1);
  lcd.print("WiFi Connected!");
  delay(500);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Scan your card");

  // Default MIFARE key
  for(byte i=0;i<6;i++) key.keyByte[i]=0xFF;

  Serial.println("Enter 'write' in Serial Monitor to store data on RFID.");
}

void loop() {
  // Check Serial for write mode
  if(Serial.available()){
    String input = Serial.readStringUntil('\n');
    input.trim();
    if(input.equalsIgnoreCase("write")){
      writeMode = true;
      Serial.println("Write mode activated. Place RFID tag on reader...");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Write Mode Active");
      lcd.setCursor(0,1);
      lcd.print("Place card...");
    }
  }

  // Check for new card
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  if(writeMode){
    // --- WRITE MODE ---
    String name="", roll="", dept="";
    Serial.println("Enter Name: "); while(!Serial.available()){} name = Serial.readStringUntil('\n'); name.trim();
    Serial.println("Enter Roll: "); while(!Serial.available()){} roll = Serial.readStringUntil('\n'); roll.trim();
    Serial.println("Enter Dept: "); while(!Serial.available()){} dept = Serial.readStringUntil('\n'); dept.trim();

    if(writeBlock(BLOCK_NAME, name) && writeBlock(BLOCK_ROLL, roll) && writeBlock(BLOCK_DEPT, dept)){
      Serial.println("Data written successfully!");
      lcd.clear(); lcd.setCursor(0,0); lcd.print("Write Success");
      buzz(3, 80);  // 3 short beeps
    } else {
      Serial.println("Write failed!");
      lcd.clear(); lcd.setCursor(0,0); lcd.print("Write Failed");
      buzz(3, 150); // 3 long beeps
    }

    writeMode = false;
    lcd.clear(); lcd.setCursor(0,0); lcd.print("Scan your card");
  } else {
    // --- READ MODE ---
    String name = readBlock(BLOCK_NAME);
    String roll = readBlock(BLOCK_ROLL);
    String dept = readBlock(BLOCK_DEPT);

    if(name.length()==0 && roll.length()==0 && dept.length()==0){
      Serial.println("Card is empty or auth failed");
      lcd.clear(); lcd.setCursor(0,0); lcd.print("Invalid Card");
      buzz(2,200);
      delay(500);
      lcd.clear(); lcd.setCursor(0,0); lcd.print("Scan your card");
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      return;
    }

    Serial.println("Name: " + name + " Roll: " + roll + " Dept: " + dept);
    lcd.clear(); lcd.setCursor(0,0); lcd.print(name + " " + roll);
    lcd.setCursor(0,1); lcd.print(dept);
    buzz(2,50);

    sendToGoogleSheets(name, roll, dept);

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    lcd.clear(); lcd.setCursor(0,0); lcd.print("Scan your card");
  }

  delay(50); // minimal delay for fast scanning
}

// ---------- READ BLOCK ----------
String readBlock(byte blockNum){
  byte buffer[18]; byte size=sizeof(buffer);

  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if(status != MFRC522::STATUS_OK){ Serial.print("Auth failed for block "); Serial.println(blockNum); return ""; }

  status = mfrc522.MIFARE_Read(blockNum, buffer, &size);
  if(status != MFRC522::STATUS_OK){ Serial.print("Read failed for block "); Serial.println(blockNum); return ""; }

  String result=""; for(byte i=0;i<16;i++){ if(buffer[i]==0) break; result += char(buffer[i]); }
  result.trim(); return result;
}

// ---------- WRITE BLOCK ----------
bool writeBlock(byte blockNum, String data){
  byte buffer[16]; memset(buffer, 0, 16);
  for(byte i=0;i<data.length() && i<16;i++) buffer[i]=data[i];

  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if(status != MFRC522::STATUS_OK){ Serial.print("Auth failed for block "); Serial.println(blockNum); return false; }

  status = mfrc522.MIFARE_Write(blockNum, buffer, 16);
  if(status != MFRC522::STATUS_OK){ Serial.print("Write failed for block "); Serial.println(blockNum); return false; }

  return true;
}

// ---------- BUZZER ----------
void buzz(int times, int duration){
  for(int i=0;i<times;i++){
    digitalWrite(BUZZER_PIN,HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN,LOW);
    delay(50); // short gap between beeps
  }
}

// ---------- SEND TO GOOGLE SHEETS ----------
void sendToGoogleSheets(String name,String roll,String dept){
  if(WiFi.status()==WL_CONNECTED){
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http;
    String url = GOOGLE_SCRIPT_URL + "?name=" + name + "&roll=" + roll + "&dept=" + dept;
    Serial.println("Requesting URL: "+url);

    http.begin(client,url);
    int httpCode = http.GET();
    if(httpCode>0){ String payload = http.getString(); Serial.println("Response: "+payload); buzz(2,50); }
    else{ Serial.println("Error sending data. HTTP Code: "+String(httpCode)); buzz(2,150); }
    http.end();
  } else Serial.println("WiFi not connected");
}
