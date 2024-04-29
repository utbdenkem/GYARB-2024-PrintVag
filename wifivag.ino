#include <HX711.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <AdafruitIO_WiFi.h>
#include <SPI.h>
#include <TouchScreen.h>

#define TFT_DC 15  // Define the pin for the TFT display data/command pin.
#define TFT_CS 33  // Define the pin for the TFT display chip select pin.

#define DOUT_PIN 14  // Define the pin connected to the HX711 data output.
#define SCK_PIN 27   // Define the pin connected to the HX711 serial clock.

#define YP A0  // Define the pin connected to the touchscreen Y+ pin.
#define XM A1  // Define the pin connected to the touchscreen X- pin.
#define YM 32  // Define the pin connected to the touchscreen Y- pin.
#define XP 12  // Define the pin connected to the touchscreen X+ pin.

#define TS_MINX 150  // Define the minimum X value for touchscreen coordinates.
#define TS_MINY 120  // Define the minimum Y value for touchscreen coordinates.
#define TS_MAXX 920  // Define the maximum X value for touchscreen coordinates.
#define TS_MAXY 940  // Define the maximum Y value for touchscreen coordinates.

#define MINPRESSURE 10    // Define the minimum pressure value for touchscreen.
#define MAXPRESSURE 1000  // Define the maximum pressure value for touchscreen.

#define IO_USERNAME "***"                  // Define the Adafruit IO username.
#define IO_KEY "***"  // Define the Adafruit IO key.
#define WIFI_SSID "***       // Define the WiFi network SSID.
#define WIFI_PASS "***"                     // Define the WiFi network password.

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);  // Create an Adafruit IO instance.

unsigned long dataConnectionTime = 0;                 // Variable to store the last time data was sent.
const unsigned long dataConnectionInterval = 300000;  // Interval between sending data and connection retry.

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);  // Create a TFT display instance.
TouchScreen ts(XP, YP, XM, YM, 300);   // Create a TouchScreen instance.
HX711 scale;                           // Create an HX711 instance for interfacing with the load cell.

int calibration_factor = 217450;  // Calibration factor for the load cell.

bool tarePressed = false;           // Flag indicating whether the tare button is pressed.
bool calibrationPressed = false;    // Flag indicating whether the calibration button is pressed.
bool uiSwitched = false;            // Flag indicating whether the UI is switched.
bool displayUpdatesPaused = false;  // Flag indicating whether display updates are paused.
long weight = 0;                    // Variable to store the weight reading.

const int maxWiFiAttempts = 5;  // Maximum attempts to connect to WiFi.
int wifiAttemptCount = 0;       // Counter for WiFi connection attempts.
const int maxIOAttempts = 5;    // Maximum attempts to connect to Adafruit IO.
int ioAttemptCount = 0;         // Counter for Adafruit IO connection attempts.

void setup() {
  // Begin serial communication
  Serial.begin(115200);  // Initialize serial communication.

  // Set analog read resolution
  analogReadResolution(10);  // Set analog read resolution to 10 bits.

  // Initialize TFT display
  tft.begin();             // Initialize TFT display.
  tft.fillScreen(0x0000);  // Fill screen with black color.
  // Initialize load cell amplifier
  scale.begin(DOUT_PIN, SCK_PIN);       // Initialize HX711 load cell amplifier.
  scale.set_scale(calibration_factor);  // Set calibration factor for the load cell.
  scale.tare();                         // Tare the load cell.

  // Attempt to connect to WiFi
  connection();  // Connect to WiFi and Adafruit IO.

  // Draw initial buttons and display calibration factor
  tareInstruction();                                // Print under the tare button instruction for how to tare
  drawButton(20, 210, 200, 40, "Tare/Empty Roll");  // Draw Tare button on screen.
  drawButton(20, 260, 200, 40, "Calibration");      // Draw Calibration button on screen.
  displayCalibrationFactor();                       // Display calibration factor on screen.
  calibrationPressed = false;                       // Reset calibration flag.
}

void loop() {
  // Get touch point
  TSPoint p = ts.getPoint();                          // Get touchscreen input.
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());   // Map touchscreen X coordinate.
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());  // Map touchscreen Y coordinate.

  // Check WiFi connection every 5 minutes
  if (millis() - dataConnectionTime >= dataConnectionInterval) {  // Check if it's time to send data.
    connection();                                                 // Reconnect if necessary.
    dataConnectionTime = millis();                                // Update last data send time.
  }

  // Check if "Calibration" button is pressed
  if (checkButtonPress(p, 20, 260, 200, 40)) {                        // Check if Calibration button is pressed.
    handleCalibrationButton();                                        // Handle Calibration button press.
  } else if (!uiSwitched && checkButtonPress(p, 20, 210, 200, 40)) {  // Check if Tare button is pressed.
    handleTareButton();                                               // Handle Tare button press.
  } else if (uiSwitched) {                                            // Check if UI is switched.
    handleCalibrationUI(p);                                           // Handle Calibration UI interactions.
  }

  // Perform tare operation
  if (tarePressed) {              // Check if tare operation is requested.
    scale.tare();                 // Perform tare operation.
    tarePressed = false;          // Reset tare flag.
    Serial.println("Tare Done");  // Print tare completion message.
  }

  // Update and display weight in normal UI
  if (!calibrationPressed && !displayUpdatesPaused && !uiSwitched) {  // Check conditions for weight update and display.
    // This condition ensures that we are in the normal user interface mode,
    // where neither calibration mode is activated, display updates are not paused,
    // and the UI has not been switched to calibration mode.
    // If all these conditions are met, the code proceeds to update and display the weight on the TFT screen.
    updateAndDisplayWeight();  // Update and display weight on TFT screen.
  }

  // Check WiFi connection every 5 minutes
  if (millis() - dataConnectionTime >= dataConnectionInterval) {  // Check if it's retry connection.
    if (WiFi.status() != WL_CONNECTED) {                          // Check WiFi connection status.
      connection();                                               // Reconnect to WiFi and Adafruit IO if not connected.
    }
  } else {                                                            // If not time to send data yet.
    if ((millis() - dataConnectionTime) >= dataConnectionInterval) {  // Check if it's time to send data.
      if (WiFi.status() == WL_CONNECTED) {                            // Check if WiFi is connected.
        sendWeightToAdafruitIO(weight);                               // Send weight data to Adafruit IO.
        dataConnectionTime = millis();                                // Update last data send time.
      }
    }
  }

  // Print timer for connection retry or Data send
  if ((millis() - dataConnectionTime) < dataConnectionInterval && WiFi.status() != WL_CONNECTED && io.status() < AIO_CONNECTED) {  // Check if it's time to connect to WiFi and Adafruit IO.
    Serial.print("Next connection attempt in: ");                                                                                  // Print text for when connection attempt will take place message.
    Serial.print(((dataConnectionInterval - (millis() - dataConnectionTime)) / 1000));                                             // Print how long until next connection attempt will happen.
    Serial.println(" seconds");                                                                                                    // Print text "seconds"
  } else {
    Serial.print("Next Data send in: ");                                                // Print text for when weight will be sent to Adafruit IO.
    Serial.print(((dataConnectionInterval - (millis() - dataConnectionTime)) / 1000));  // Print how long until next weight is sent.
    Serial.println(" seconds");                                                         // Print text "seconds"
  }
}

// Function to attempt connecting to WiFi and Adafruit IO multiple times
void connection() {
  wifiAttemptCount = 0;                                                          // Reset WiFi connection attempt counter.
  while (wifiAttemptCount < maxWiFiAttempts && WiFi.status() != WL_CONNECTED) {  // Attempt WiFi connection.
    WiFi.begin(WIFI_SSID, WIFI_PASS);                                            // Begin WiFi connection.
    clearScreen();                                                               // Clear TFT screen.
    tft.setTextSize(2);                                                          // Set text size for TFT display.
    tft.setCursor(10, 180);                                                      // Set cursor position for TFT display.
    tft.print("Connecting to WiFi");                                             // Print connecting message on TFT display.
    Serial.println("Connecting to WiFi...");                                     // Print connecting message on serial monitor.
    delay(5000);                                                                 // Wait for 5 seconds.
    wifiAttemptCount++;                                                          // Increment WiFi connection attempt counter.
  }

  if (WiFi.status() != WL_CONNECTED) {                                                                // Check if WiFi connection failed.
    clearScreen();                                                                                    // Clear TFT screen.
    tft.setCursor(10, 180);                                                                           // Set cursor position for TFT display.
    tft.print("Failed to Connect");                                                                   // Print connection failure message on TFT display.
    tft.setCursor(10, 200);                                                                           // Set cursor position for TFT display.
    tft.print("Retrying in 5 min");                                                                   // Print retry message on TFT display.
    Serial.println("Failed to connect to WiFi. Continuing without internet. Retrying in 5 minutes");  // Print connection failure message on serial monitor.
    delay(1000);                                                                                      // Wait for 1 second.
    clearScreen();                                                                                    // Clear TFT screen.
    WiFi.disconnect(true);                                                                            // Disconnect WiFi connection.
  } else {                                                                                            // If WiFi connected successfully.
    int ioAttemptCount = 0;                                                                           // Initialize Adafruit IO connection attempt counter.
    while (ioAttemptCount < maxIOAttempts && io.status() < AIO_CONNECTED) {                           // Attempt Adafruit IO connection.
      io.connect();                                                                                   // Connect to Adafruit IO.
      clearScreen();                                                                                  // Clear TFT screen.
      tft.setCursor(20, 180);                                                                         // Set cursor position for TFT display.
      tft.print("Connecting to IO");                                                                  // Print connecting message on TFT display.
      Serial.println(io.statusText());                                                                // Print Adafruit IO connection status on serial monitor.
      delay(5000);                                                                                    // Wait for 5 seconds.
      ioAttemptCount++;                                                                               // Increment Adafruit IO connection attempt counter.
    }

    if (io.status() != AIO_CONNECTED) {                                                                           // Check if Adafruit IO connection failed.
      clearScreen();                                                                                              // Clear TFT screen.
      tft.setCursor(10, 180);                                                                                     // Set cursor position for TFT display.
      tft.print("Failed to Connect");                                                                             // Print connection failure message on TFT display.
      tft.setCursor(10, 200);                                                                                     // Set cursor position for TFT display.
      tft.print("Retrying in 5 min");                                                                             // Print retry message on TFT display.
      Serial.println("Failed to connect to Adafruit IO. Continuing without Adafruit IO. Retrying in 5 minutes");  // Print connection failure message on serial monitor.
      delay(1000);                                                                                                // Wait for 1 second.
      clearScreen();                                                                                              // Clear TFT screen.
      WiFi.disconnect(true);                                                                                      // Disconnect WiFi connection.
    } else {                                                                                                      // If Adafruit IO connected successfully.
      clearScreen();                                                                                              // Clear TFT screen.
      tft.setCursor(10, 180);                                                                                     // Set cursor position for TFT display.
      tft.print("WiFi Connected");                                                                                // Print WiFi connection status on TFT display.
      tft.setCursor(10, 200);                                                                                     // Set cursor position for TFT display.
      tft.print("Io Connected");                                                                                  // Print Adafruit IO connection status on TFT display.
      Serial.println("WiFi Connected. Adafruit Connected");                                                       // Print WiFi and Adafruit IO connection status on serial monitor.
      sendWeightToAdafruitIO(weight);                                                                             // Send weight data to Adafruit IO.
      delay(2500);                                                                                                // Wait for 2.5 seconds.
      clearScreen();                                                                                              // Clear TFT screen.
    }
  }

  switchToNormalUI();             // Switch to normal UI mode.
  dataConnectionTime = millis();  // Update last data send time.
}

// Function to send weight to Adafruit IO
void sendWeightToAdafruitIO(long weight) {
  AdafruitIO_Feed *weightFeed = io.feed("vikt");  // Get Adafruit IO feed for weight data.
  weightFeed->save(weight);                       // Save weight data to Adafruit IO feed.
}

// Function to handle press on "Calibration" button
void handleCalibrationButton() {
  if (!uiSwitched) {                             // If not already in calibration UI mode.
    switchToCalibrationUI();                     // Switch to calibration UI mode.
    Serial.println("Calibration UI Activated");  // Print calibration UI activation message on serial monitor.
  } else {                                       // If already in calibration UI mode.
    switchToNormalUI();                          // Switch to normal UI mode.
    Serial.println("Normal UI Activated");       // Print normal UI activation message on serial monitor.
  }
}

// Function to handle press on "Tare" button in normal interface
void handleTareButton() {
  tarePressed = true;               // Set tare flag to true.
  Serial.println("Taring weight");  // Print tare operation message on serial monitor.
}

// Function to handle actions in calibration UI
void handleCalibrationUI(const TSPoint &p) {
  if (checkButtonPress(p, 20, 210, 50, 40)) {          // Check if +10 button is pressed.
    updateCalibrationFactor(10);                       // Update calibration factor by +10.
    delay(100);                                        // Delay for debouncing.
  } else if (checkButtonPress(p, 70, 210, 50, 40)) {   // Check if +1 button is pressed.
    updateCalibrationFactor(1);                        // Update calibration factor by +1.
    delay(100);                                        // Delay for debouncing.
  } else if (checkButtonPress(p, 120, 210, 50, 40)) {  // Check if -1 button is pressed.
    updateCalibrationFactor(-1);                       // Update calibration factor by -1.
    delay(100);                                        // Delay for debouncing.
  } else if (checkButtonPress(p, 170, 210, 50, 40)) {  // Check if -10 button is pressed.
    updateCalibrationFactor(-10);                      // Update calibration factor by -10.
    delay(100);                                        // Delay for debouncing.
  }
  if (scale.is_ready()) {                  // Check if the scale is ready.
    weight = (scale.get_units() * -1000);  // Get the weight reading from the scale.
    Serial.println(weight);                // Print weight reading on serial monitor.
    updateCalibrationDisplay(weight);      // Update calibration display with new weight reading.
  }
}

// Function to switch to calibration UI
void switchToCalibrationUI() {
  uiSwitched = true;               // Set UI switch flag to true.
  displayUpdatesPaused = true;     // Pause display updates.
  clearScreen();                   // Clear TFT screen.
  calibrationInstruction();        // Show the Calibration instructions.
  clearScreen();                   // Clear TFT screen.
  drawBackButton();                // Draw back button on TFT screen.
  drawCalibrationButtons();        // Draw calibration buttons on TFT screen.
  drawCalibrationFactorDisplay();  // Draw calibration factor display on TFT screen.
  delay(100);                      // Delay for stability.
}

// Function to switch to normal UI
void switchToNormalUI() {
  uiSwitched = false;                               // Set UI switch flag to false.
  displayUpdatesPaused = false;                     // Resume display updates.
  clearScreen();                                    // Clear TFT screen.
  tareInstruction();                                // Print under the tare button instruction for how to tare
  drawButton(20, 210, 200, 40, "Tare/Empty Roll");  // Draw Tare button on TFT screen.
  drawButton(20, 260, 200, 40, "Calibration");      // Draw Calibration button on TFT screen.
  displayCalibrationFactor();                       // Display calibration factor on TFT screen.
  delay(100);                                       // Delay for stability.
}

// Function for showing calibration instruction
void calibrationInstruction() {
  tft.setTextSize(2);               // Set a smaller text size for better fit.
  tft.setCursor(5, 90);             // Set cursor position for the first line
  tft.print("Change the");          // First part of the first line
  tft.setCursor(5, 110);            // Set cursor position for the second part of the first line
  tft.print("calibration factor");  // Second part of the first line
  tft.setCursor(5, 130);            // Set cursor position for the second line
  tft.print("until the weight");    // Third part of the second line
  tft.setCursor(5, 150);            // Set cursor position for the second part of the second line
  tft.print("on the screen");       // Fourth part of the second line
  tft.setCursor(5, 170);            // Set cursor position for the third line
  tft.print("matches");           // Fifth part of the text
  tft.setCursor(5, 190);            // Set cursor position for the third line
  tft.print("500 gram");            // Sixth part of the text
  delay(5000);                      // Time so that user can read the text
}

// Function for showing tare instruction
void tareInstruction() {
  tft.setTextSize(2);                          // Set a smaller text size for better fit.
  tft.setCursor(25, 160);                       // Set cursor position for the instructions
  tft.print("Place empty roll");  // First part of the first line
  tft.setCursor(40, 180);                       // Set cursor position for the instructions
  tft.print("before taring");                  // Second part of the second line
}

// Function to update and display weight in normal UI
void updateAndDisplayWeight() {
  if (scale.is_ready()) {                  // If HX711 is ready.
    weight = (scale.get_units() * -1000);  // Get weight reading from HX711.
    Serial.println(weight);                // Print weight reading on serial monitor.
    updateDisplay(weight);                 // Update and display weight on TFT screen.
  }
}

// Function to update calibration factor
void updateCalibrationFactor(int factorChange) {
  calibration_factor += factorChange;         // Update calibration factor.
  drawCalibrationFactorDisplay();             // Redraw calibration factor display.
  Serial.print(factorChange > 0 ? "+" : "");  // Print sign of change.
  Serial.println(String(factorChange));       // Print the factor change value.
}

// Function to clear the TFT screen
void clearScreen() {
  tft.fillScreen(0x0000);  // Fill TFT screen with black color.
}

// Function to draw a button on the TFT screen
void drawButton(int x, int y, int width, int height, const char *label) {
  tft.fillRoundRect(x, y, width, height, 8, 0xF81F);  // Draw filled rounded rectangle for button.
  tft.drawRoundRect(x, y, width, height, 8, 0xFFFF);  // Draw rounded rectangle border for button.
  tft.setTextColor(0xFFFF);                           // Set text color to white.
  tft.setTextSize(2);                                 // Set text size for button label.
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);  // Get bounding box of button label.
  int xOffset = x + (width - w) / 2 - x1;            // Calculate x offset for centering label.
  int yOffset = y + (height - h) / 2 - y1;           // Calculate y offset for centering label.
  tft.setCursor(xOffset, yOffset);                   // Set cursor position for button label.
  tft.println(label);                                // Print button label.
}

// Function to draw a "Back" button in the calibration interface
void drawBackButton() {
  drawButton(20, 260, 200, 40, "Back");  // Draw back button on TFT screen.
}

// Function to draw buttons for calibration adjustments
void drawCalibrationButtons() {
  drawButton(20, 210, 50, 40, "+10");   // Draw +10 button on TFT screen.
  drawButton(70, 210, 50, 40, "+1");    // Draw +1 button on TFT screen.
  drawButton(120, 210, 50, 40, "-1");   // Draw -1 button on TFT screen.
  drawButton(170, 210, 50, 40, "-10");  // Draw -10 button on TFT screen.
}

// Function to draw and display the calibration factor in the calibration interface
void drawCalibrationFactorDisplay() {
  tft.fillRect(20, 90, 240, 100, 0x0000);  // Clear area for calibration factor display.
  tft.setTextSize(2);                      // Set text size for calibration factor display.
  tft.setTextColor(0xFFFF);                // Set text color to white.
  tft.setCursor(7, 65);                    // Set cursor position for calibration factor label.
  tft.print("Calibration Factor:");        // Print calibration factor label.
  tft.setTextSize(5);                      // Set text size for calibration factor value.
  int16_t x1, y1;
  uint16_t w, h;
  String calibrationText = String(calibration_factor);                 // Convert calibration factor to string.
  tft.getTextBounds(calibrationText.c_str(), 0, 0, &x1, &y1, &w, &h);  // Get bounding box of calibration factor value.
  int calibrationX = max(0, (tft.width() - w) / 2 - x1);               // Calculate x position for centering value.
  tft.setCursor(calibrationX, 100);                                    // Set cursor position for calibration factor value.
  tft.print(calibration_factor);                                       // Print calibration factor value.
}

// Function to update and display the weight in normal interface
void updateDisplay(long weight) {
  tft.fillRect(0, 80, 230, 60, 0x0000);  // Clear area for weight display.
  weight = (scale.get_units() * -1000);   // Get weight reading from HX711.
  tft.setTextSize(3);                     // Set text size for weight label.
  tft.setCursor(65, 30);                  // Set cursor position for weight label.
  tft.print("Weight:");                   // Print weight label.
  String weightText = String(weight);     // Convert weight to string.
  tft.setTextSize(5);                     // Set text size for weight value.
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(weightText.c_str(), 0, 0, &x1, &y1, &w, &h);  // Get bounding box of weight value.
  int weightX = max(0, (tft.width() - w) / 2 - x1);               // Calculate x position for centering value.
  tft.setCursor(weightX, 80);                                    // Set cursor position for weight value.
  tft.print(weight);                                              // Print weight value.
  tft.println("g");                                               // Print unit (grams).
}

// Function to update and display the weight in the calibration interface
void updateCalibrationDisplay(long weight) {
  tft.fillRect(0, 150, 240, 30, 0x0000);  // Clear area for calibration display.
  weight = (scale.get_units() * -1000);   // Get weight reading from HX711.
  tft.setTextSize(2);                     // Set text size for calibration value.
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(String(weight) + "g", 0, 0, &x1, &y1, &w, &h);  // Get bounding box of calibration value.
  int weightX = max(0, (tft.width() - w) / 2);                      // Calculate x position for centering value.
  tft.setCursor(weightX, 150);                                      // Set cursor position for calibration value.
  tft.print(weight);                                                // Print calibration value.
  tft.println("g");                                                 // Print unit (grams).
}

// Function to display the calibration factor in normal interface
void displayCalibrationFactor() {
  if (!uiSwitched) {                // If not in calibration UI mode.
    tft.setTextSize(1);             // Set text size for calibration factor.
    tft.setCursor(110, 140);        // Set cursor position for calibration factor.
    tft.print(calibration_factor);  // Print calibration factor.
  }
}

// Function to check if a button is pressed on the touchscreen
bool checkButtonPress(const TSPoint &p, int x, int y, int width, int height) {
  return (p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height);  // Check if touchscreen press is within button area.
}
