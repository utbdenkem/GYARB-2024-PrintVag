#include <HX711.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <AdafruitIO_WiFi.h>
#include <SPI.h>
#include <TouchScreen.h>

// Pin definitions for TFT display
#define TFT_DC 15
#define TFT_CS 33

// Pin definitions for HX711 load cell amplifier
#define DOUT_PIN 12
#define SCK_PIN 14

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

// Adafruit IO setup
#define IO_USERNAME "DennisTE21B"
#define IO_KEY "aio_AUzH67Ub4blMlnpOVPEVW5Ntg4rp"
#define WIFI_SSID "D410"      // Replace with your WiFi SSID
#define WIFI_PASS "Axel2018"  // Replace with your WiFi password

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// Create objects for TFT display, touchscreen, and load cell amplifier
Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
TouchScreen ts(XP, YP, XM, YM, 300);
HX711 scale;

// Time tracking variables used to know at what time interval it should send data to adafruit io
unsigned long lastDataSentTime = 0;
const unsigned long sendDataInterval = 300000;  // 5 minutes in milliseconds

// Initial calibration factor for load cell
int calibration_factor = 420;

// Button states and weight variables
bool tarePressed = false;
bool calibrationPressed = false;
bool uiSwitched = false;
bool displayUpdatesPaused = false;
long weight = 0;

void setup() {
  Serial.begin(115200);

  // Set analog read resolution
  analogReadResolution(10);

  // Initialize TFT display
  tft.begin();
  tft.fillScreen(0x0000);

  // Initialize load cell amplifier
  scale.begin(DOUT_PIN, SCK_PIN);
  scale.set_scale(calibration_factor);

  // Initialize Adafruit IO
  io.connect();
  while (io.status() < AIO_CONNECTED) {
    Serial.println(io.statusText());
    delay(500);
  }
  //Connecting WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting...");
  }

  // Draw initial buttons and display calibration factor
  drawButton(20, 210, 200, 40, "Tare/Empty Roll");
  drawButton(20, 260, 200, 40, "Calibration");
  displayCalibrationFactor();
  calibrationPressed = false;
}

void loop() {
  // Get touch point
  TSPoint p = ts.getPoint();
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());

  // Check if "Calibration" button is pressed
  if (checkButtonPress(p, 20, 260, 200, 40)) {
    handleCalibrationButton();
  }

  // Check if "Tare" button is pressed in normal UI
  else if (!uiSwitched && checkButtonPress(p, 20, 210, 200, 40)) {
    handleTareButton();
  }

  // Handle calibration UI actions
  else if (uiSwitched) {
    handleCalibrationUI(p);
  }

  // Perform tare operation if requested
  if (tarePressed) {
    scale.tare();
    tarePressed = false;
    Serial.println("Tare Done");
  }

  // Update and display weight in normal UI
  // Send weight to Adafruit IO every 5 minutes
  if (!calibrationPressed && !displayUpdatesPaused && !uiSwitched) {
    if (scale.is_ready()) {
      weight = (scale.get_units(10) * 1000);
      Serial.println(weight);

      // Check if it's time to send data
      unsigned long currentTime = millis();
      if (currentTime - lastDataSentTime >= sendDataInterval) {
        // Send weight to Adafruit IO
        sendWeightToAdafruitIO(weight);

        // Update last data sent time
        lastDataSentTime = currentTime;
      }

      updateDisplay(weight);
    }
  }
}

// Function to handle "Calibration" button press
void handleCalibrationButton() {
  if (!uiSwitched) {
    switchToCalibrationUI();
    Serial.println("Calibration UI Activated");
  } else {
    switchToNormalUI();
    Serial.println("Normal UI Activated");
  }
}

// Function to handle "Tare" button press in normal UI
void handleTareButton() {
  tarePressed = true;
  Serial.println("Taring weight");
}

// Function to handle actions in calibration UI
void handleCalibrationUI(const TSPoint &p) {
  if (checkButtonPress(p, 20, 210, 50, 40)) {
    updateCalibrationFactor(10);
  } else if (checkButtonPress(p, 70, 210, 50, 40)) {
    updateCalibrationFactor(1);
  } else if (checkButtonPress(p, 120, 210, 50, 40)) {
    updateCalibrationFactor(-1);
  } else if (checkButtonPress(p, 170, 210, 50, 40)) {
    updateCalibrationFactor(-10);
  }

  if (scale.is_ready()) {
    weight = (scale.get_units(10) * 1000);
    Serial.println(weight);
    updateCalibrationDisplay(weight);
  }
}

// Function to switch to calibration UI
void switchToCalibrationUI() {
  uiSwitched = true;
  displayUpdatesPaused = true;
  clearScreen();
  drawBackButton();
  drawCalibrationButtons();
  drawCalibrationFactorDisplay();
}

// Function to switch to normal UI
void switchToNormalUI() {
  uiSwitched = false;
  displayUpdatesPaused = false;
  clearScreen();
  drawButton(20, 210, 200, 40, "Tare/Empty Roll");
  drawButton(20, 260, 200, 40, "Calibration");
  displayCalibrationFactor();
}

// Function to update and display weight in normal UI
void updateAndDisplayWeight() {
  if (scale.is_ready()) {
    weight = (scale.get_units(10) * 1000);
    Serial.println(weight);
    updateDisplay(weight);
  }
}

// Function to update calibration factor
void updateCalibrationFactor(int factorChange) {
  calibration_factor += factorChange;
  drawCalibrationFactorDisplay();
  Serial.print(factorChange > 0 ? "+" : "");
  Serial.println(String(factorChange));
}


// Function to clear TFT screen
void clearScreen() {
  tft.fillScreen(0x0000);
}

// Function to draw a button on TFT display
void drawButton(int x, int y, int width, int height, const char *label) {
  tft.fillRoundRect(x, y, width, height, 8, 0xF81F);
  tft.drawRoundRect(x, y, width, height, 8, 0xFFFF);
  tft.setTextColor(0xFFFF);
  tft.setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  int xOffset = x + (width - w) / 2 - x1;
  int yOffset = y + (height - h) / 2 - y1;
  tft.setCursor(xOffset, yOffset);
  tft.println(label);
}

// Function to draw a "Back" button in calibration UI
void drawBackButton() {
  drawButton(20, 260, 200, 40, "Back");
}

// Function to draw calibration adjustment buttons
void drawCalibrationButtons() {
  drawButton(20, 210, 50, 40, "+10");
  drawButton(70, 210, 50, 40, "+1");
  drawButton(120, 210, 50, 40, "-1");
  drawButton(170, 210, 50, 40, "-10");
}

// Function to draw and display the calibration factor in calibration UI
void drawCalibrationFactorDisplay() {
  tft.fillRect(20, 90, 240, 100, 0x0000);
  tft.setTextSize(2);
  tft.setTextColor(0xFFFF);
  tft.setCursor(7, 65);
  tft.print("Calibration Factor:");
  tft.setTextSize(5);
  int16_t x1, y1;
  uint16_t w, h;
  String calibrationText = String(calibration_factor);
  tft.getTextBounds(calibrationText.c_str(), 0, 0, &x1, &y1, &w, &h);
  int calibrationX = max(0, (tft.width() - w) / 2 - x1);
  tft.setCursor(calibrationX, 100);
  tft.print(calibration_factor);
}

// Function to update and display the weight in normal UI
void updateDisplay(long weight) {
  tft.fillRect(0, 100, 230, 80, 0x0000);
  weight = (scale.get_units(10) * 1000);
  tft.setTextSize(3);
  tft.setCursor(65, 70);
  tft.print("Weight:");
  String weightText = String(weight);
  tft.setTextSize(5);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(weightText.c_str(), 0, 0, &x1, &y1, &w, &h);
  int weightX = max(0, (tft.width() - w) / 2 - x1);
  tft.setCursor(weightX, 120);
  tft.print(weight);
  tft.println("g");
}

// Function to update and display the weight in calibration UI
void updateCalibrationDisplay(long weight) {
  tft.fillRect(0, 150, 240, 30, 0x0000);
  weight = (scale.get_units(10) * 1000);
  tft.setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(String(weight) + "g", 0, 0, &x1, &y1, &w, &h);
  int weightX = max(0, (tft.width() - w) / 2);
  tft.setCursor(weightX, 150);
  tft.print(weight);
  tft.println("g");
}

// Function to display the calibration factor in normal UI
void displayCalibrationFactor() {
  if (!uiSwitched) {
    tft.setTextSize(1);
    tft.setCursor(110, 180);
    tft.print(calibration_factor);
  }
}

void sendWeightToAdafruitIO(long weight) {
  AdafruitIO_Feed *weightFeed = io.feed("vikt");
  weightFeed->save(weight);
}

// Function to check if a button is pressed on the touchscreen
bool checkButtonPress(const TSPoint &p, int x, int y, int width, int height) {
  return (p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height);
}
