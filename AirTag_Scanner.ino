#include <TFT_eSPI.h>
#include <SPIFFS.h>
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

// Function to print text with spacing between wrapped lines
void printWithSpacing(TFT_eSPI &tft, const String &text, int x, int &y, int maxWidth) {
    int lineHeight = tft.fontHeight();  // Get the current font height
    int cursorX = x;
    String line = "";

    for (int i = 0; i < text.length(); i++) {
        char c = text[i];
        int cWidth = tft.textWidth(String(c));  // Get the width of the current character

        if (cursorX + cWidth > maxWidth) {
            // If adding this character would exceed the maxWidth, print the line and start a new one
            tft.drawString(line, x, y);
            y += lineHeight + 5; // Move down by the height of the text plus 5 pixels
            line = "";
            cursorX = x;
        }

        line += c;
        cursorX += cWidth;
    }

    // Print any remaining text in the last line
    if (line.length() > 0) {
        tft.drawString(line, x, y);
        y += lineHeight + 5; // Move down after the last line
    }
}

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
          int maxWidth = tft.width() - 10; // Define your maximum width based on the screen size
          
          // Set color for title and print "Tag X"
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.setTextFont(2); // Larger font for title
          printWithSpacing(tft, "Tag " + String(airTagCount) + ": ", 5, yPosition, maxWidth);

          // Set color for tag data and print MAC address and RSSI
          tft.setTextColor(TFT_YELLOW, TFT_BLACK);
          printWithSpacing(tft, macAddress + ", RSSI: " + String(rssi) + " dBm", 5, yPosition, maxWidth);

          // Set color for title and print "Payload"
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.setTextFont(2); // Larger font for title
          printWithSpacing(tft, "PAYLOAD: ", 5, yPosition, maxWidth);

          // Set color for payload data and print payload
          tft.setTextFont(1); // Smaller font for payload
          tft.setTextColor(TFT_BLUE, TFT_BLACK);
          String payload = "";
          for (size_t i = 0; i < payLoadLength; i++) {
              payload += String(payLoad[i], HEX) + " ";
          }
          printWithSpacing(tft, payload, 5, yPosition, maxWidth);
        }
      }
    }
};

void setup() {
  tft.init();
  tft.setRotation(1); // This is the display in landscape
  tft.fillScreen(TFT_BLACK);
  Serial.begin(115200);
  Serial.println("Scanning for AirTags...");
  int x = 5;
  int y = 10;
  int fontNum = 2; 
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
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
      yPosition = 30; // Reset the y position when rescanning
      tft.fillScreen(TFT_BLACK); // Clear the display
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Rescanning for AirTags...", 5, 10, 2);
      Serial.println("Rescanning for AirTags...");
    }
  }

  BLEScanResults foundDevicesScan = pBLEScan->start(scanTime, false);
  pBLEScan->clearResults();
  delay(50);
}
