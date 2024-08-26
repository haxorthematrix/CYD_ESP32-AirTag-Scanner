// The screen on the TTGO is 135X240
#include <TFT_eSPI.h> // Hardware-specific library
#include <driver/adc.h>
#include "esp_adc_cal.h"
#include <painlessMesh.h>
#include "logo.h"

// Mesh network setup
painlessMesh mesh;
constexpr int MESH_PORT = 5555;
const char* const MESH_SSID = "hallouino";
const char* const MESH_PASSWORD = "$$omething$$p00ky";
constexpr int BUTTON1PIN = 35;
constexpr int BUTTON2PIN = 0;
bool screenOn = false; // Variable to keep track of the screen's power state
unsigned long screenTimeout = 30000; // Screen timeout in milliseconds (30 seconds)
unsigned long lastScreenOnTime = 0; // Last time the screen was turned on
volatile bool buttonPressed = false; // Flag to track if the button has been pressed

// ADC calibration characteristics
esp_adc_cal_characteristics_t adc_chars;

// Battery measurement parameters
const float batteryMaxVoltage = 4.2; // Maximum voltage of your battery
const float batteryMinVoltage = 3.2; // Minimum voltage of your battery when it's considered empty
const float voltageMultiplier = 6.8; // Adjusted based on calibration

// Function prototypes
int calculateBatteryPercentage(float voltage);
void drawBatteryIcon(int percentage, bool isCharging);
void updateNodeCountDisplay();
void IRAM_ATTR handleButtonPress();

// Global variables to store the last known state
int lastBatteryPercentage = 0;
size_t lastNodeCount = 0; // Initialize lastNodeCount

TFT_eSPI tft = TFT_eSPI();  // Create an instance of the display

void setup() {
  Serial.begin(115200);
  //pinMode(BUTTON2PIN, INPUT_PULLUP); // Set the button pin as input with pull-up
  // Attach the interrupt to the button pin
  // Use CHANGE for detecting both press and release, or FALLING for press only if the button is active low
  attachInterrupt(digitalPinToInterrupt(BUTTON2PIN), handleButtonPress, FALLING);
  
  // Initialize TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  pinMode(4, OUTPUT); // Set the pin mode for the backlight control

  // Initial screen state
  toggleScreen(true);
  lastScreenOnTime = millis(); // Reset the screen on time

  // Calibrate ADC
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC_ATTEN_DB_2_5, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
  //esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC_ATTEN_DB_2_5, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
  pinMode(14, OUTPUT);

  // Set up the mesh network
  mesh.init(MESH_SSID, MESH_PASSWORD, MESH_PORT);
  mesh.onChangedConnections([]() {
    updateNodeCountDisplay();
  });

  // Initial display update
  updateNodeCountDisplay();
  drawBatteryIcon(lastBatteryPercentage, false); // Initial draw with 0%
}

void loop() {
  mesh.update();
  static unsigned long lastUpdate = 0;
  if (screenOn && millis() - lastScreenOnTime > screenTimeout) {
    Serial.println("TimeOut: Turning screen off"); // For debugging purposes
    toggleScreen(false);
  }
  if (millis() - lastUpdate > 5000) { // Non-blocking delay
    readBattery(lastUpdate);
  }
}

void updateNodeCountDisplay() {
    size_t nodeCount = mesh.getNodeList().size();
    Serial.print("Last Node count: ");
    Serial.println(lastNodeCount);
    if (nodeCount != lastNodeCount) {
        lastNodeCount = nodeCount;// Update the last known node count
        Serial.print("Updated Node count: "); // For debugging purposes
        Serial.println(nodeCount); // This will convert the number to a string and print it
        // Only turn the screen on if it's currently off
        if (!screenOn) {
            toggleScreen(true);
            lastScreenOnTime = millis(); // Update the last screen on time
        }
    }
    // Always update the display with the current node count
    tft.setCursor(0, 40);
    tft.fillRect(0, 40, 160, 16, TFT_BLACK); // Clear the previous text
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    if (nodeCount == 0) {
      tft.printf("Nodes: self"); // Display the current node count (+1 for self)
    } else {
      tft.printf("Nodes: %u\n", (nodeCount + 1)); // Display the current node count (+1 for self)
    }
  
    // Print the current node ID
    tft.setCursor(0, 120);
    tft.setTextSize(1.7);
    tft.print("NodeID: ");
    tft.println(mesh.getNodeId());
}

void readBattery(unsigned long &lastUpdate) {
    lastUpdate = millis();
    digitalWrite(14, HIGH);
    delay(1); // Necessary for the ADC to stabilize
    uint32_t adc_reading = analogRead(34);
    digitalWrite(14, LOW);
     // Convert adc_reading to voltage in mV
    uint32_t voltage_mV = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);
    float battery_voltage = (voltage_mV / 1000.0) * voltageMultiplier; // Convert mV to V and apply the voltage divider multiplier
    Serial.print("Battery Voltage: ");
    Serial.println(battery_voltage);
    // Print the Battery Voltage
    tft.setCursor(0, 100);
    tft.setTextSize(1.7);
    tft.print("Volt: ");
    tft.println(battery_voltage);
    bool isCharging = battery_voltage > batteryMaxVoltage;
    int battery_percentage = calculateBatteryPercentage(battery_voltage);
    if (battery_percentage != lastBatteryPercentage) {
      lastBatteryPercentage = battery_percentage;
      drawBatteryIcon(battery_percentage, isCharging);
    }
}

void drawInitialScreen() {
  tft.fillScreen(TFT_BLACK); // Clear the screen
  // Draw the logo
  // Assuming you have a 135x240 pixel display and you want to place the image on the right
  int imageWidth = 60; // The width of the image
  int imageHeight = 91; // The height of the image
  int imageX = tft.width() - imageWidth; // X coordinate to start image on the right
  int imageY = (tft.height() / 2) - (imageHeight / 2); // Y coordinate to center image vertically
  drawBitmap(imageX, imageY, epd_bitmap_Hallowuino2, 60, 91, TFT_WHITE); // Adjust the position as needed
  drawBatteryIcon(lastBatteryPercentage, false);
  // Draw the node count and node name
  updateNodeCountDisplay();
}

// This function will be called every time the button connected to GPIO0 is pressed
void IRAM_ATTR handleButtonPress() {
  buttonPressed = true;
  Serial.println("Button pressed ISR triggered"); // For debugging purposes
  toggleScreen(true);
}

void toggleScreen(bool turnOn) {
    // Only change the screen state if it's different from the desired state
    if (turnOn != screenOn) {
        Serial.println((turnOn ? "Toggling screen on" : "Toggling screen off")); // Debugging output
        if (turnOn) {
            tft.init(); // Re-initialize the display to turn it on
            digitalWrite(4, HIGH); // Should turn backlight on
            tft.setRotation(1); // Set rotation if necessary
            tft.fillScreen(TFT_BLACK); // Clear the screen
            drawInitialScreen(); // Redraw the initial screen content
            lastScreenOnTime = millis(); // Update the last screen on time
        } else {
            tft.fillScreen(TFT_BLACK); // Clear the screen
            //pinMode(4,OUTPUT); //
            digitalWrite(4,LOW); // Should force backlight off
            tft.writecommand(ST7789_DISPOFF);// Switch off the display
            tft.writecommand(ST7789_SLPIN);// Sleep the display driver
        }
        screenOn = turnOn; // Update the screenOn variable to reflect the new state
    }
}

int calculateBatteryPercentage(float voltage) {
  // If the voltage is above the max, cap it at 100%
  if (voltage > batteryMaxVoltage) {
    return 100;
  }
  // Calculate percentage based on the voltage
  int percentage = (int)((voltage - batteryMinVoltage) / (batteryMaxVoltage - batteryMinVoltage) * 100);
  percentage = constrain(percentage, 0, 100); // Constrain values to between 0 and 100
  return percentage;
}

void drawBatteryIcon(int percentage, bool isCharging) {
  // Define the size and position of the battery icon
  int x = 10; // X position of the battery icon
  int y = 10; // Y position of the battery icon
  int width = 40; // Width of the battery icon
  int height = 20; // Height of the battery icon
  int gap = 2; // Gap for the battery 'cap'

  // Clear the area where the battery icon will be drawn
  tft.fillRect(x, y - gap, width + gap, height + (2 * gap), TFT_BLACK);

  // Draw the battery 'cap'
  tft.fillRect(x + width, y + (height / 4), gap, height / 2, TFT_WHITE);

  // Draw the battery outline
  tft.drawRect(x, y, width, height, TFT_WHITE);

  // Calculate the width of the inner rectangle based on the percentage
  int innerWidth = (percentage * (width - 2)) / 100;

  // Change the color of the battery based on charging state
  uint32_t fillColor = isCharging ? TFT_BLUE : (percentage > 20 ? TFT_GREEN : TFT_RED);

  // Draw the inner rectangle based on the percentage
  tft.fillRect(x + 1, y + 1, innerWidth, height - 2, fillColor);

  // If charging, draw a lightning bolt symbol or similar to indicate charging
  if (isCharging) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(x + width + 5, y + (height / 2) - 4);
    tft.print("↯");
  } else {
    // Optionally, you can add text on top of the battery icon to show the exact percentage
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(x + width + 5, y + (height / 2) - 4);
    tft.printf("%d%%", percentage);
  }
}

// Function to draw bitmap at the given position x,y
void drawBitmap(int16_t x, int16_t y, const unsigned char *bitmap, int16_t w, int16_t h, uint16_t color) {
  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t byte = 0;

  tft.startWrite(); // Start writing to the display
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7) {
        byte <<= 1;
      } else {
        byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      }
      if (byte & 0x80) {
        tft.drawPixel(x + i, y, color);
      }
    }
  }
  tft.endWrite(); // End writing to the display
}
