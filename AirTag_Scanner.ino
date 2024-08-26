// AirTag Scanner on ESP32 Cheap Yellow Display
// https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/
// https://github.com/MatthewKuKanich/ESP32-AirTag-Scanner
// https://github.com/MatthewKuKanich/FindMyFlipper

#include <TFT_eSPI.h>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <set>

TFT_eSPI tft = TFT_eSPI();
int scanTime = 1;
BLEScan* pBLEScan;
std::set<String> foundDevices;
unsigned int airTagCount = 0;
int yPosition = 30; // Starting y position for the first AirTag display

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      // raw data
      uint8_t* payLoad = advertisedDevice.getPayload();
      size_t payLoadLength = advertisedDevice.getPayloadLength();

      // searches both "1E FF 4C 00" and "4C 00 12 19" as the payload can differ slightly
      bool patternFound = false;
      for (int i = 0; i <= payLoadLength - 4; i++) {
        if (payLoad[i] == 0x1E && payLoad[i+1] == 0xFF && payLoad[i+2] == 0x4C && payLoad[i+3] == 0x00) {
          patternFound = true;
          break;
        }
        if (payLoad[i] == 0x4C && payLoad[i+1] == 0x00 && payLoad[i+2] == 0x12 && payLoad[i+3] == 0x19) {
          patternFound = true;
          break;
        }
      }

      if (patternFound) {
        String macAddress = advertisedDevice.getAddress().toString().c_str();
        macAddress.toUpperCase();

        if (foundDevices.find(macAddress) == foundDevices.end()) {
          foundDevices.insert(macAddress);
          airTagCount++;

          int rssi = advertisedDevice.getRSSI();

          Serial.println("AirTag found!");
          Serial.print("Tag: ");
          Serial.println(airTagCount);
          Serial.print("MAC Address: ");
          Serial.println(macAddress);
          Serial.print("RSSI: ");
          Serial.print(rssi);
          Serial.println(" dBm");
          Serial.print("Payload Data: ");
          for (size_t i = 0; i < payLoadLength; i++) {
            Serial.printf("%02X ", payLoad[i]);
          }
          Serial.println("\n");

          // Print to display
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.setCursor(5, yPosition);
          tft.printf("Tag %d: %s, RSSI: %d dBm", airTagCount, macAddress.c_str(), rssi);
          yPosition += 10; // Move to the next line for the next AirTag
          // Print payload data
          tft.setCursor(5, yPosition);
          tft.print("PAYLOAD: ");
          tft.setTextColor(TFT_BLUE, TFT_BLACK);
          tft.setTextFont(1);
          for (size_t i = 0; i < payLoadLength; i++) {
            tft.printf("%02X ", payLoad[i]);
          }
          yPosition += 20; // Move to the next line for the next AirTag
        }
      }
    }
};

void setup() {
  tft.init();
  tft.setRotation(1); //This is the display in landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  Serial.begin(115200);
  Serial.println("Scanning for AirTags...");
  int x = 5;
  int y = 10;
  int fontNum = 2; 
  tft.drawString("Scanning for AirTags...", x, y, fontNum); // Left Aligned

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  delay(1000);
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.equals("rescan")) {
      foundDevices.clear();
      airTagCount = 0;
      Serial.println("Rescanning for AirTags...");
    }
  }

  BLEScanResults foundDevicesScan = pBLEScan->start(scanTime, false);
  pBLEScan->clearResults();
  delay(50);
}
