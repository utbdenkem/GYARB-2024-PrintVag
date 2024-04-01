#include <HX711.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include "TouchScreen.h"


// Pin definitions for TFT display
#define TFT_DC 15
#define TFT_CS 33


// Pin definitions for HX711 load cell amplifier
#define DOUT_PIN 14
#define SCK_PIN 27


// Touchscreen pin definitions
#define YP A0
#define XM A1
#define YM 32
#define XP 12


// Touchscreen calibration values
#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940


// Calibration constants
#define MINPRESSURE 10
#define MAXPRESSURE 1000


// Create objects for TFT display, touchscreen, and load cell amplifier
Adafruit_ILI9341 tft(TFT_CS, TFT_DC); // Initialize TFT display
TouchScreen ts(XP, YP, XM, YM, 300); // Initialize touchscreen
HX711 scale; // Initialize load cell amplifier


// Initial calibration factor for load cell
int calibration_factor = 217450; // Set initial calibration factor


// Button states and weight variables
bool tarePressed = false; // Flag for tare operation
bool calibrationPressed = false; // Flag for calibration operation
bool uiSwitched = false; // Flag for UI switch
bool displayUpdatesPaused = false; // Flag to pause display updates
long weight = 0; // Variable to store weight


void setup() {
 Serial.begin(115200); // Initialize serial communication


 // Set analog read resolution
 analogReadResolution(10); // Set ADC resolution to 10 bits
  // Initialize TFT display
 tft.begin(); // Initialize TFT display
 tft.fillScreen(0x0000); // Fill screen with black


 // Initialize load cell amplifier
 scale.begin(DOUT_PIN, SCK_PIN); // Initialize load cell amplifier
 scale.set_scale(calibration_factor); // Set scale with calibration factor
 scale.tare(); // Tare the scale
  // Draw initial buttons and display calibration factor
 drawButton(20, 210, 200, 40, "Tare/Empty Roll"); // Draw Tare button
 drawButton(20, 260, 200, 40, "Calibration"); // Draw Calibration button
 displayCalibrationFactor(); // Display calibration factor
 calibrationPressed = false; // Reset calibration flag
}


void loop() {
 // Get touch point
 TSPoint p = ts.getPoint(); // Read touchscreen input
 p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width()); // Map x coordinate
 p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height()); // Map y coordinate


 // Check if "Calibration" button is pressed
 if (checkButtonPress(p, 20, 260, 200, 40)) {
   handleCalibrationButton(); // Handle Calibration button press
 }


 // Check if "Tare" button is pressed in normal UI
 else if (!uiSwitched && checkButtonPress(p, 20, 210, 200, 40)) {
   handleTareButton(); // Handle Tare button press
 }


 // Handle calibration UI actions
 else if (uiSwitched) {
   handleCalibrationUI(p); // Handle actions in calibration UI
 }


 // Perform tare operation if requested
 if (tarePressed) {
   scale.tare(); // Perform tare operation
   tarePressed = false; // Reset tare flag
   Serial.println("Tare Done"); // Print tare completion message
 }


 // Update and display weight in normal UI
 if (!calibrationPressed && !displayUpdatesPaused && !uiSwitched) { // Check if the display is on normal UI
   updateAndDisplayWeight(); // Update and display weight
 }
}


// Function to handle "Calibration" button press
void handleCalibrationButton() {
 if (!uiSwitched) {
   switchToCalibrationUI(); // Switch to Calibration UI
   Serial.println("Calibration UI Activated"); // Print activation message
 } else {
   switchToNormalUI(); // Switch to Normal UI
   Serial.println("Normal UI Activated"); // Print activation message
 }
}


// Function to handle "Tare" button press in normal UI
void handleTareButton() {
 tarePressed = true; // Set tare flag
 Serial.println("Taring weight"); // Print tare operation message
}


// Function to handle actions in calibration UI
void handleCalibrationUI(const TSPoint &p) {
 if (checkButtonPress(p, 20, 210, 50, 40)) {
   updateCalibrationFactor(10); // Increase calibration factor
   delay(100); // Delay for stability
 } else if (checkButtonPress(p, 70, 210, 50, 40)) {
   updateCalibrationFactor(1); // Increase calibration factor
   delay(100); // Delay for stability
 } else if (checkButtonPress(p, 120, 210, 50, 40)) {
   updateCalibrationFactor(-1); // Decrease calibration factor
   delay(100); // Delay for stability
 } else if (checkButtonPress(p, 170, 210, 50, 40)) {
   updateCalibrationFactor(-10); // Decrease calibration factor
   delay(100); // Delay for stability
 }


 if (scale.is_ready()) {
   weight = (scale.get_units() * -1000); // Get weight
   Serial.println(weight); // Print weight
   updateCalibrationDisplay(weight); // Update calibration display
 }
}


// Function to switch to calibration UI
void switchToCalibrationUI() {
 uiSwitched = true; // Set UI switch flag
 displayUpdatesPaused = true; // Pause display updates
 clearScreen(); // Clear screen
 drawBackButton(); // Draw "Back" button
 drawCalibrationButtons(); // Draw calibration buttons
 drawCalibrationFactorDisplay(); // Draw calibration factor display
 delay(100); // Add a delay to prevent immediate UI switch
}


// Function to switch to normal UI
void switchToNormalUI() {
 uiSwitched = false; // Reset UI switch flag
 displayUpdatesPaused = false; // Resume display updates
 clearScreen(); // Clear screen
 drawButton(20, 210, 200, 40, "Tare/Empty Roll"); // Draw Tare button
 drawButton(20, 260, 200, 40, "Calibration"); // Draw Calibration button
 displayCalibrationFactor(); // Display calibration factor
 delay(100); // Add a delay to prevent immediate UI switch
}


// Function to update and display weight in normal UI
void updateAndDisplayWeight() {
 if (scale.is_ready()) {
   weight = (scale.get_units() * -1000); // Get weight
   Serial.println(weight); // Print weight
   updateDisplay(weight); // Update display
 }
}


// Function to update calibration factor
void updateCalibrationFactor(int factorChange) {
 calibration_factor += factorChange; // Adjust calibration factor
 drawCalibrationFactorDisplay(); // Update calibration factor display
 Serial.print(factorChange > 0 ? "+" : ""); // Print sign of change
 Serial.println(String(factorChange)); // Print change amount
}


// Function to clear TFT screen
void clearScreen() {
 tft.fillScreen(0x0000); // Fill screen with black
}


// Function to draw a button on TFT display
void drawButton(int x, int y, int width, int height, const char *label) {
 tft.fillRoundRect(x, y, width, height, 8, 0xF81F); // Draw button background
 tft.drawRoundRect(x, y, width, height, 8, 0xFFFF); // Draw button border
 tft.setTextColor(0xFFFF); // Set text color to white
 tft.setTextSize(2); // Set text size
 int16_t x1, y1;
 uint16_t w, h;
 tft.getTextBounds(label, 0, 0, &x1, &y1, &w, &h); // Get text bounds
 int xOffset = x + (width - w) / 2 - x1; // Calculate x offset
 int yOffset = y + (height - h) / 2 - y1; // Calculate y offset
 tft.setCursor(xOffset, yOffset); // Set cursor position
 tft.println(label); // Print label
}


// Function to draw a "Back" button in calibration UI
void drawBackButton() {
 drawButton(20, 260, 200, 40, "Back"); // Draw "Back" button
}


// Function to draw calibration adjustment buttons
void drawCalibrationButtons() {
 drawButton(20, 210, 50, 40, "+10"); // Draw "+10" button
 drawButton(70, 210, 50, 40, "+1"); // Draw "+1" button
 drawButton(120, 210, 50, 40, "-1"); // Draw "-1" button
 drawButton(170, 210, 50, 40, "-10"); // Draw "-10" button
}


// Function to draw and display the calibration factor in calibration UI
void drawCalibrationFactorDisplay() {
 tft.fillRect(20, 90, 240, 100, 0x0000); // Clear calibration factor display area
 tft.setTextSize(2); // Set text size
 tft.setTextColor(0xFFFF); // Set text color to white
 tft.setCursor(7, 65); // Set cursor position
 tft.print("Calibration Factor:"); // Print label
 tft.setTextSize(5); // Set text size
 int16_t x1, y1;
 uint16_t w, h;
 String calibrationText = String(calibration_factor); // Convert calibration factor to string
 tft.getTextBounds(calibrationText.c_str(), 0, 0, &x1, &y1, &w, &h); // Get text bounds
 int calibrationX = max(0, (tft.width() - w) / 2 - x1); // Calculate x position
 tft.setCursor(calibrationX, 100); // Set cursor position
 tft.print(calibration_factor); // Print calibration factor
}


// Function to update and display the weight in normal UI
void updateDisplay(long weight) {
 tft.fillRect(0, 100, 230, 80, 0x0000); // Clear weight display area
 weight = (scale.get_units() * -1000); // Get weight
 tft.setTextSize(3); // Set text size
 tft.setCursor(65, 70); // Set cursor position
 tft.print("Weight:"); // Print label
 String weightText = String(weight); // Convert weight to string
 tft.setTextSize(5); // Set text size
 int16_t x1, y1;
 uint16_t w, h;
 tft.getTextBounds(weightText.c_str(), 0, 0, &x1, &y1, &w, &h); // Get text bounds
 int weightX = max(0, (tft.width() - w) / 2 - x1); // Calculate x position
 tft.setCursor(weightX, 120); // Set cursor position
 tft.print(weight); // Print weight
 tft.println("g"); // Print units
}


// Function to update and display the weight in calibration UI
void updateCalibrationDisplay(long weight) {
 tft.fillRect(0, 150, 240, 30, 0x0000); // Clear calibration display area
 weight = (scale.get_units() * -1000); // Get weight
 tft.setTextSize(2); // Set text size
 int16_t x1, y1;
 uint16_t w, h;
 tft.getTextBounds(String(weight) + "g", 0, 0, &x1, &y1, &w, &h); // Get text bounds
 int weightX = max(0, (tft.width() - w) / 2); // Calculate x position
 tft.setCursor(weightX, 150); // Set cursor position
 tft.print(weight); // Print weight
 tft.println("g"); // Print units
}


// Function to display the calibration factor in normal UI
void displayCalibrationFactor() {
 if (!uiSwitched) { // Check if not in calibration UI
   tft.setTextSize(1); // Set text size
   tft.setCursor(110, 180); // Set cursor position
   tft.print(calibration_factor); // Print calibration factor
 }
}


// Function to check if a button is pressed on the touchscreen
bool checkButtonPress(const TSPoint &p, int x, int y, int width, int height) {
 return (p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height); // Check if touch point is within button bounds
}