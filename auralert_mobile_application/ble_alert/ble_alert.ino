#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
// You define these UUIDs — they just need to be unique.
// Generate your own at https://www.uuidgenerator.net/
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcdefab-cdef-abcd-efab-cdefabcdefab"
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
// Callbacks to track connection state
class ServerCallbacks : public BLEServerCallbacks {
void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
Serial.println("Client connected");
  }
void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
Serial.println("Client disconnected — restarting advertising");
pServer->startAdvertising();
  }
};
void setup() {
Serial.begin(115200);
  BLEDevice::init("ESP32-Alert");          // BLE device name
  pServer = BLEDevice::createServer();
pServer->setCallbacks(new ServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
pCharacteristic->addDescriptor(new BLE2902()); // Required for notifications
pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
pAdvertising->addServiceUUID(SERVICE_UUID);
pAdvertising->start();
Serial.println("BLE advertising started");
}
void loop() {
if (deviceConnected) {
    // Send a message every 5 seconds as a test
    String message = "ALERT:sensor_triggered";
pCharacteristic->setValue(message.c_str());
pCharacteristic->notify();
Serial.println("Sent: " + message);
delay(5000);
  }
}
