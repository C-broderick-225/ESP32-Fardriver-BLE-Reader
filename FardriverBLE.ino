/****************************************************************************
 *
 * ESP32 & Fardriver BLE Controller - Base Code for Serial Output
 *
 * This is a stripped-down version for sharing.
 * It connects to the controller via BLE, receives and decodes data,
 * and outputs the parsed data to the Serial Monitor for testing.
 *
 ****************************************************************************/

// 1. INCLUDE LIBRARIES
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// --- BLE Configuration ---
#define SERVICE_UUID        "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID "0000ffec-0000-1000-8000-00805f9b34fb"

// 2. Fardriver Data Structure & BLE Variables
struct FardriverData {
  float voltage = 0;
  float lineCurrent = 0;
  float power = 0;
  float rpm = 0;
  int16_t rawRpm = 0;
  int gear = 0;
  float speed = 0;
  int controllerTemp = 0;
  int motorTemp = 0;
  int soc = 0;
  bool isRegenFromCurrent = false; // Flag from negative current at 0xE8
};

FardriverData controllerData;
static BLEAdvertisedDevice* myDevice = nullptr;
static BLEClient* pClient = nullptr;
static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
static bool connected = false;
static bool needsToScan = false;

unsigned long lastPacketTime = 0;
unsigned long lastPrintTime = 0;
const unsigned long printInterval = 500; // Print data every 500ms

const uint8_t flash_read_addr[55] = {
  0xE2, 0xE8, 0xEE, 0xE4, 0x06, 0x0C, 0x12, 0xE2, 0xE8, 0xEE, 0x18, 0x1E, 0x24, 0x2A,
  0xE2, 0xE8, 0xEE, 0x30, 0x5D, 0x63, 0x69, 0xE2, 0xE8, 0xEE, 0x7C, 0x82, 0x88, 0x8E,
  0xE2, 0xE8, 0xEE, 0x94, 0x9A, 0xA0, 0xA6, 0xE2, 0xE8, 0xEE, 0xAC, 0xB2, 0xB8, 0xBE,
  0xE2, 0xE8, 0xEE, 0xC4, 0xCA, 0xD0, 0xE2, 0xE8, 0xEE, 0xD6, 0xDC, 0xF4, 0xFA
};

// 3. FUNCTION PROTOTYPES
void processPacket(uint8_t* pData, size_t length);
bool verifyCRC(uint8_t* data, uint16_t length);
bool connectToServer();
void startScan();
void printControllerData();

// 4. BLE CALLBACKS
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  processPacket(pData, length);
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient_cb) {
    connected = true;
    needsToScan = false;
  }
  void onDisconnect(BLEClient* pclient_cb) {
    connected = false;
    if (myDevice != nullptr) {
        delete myDevice;
        myDevice = nullptr;
    }
    needsToScan = true;
  }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      if (myDevice == nullptr) {
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        BLEDevice::getScan()->stop();
        needsToScan = false;
      }
    }
  }
};

// 5. DATA PROCESSING & VALIDATION
void processPacket(uint8_t* packet, size_t length) {
  if (!verifyCRC(packet, length)) return;
  uint8_t id = packet[1] & 0x3F;
  if (id >= sizeof(flash_read_addr)) return;
  uint8_t address = flash_read_addr[id];
  uint8_t* pData = &packet[2];
  switch (address) {
    case 0xE2:
      controllerData.gear = ((pData[0] >> 2) & 0b11) + 1;
      controllerData.rawRpm = (int16_t)((pData[7] << 8) | pData[6]);
      break;
    case 0xE8: {
      float new_voltage = ((pData[1] << 8) | pData[0]) / 10.0f;
      if (new_voltage >= 0 && new_voltage <= 100.0f) { controllerData.voltage = new_voltage; }
      
      // Interpret current as a signed 16-bit integer
      float new_lineCurrent = (int16_t)((pData[5] << 8) | pData[4]) / 4.0f;
      // Adjust the valid range to include negative values for regen
      if (new_lineCurrent >= -100.0f && new_lineCurrent <= 300.0f) { 
          controllerData.lineCurrent = new_lineCurrent; 
      }
      
      // Set the regen flag if the current is negative
      controllerData.isRegenFromCurrent = (controllerData.lineCurrent < 0);
      break;
    }
    case 0xD6: {
      int new_controllerTemp = (int16_t)((pData[11] << 8) | pData[10]);
      if (new_controllerTemp > -20 && new_controllerTemp < 100) { controllerData.controllerTemp = new_controllerTemp; }
      break;
    }
    case 0xF4: {
      int new_motorTemp = (int16_t)((pData[1] << 8) | pData[0]);
      if (new_motorTemp > -20 && new_motorTemp < 200) { controllerData.motorTemp = new_motorTemp; }
      controllerData.soc = pData[3];
      break;
    }
  }
  
  const float wheel_circumference_m = 1.416;
  const int motor_pole_pairs = 20;

  int16_t displayRawRpm = (controllerData.rawRpm < 0) ? 0 : controllerData.rawRpm;
  controllerData.rpm = displayRawRpm * 4.0 / motor_pole_pairs;
  controllerData.speed = (controllerData.rpm * wheel_circumference_m * 60.0) / 1000.0;
  
  // Power will now be negative during regen, which is correct
  if (controllerData.voltage > 0) {
    controllerData.power = controllerData.voltage * controllerData.lineCurrent;
  }
  
  lastPacketTime = millis();
}

bool verifyCRC(uint8_t* data, uint16_t length) {
  if (length != 16) return false;
  uint16_t crc = 0x7F3C;
  for (int i = 0; i < 14; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc == ((data[15] << 8) | data[14]);
}

// 6. BLE HELPER FUNCTIONS
void startScan() {
  if (myDevice != nullptr) {
    delete myDevice;
    myDevice = nullptr;
  }
  BLEDevice::getScan()->start(0, nullptr, false);
}

bool connectToServer() {
  if (!pClient->connect(myDevice)) {
    return false;
  }
  BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (!pRemoteService) {
    pClient->disconnect();
    return false;
  }
  pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
  if (!pRemoteCharacteristic) {
    pClient->disconnect();
    return false;
  }
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }
  return true;
}

// 7. SERIAL OUTPUT FUNCTION
void printControllerData() {
  Serial.println("--- Fardriver Controller Data ---");
  Serial.print("Voltage: "); Serial.print(controllerData.voltage, 1); Serial.println(" V");
  Serial.print("Line Current: "); Serial.print(controllerData.lineCurrent, 1); Serial.println(" A");
  Serial.print("Power: "); Serial.print(controllerData.power, 0); Serial.println(" W");
  Serial.print("RPM: "); Serial.print(controllerData.rpm, 0); Serial.println("");
  Serial.print("Gear: "); Serial.println(controllerData.gear);
  Serial.print("Speed: "); Serial.print(controllerData.speed, 1); Serial.println(" km/h");
  Serial.print("Controller Temp: "); Serial.print(controllerData.controllerTemp); Serial.println(" C");
  Serial.print("Motor Temp: "); Serial.print(controllerData.motorTemp); Serial.println(" C");
  Serial.print("SOC: "); Serial.print(controllerData.soc); Serial.println(" %");
  Serial.print("Regen (Current): "); Serial.println(controllerData.isRegenFromCurrent ? "Yes" : "No");
  Serial.println("--------------------------------");
}

// 8. SETUP FUNCTION
void setup(void) {
  Serial.begin(115200);
  Serial.println("Starting ESP32 Fardriver BLE Client...");

  BLEDevice::init("ESP32 Fardriver Client");
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  needsToScan = true;
}

// 9. MAIN LOOP
void loop(void) {
  if (needsToScan && !connected) {
    startScan();
    needsToScan = false;
  }

  if (myDevice != nullptr && !connected) {
    if (connectToServer()) {
      Serial.println("Connected to Fardriver Controller");
    } else {
      Serial.println("Failed to connect, retrying...");
      delete myDevice;
      myDevice = nullptr;
    }
  }
  
  if (connected) {
    unsigned long currentTime = millis();
    if (currentTime - lastPrintTime > printInterval) {
      printControllerData();
      lastPrintTime = currentTime;
    }
  } else {
    Serial.println("Scanning for Fardriver Controller...");
    delay(2000); // Avoid spamming the serial
  }
  
  delay(20);
}