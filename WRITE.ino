#include <SPI.h>
#include <MFRC522.h>

constexpr uint8_t RST_PIN = D3;
constexpr uint8_t SS_PIN  = D4;

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

MFRC522::StatusCode status;

// Buffers
byte bufferLen = 18;
byte readBlockData[18];

// Example data (must be 16 bytes each) add your data to write in RFID card
byte nameData[16]  = {"xyz"};        // Block 4
byte rollData[16]  = {"009"};        // Block 5
byte deptData[16]  = {"dept"};          // Block 6

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Scan RFID card to write data...");
}

void loop() {
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  Serial.println("\n**Card Detected**");
  Serial.print("UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Write data into multiple blocks
  WriteDataToBlock(4, nameData);
  WriteDataToBlock(5, rollData);
  WriteDataToBlock(6, deptData);

  // Read data back
  ReadDataFromBlock(4, readBlockData);
  PrintBlock(4, readBlockData);

  ReadDataFromBlock(5, readBlockData);
  PrintBlock(5, readBlockData);

  ReadDataFromBlock(6, readBlockData);
  PrintBlock(6, readBlockData);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void WriteDataToBlock(int blockNum, byte blockData[]) {
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A,
                                    blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Auth failed for Write: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  status = mfrc522.MIFARE_Write(blockNum, blockData, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Write failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
  } else {
    Serial.print("Data written to block "); Serial.println(blockNum);
  }
}

void ReadDataFromBlock(int blockNum, byte readBlockData[]) {
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A,
                                    blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Auth failed for Read: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  status = mfrc522.MIFARE_Read(blockNum, readBlockData, &bufferLen);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Read failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
  } else {
    Serial.print("Block "); Serial.print(blockNum);
    Serial.println(" read successfully");
  }
}

void PrintBlock(int blockNum, byte data[]) {
  Serial.print("Data in Block "); Serial.print(blockNum); Serial.print(": ");
  for (int i = 0; i < 16; i++) {
    Serial.write(data[i]);
  }
  Serial.println();
}
