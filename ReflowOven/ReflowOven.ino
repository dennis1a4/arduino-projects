/**
 * Reflow Oven Controller
 *
 * Arduino-based toaster oven controller for reflow soldering
 * and 3D printer filament drying.
 *
 * Hardware: Wemos D1 Mini (ESP8266)
 * Display: 128x64 SSD1306 OLED
 * Temperature: AD8495 thermocouple amplifier
 *
 * Version: 1.0.0
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

#include "config.h"

// ===========================================
// Enumerations
// ===========================================

enum SystemState {
  STATE_IDLE,
  STATE_PREHEATING,
  STATE_SOAKING,
  STATE_REFLOW,
  STATE_COOLING,
  STATE_HEATING,
  STATE_MAINTAINING,
  STATE_COMPLETE,
  STATE_ERROR
};

enum OperatingMode {
  MODE_IDLE,
  MODE_REFLOW,
  MODE_FILAMENT
};

enum MenuScreen {
  SCREEN_MAIN,
  SCREEN_MENU,
  SCREEN_MODE_SELECT,
  SCREEN_REFLOW_SETTINGS,
  SCREEN_FILAMENT_SETTINGS,
  SCREEN_NETWORK_SETTINGS,
  SCREEN_SYSTEM_INFO,
  SCREEN_EDIT_VALUE,
  SCREEN_CONFIRM_START,
  SCREEN_FILAMENT_TYPE
};

enum FilamentType {
  FILAMENT_CUSTOM,
  FILAMENT_PLA,
  FILAMENT_PETG,
  FILAMENT_ABS,
  FILAMENT_NYLON,
  FILAMENT_TPU
};

enum ErrorCode {
  ERROR_NONE = 0,
  ERROR_OVER_TEMP = 1,
  ERROR_SENSOR_FAIL = 2,
  ERROR_THERMAL_RUNAWAY = 3,
  ERROR_WIFI_LOST = 4,
  ERROR_MQTT_LOST = 5,
  ERROR_INVALID_CMD = 6,
  ERROR_TIMEOUT = 7
};

// ===========================================
// Global Objects
// ===========================================

Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1);
ESP8266WebServer webServer(WEB_SERVER_PORT);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ===========================================
// System State Variables
// ===========================================

volatile SystemState systemState = STATE_IDLE;
OperatingMode operatingMode = MODE_IDLE;
MenuScreen currentScreen = SCREEN_MAIN;
ErrorCode lastError = ERROR_NONE;

// ===========================================
// Temperature Variables
// ===========================================

float currentTemp = 0.0;
float targetTemp = 0.0;
float tempHistory[TEMP_SAMPLE_COUNT];
int tempHistoryIndex = 0;
float tempCalibOffset = TEMP_CALIB_OFFSET;

// Long-term history for graphing
float tempGraphHistory[TEMP_HISTORY_SIZE];
int tempGraphIndex = 0;

// ===========================================
// PID Variables
// ===========================================

float pidKp, pidKi, pidKd;
float pidIntegral = 0.0;
float pidLastError = 0.0;
float pidOutput = 0.0;

// ===========================================
// Timing Variables
// ===========================================

unsigned long operationStartTime = 0;
unsigned long operationDuration = 0;
unsigned long phaseStartTime = 0;
unsigned long lastTempRead = 0;
unsigned long lastPidUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastSerialLog = 0;

// Button timing
unsigned long buttonPressStart = 0;
bool buttonPressed = false;
bool buttonLongPressHandled = false;

// Encoder
volatile int encoderPosition = 0;
volatile int lastEncoderCLK = HIGH;
unsigned long lastEncoderTime = 0;

// SSR PWM timing
unsigned long ssrCycleStart = 0;
bool ssrState = false;
int ssrDutyCycle = 0;  // 0-255

// ===========================================
// Reflow Profile Settings
// ===========================================

int reflowPeakTemp = REFLOW_PEAK_TEMP;
int reflowSoakTemp = REFLOW_PREHEAT_TEMP;
int reflowSoakDuration = REFLOW_SOAK_DURATION;

// ===========================================
// Filament Drying Settings
// ===========================================

FilamentType selectedFilament = FILAMENT_PLA;
int filamentTemp = FILAMENT_PLA_TEMP;
int filamentDuration = FILAMENT_PLA_TIME;  // in minutes

// ===========================================
// Network Settings
// ===========================================

String wifiSSID = DEFAULT_WIFI_SSID;
String wifiPassword = DEFAULT_WIFI_PASSWORD;
String mqttBroker = DEFAULT_MQTT_BROKER;
int mqttPort = DEFAULT_MQTT_PORT;
String mqttUser = DEFAULT_MQTT_USER;
String mqttPassword = DEFAULT_MQTT_PASS;

bool wifiConnected = false;
bool mqttConnected = false;
bool apMode = false;

String deviceId;
unsigned long mqttReconnectDelay = MQTT_RECONNECT_DELAY_BASE;
unsigned long lastMqttReconnectAttempt = 0;

// ===========================================
// Menu Variables
// ===========================================

int menuIndex = 0;
int editValue = 0;
int editMin = 0;
int editMax = 100;
String editLabel = "";
int* editTarget = nullptr;

const char* mainMenuItems[] = {
  "Select Mode",
  "Reflow Settings",
  "Filament Settings",
  "Network Settings",
  "System Info",
  "Back"
};
const int mainMenuCount = 6;

const char* modeMenuItems[] = {
  "Reflow Solder",
  "Filament Drying",
  "Back"
};
const int modeMenuCount = 3;

const char* reflowMenuItems[] = {
  "Peak Temp",
  "Soak Temp",
  "Soak Time",
  "Back"
};
const int reflowMenuCount = 4;

const char* filamentMenuItems[] = {
  "Filament Type",
  "Temperature",
  "Duration",
  "Back"
};
const int filamentMenuCount = 4;

const char* filamentTypeItems[] = {
  "PLA (45C)",
  "PETG (65C)",
  "ABS (70C)",
  "Nylon (80C)",
  "TPU (50C)",
  "Custom",
  "Back"
};
const int filamentTypeCount = 7;

const char* networkMenuItems[] = {
  "WiFi Status",
  "MQTT Status",
  "Back"
};
const int networkMenuCount = 3;

// ===========================================
// Safety Variables
// ===========================================

float heaterOffTemp = 0.0;
unsigned long heaterOffTime = 0;
bool heaterWasOn = false;

// ===========================================
// Function Prototypes
// ===========================================

// Temperature
void readTemperature();
float filterTemperature(float rawTemp);
bool checkSensorFailure(int adcValue);

// PID Control
void updatePID();
void setPIDParams(OperatingMode mode);
void controlSSR();

// Safety
void checkSafetyConditions();
void emergencyShutdown(ErrorCode error);
bool checkOverTemperature();
bool checkThermalRunaway();
bool checkTimeout();

// State Machine
void updateStateMachine();
void startOperation();
void stopOperation();
void transitionToState(SystemState newState);

// Display
void updateDisplay();
void drawMainScreen();
void drawMenuScreen();
void drawEditScreen();
void drawConfirmScreen();
void drawErrorScreen();
void drawProgressBar(int x, int y, int width, int height, int percent);

// Input
void IRAM_ATTR encoderISR();
void handleEncoder();
void handleButton();

// Menu
void handleMenuNavigation(int direction);
void handleMenuSelect();
void handleMenuBack();

// WiFi
void setupWiFi();
void handleWiFiConnection();
void startAPMode();

// MQTT
void setupMQTT();
void handleMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishStatus();
void publishDiscovery();

// Web Server
void setupWebServer();
void handleWebRoot();
void handleWebAPI();
void handleWebConfig();
void handleWebNotFound();

// EEPROM
void loadSettings();
void saveSettings();

// Utilities
String getStateString(SystemState state);
String getModeString(OperatingMode mode);
String formatTime(unsigned long seconds);
String getMacAddress();

// ===========================================
// Setup
// ===========================================

void setup() {
  // Initialize serial
  Serial.begin(SERIAL_BAUD_RATE);
  if (SERIAL_ENABLE_DEBUG) {
    Serial.println();
    Serial.println(F("================================="));
    Serial.println(F("  Reflow Oven Controller v1.0"));
    Serial.println(F("================================="));
  }

  // Initialize I2C
  Wire.begin(PIN_SDA, PIN_SCL);

  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
    Serial.println(F("ERROR: SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 25);
  display.println(F("Reflow Oven"));
  display.setCursor(30, 40);
  display.println(F("Starting..."));
  display.display();

  // Initialize GPIO
  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODER_DT, INPUT_PULLUP);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_SSR, OUTPUT);
  digitalWrite(PIN_SSR, LOW);

  // Initialize encoder interrupt
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_CLK), encoderISR, CHANGE);

  // Initialize temperature array
  for (int i = 0; i < TEMP_SAMPLE_COUNT; i++) {
    tempHistory[i] = 0.0;
  }

  // Initialize EEPROM and load settings
  EEPROM.begin(EEPROM_SIZE);
  loadSettings();

  // Get device ID from MAC address
  deviceId = getMacAddress();
  deviceId.replace(":", "");

  // Initialize WiFi
  setupWiFi();

  // Initialize MQTT
  setupMQTT();

  // Initialize web server
  setupWebServer();

  // Enable watchdog
  ESP.wdtEnable(WATCHDOG_TIMEOUT);

  if (SERIAL_ENABLE_DEBUG) {
    Serial.println(F("Initialization complete"));
    Serial.print(F("Device ID: "));
    Serial.println(deviceId);
  }

  delay(1000);
  display.clearDisplay();
}

// ===========================================
// Main Loop
// ===========================================

void loop() {
  unsigned long currentMillis = millis();

  // Feed watchdog
  ESP.wdtFeed();

  // Read temperature (10 Hz)
  if (currentMillis - lastTempRead >= 100) {
    readTemperature();
    lastTempRead = currentMillis;
  }

  // Safety checks (10 Hz)
  checkSafetyConditions();

  // Update PID (1 Hz)
  if (currentMillis - lastPidUpdate >= 1000) {
    if (systemState != STATE_IDLE && systemState != STATE_ERROR && systemState != STATE_COMPLETE) {
      updatePID();
    }
    lastPidUpdate = currentMillis;

    // Store temperature in graph history
    tempGraphHistory[tempGraphIndex] = currentTemp;
    tempGraphIndex = (tempGraphIndex + 1) % TEMP_HISTORY_SIZE;
  }

  // Control SSR
  controlSSR();

  // Update state machine
  updateStateMachine();

  // Handle user input (50 Hz)
  handleEncoder();
  handleButton();

  // Update display (2 Hz)
  if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = currentMillis;
  }

  // Handle web server
  webServer.handleClient();

  // Handle WiFi connection
  handleWiFiConnection();

  // Handle MQTT
  handleMQTT();

  // Publish MQTT status
  if (currentMillis - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {
    if (mqttConnected) {
      publishStatus();
    }
    lastMqttPublish = currentMillis;
  }

  // Serial logging
  if (SERIAL_TEMP_LOGGING && (currentMillis - lastSerialLog >= SERIAL_LOG_INTERVAL)) {
    Serial.print(F("Temp: "));
    Serial.print(currentTemp, 1);
    Serial.print(F("C, Target: "));
    Serial.print(targetTemp, 1);
    Serial.print(F("C, State: "));
    Serial.print(getStateString(systemState));
    Serial.print(F(", PWM: "));
    Serial.println(ssrDutyCycle);
    lastSerialLog = currentMillis;
  }
}

// ===========================================
// Temperature Functions
// ===========================================

void readTemperature() {
  int adcValue = analogRead(PIN_TEMP_SENSOR);

  // Check for sensor failure
  if (checkSensorFailure(adcValue)) {
    return;
  }

  // Convert ADC to temperature (Adafruit AD8495 with 1.25V offset at 0°C)
  float voltage = adcValue * ADC_REFERENCE_VOLTAGE / 1023.0;
  float rawTemp = (voltage - AD8495_OFFSET_VOLTAGE) / AD8495_MV_PER_C + tempCalibOffset;

  // Apply filtering
  currentTemp = filterTemperature(rawTemp);
}

float filterTemperature(float rawTemp) {
  // Check for outliers
  float sum = 0.0;
  int validCount = 0;
  for (int i = 0; i < TEMP_SAMPLE_COUNT; i++) {
    if (tempHistory[i] > 0) {
      sum += tempHistory[i];
      validCount++;
    }
  }

  if (validCount > 0) {
    float avg = sum / validCount;
    if (abs(rawTemp - avg) > TEMP_OUTLIER_THRESHOLD) {
      // Outlier detected, use previous average
      return avg;
    }
  }

  // Add to rolling average
  tempHistory[tempHistoryIndex] = rawTemp;
  tempHistoryIndex = (tempHistoryIndex + 1) % TEMP_SAMPLE_COUNT;

  // Calculate new average
  sum = 0.0;
  for (int i = 0; i < TEMP_SAMPLE_COUNT; i++) {
    sum += tempHistory[i];
  }

  return sum / TEMP_SAMPLE_COUNT;
}

bool checkSensorFailure(int adcValue) {
  // Open thermocouple
  if (adcValue > SENSOR_OPEN_THRESHOLD) {
    emergencyShutdown(ERROR_SENSOR_FAIL);
    return true;
  }

  // Shorted thermocouple (only check if heater has been on)
  if (adcValue < SENSOR_SHORT_THRESHOLD && heaterWasOn) {
    emergencyShutdown(ERROR_SENSOR_FAIL);
    return true;
  }

  return false;
}

// ===========================================
// PID Control Functions
// ===========================================

void updatePID() {
  float error = targetTemp - currentTemp;

  // Proportional term
  float pTerm = pidKp * error;

  // Integral term (with anti-windup)
  pidIntegral += error;
  pidIntegral = constrain(pidIntegral, -100.0, 100.0);
  float iTerm = pidKi * pidIntegral;

  // Derivative term
  float dTerm = pidKd * (error - pidLastError);
  pidLastError = error;

  // Calculate output
  pidOutput = pTerm + iTerm + dTerm;
  pidOutput = constrain(pidOutput, 0.0, 100.0);

  // Apply control logic from FSD
  if (currentTemp < targetTemp - 10) {
    // Full power when far from target
    ssrDutyCycle = 255;
  } else if (currentTemp > targetTemp) {
    // Off when above target
    ssrDutyCycle = 0;
  } else {
    // PID control when within 10 degrees
    ssrDutyCycle = (int)(pidOutput * 2.55);  // Scale 0-100 to 0-255
  }
}

void setPIDParams(OperatingMode mode) {
  if (mode == MODE_REFLOW) {
    pidKp = REFLOW_KP;
    pidKi = REFLOW_KI;
    pidKd = REFLOW_KD;
  } else {
    pidKp = FILAMENT_KP;
    pidKi = FILAMENT_KI;
    pidKd = FILAMENT_KD;
  }

  // Reset PID state
  pidIntegral = 0.0;
  pidLastError = 0.0;
  pidOutput = 0.0;
}

void controlSSR() {
  unsigned long currentMillis = millis();

  // 1 Hz PWM cycle (1 second period)
  unsigned long cyclePosition = currentMillis - ssrCycleStart;

  if (cyclePosition >= 1000) {
    ssrCycleStart = currentMillis;
    cyclePosition = 0;
  }

  // Calculate ON time in ms (duty cycle is 0-255, period is 1000ms)
  unsigned long onTime = (ssrDutyCycle * 1000UL) / 255;

  // Apply minimum ON time
  if (onTime > 0 && onTime < PWM_MIN_ON_TIME) {
    onTime = PWM_MIN_ON_TIME;
  }

  // Control SSR
  bool newState = (cyclePosition < onTime) && (ssrDutyCycle > 0);

  if (newState != ssrState) {
    ssrState = newState;
    digitalWrite(PIN_SSR, ssrState ? HIGH : LOW);

    // Track heater state for thermal runaway detection
    if (ssrState) {
      heaterWasOn = true;
    } else if (heaterWasOn && !ssrState) {
      heaterOffTemp = currentTemp;
      heaterOffTime = currentMillis;
    }
  }
}

// ===========================================
// Safety Functions
// ===========================================

void checkSafetyConditions() {
  if (systemState == STATE_IDLE || systemState == STATE_ERROR) {
    return;
  }

  if (checkOverTemperature()) return;
  if (checkThermalRunaway()) return;
  if (checkTimeout()) return;
}

bool checkOverTemperature() {
  float maxTemp = (operatingMode == MODE_REFLOW) ? REFLOW_MAX_TEMP : FILAMENT_MAX_TEMP;

  if (currentTemp > maxTemp) {
    emergencyShutdown(ERROR_OVER_TEMP);
    return true;
  }
  return false;
}

bool checkThermalRunaway() {
  if (!heaterWasOn || ssrState) {
    return false;
  }

  unsigned long timeSinceOff = millis() - heaterOffTime;

  if (timeSinceOff > THERMAL_RUNAWAY_TIME) {
    float tempRise = currentTemp - heaterOffTemp;
    if (tempRise > THERMAL_RUNAWAY_TEMP_RISE) {
      emergencyShutdown(ERROR_THERMAL_RUNAWAY);
      return true;
    }
  }
  return false;
}

bool checkTimeout() {
  unsigned long maxDuration = (operatingMode == MODE_REFLOW) ?
                              REFLOW_TIMEOUT : (filamentDuration * 60UL);
  unsigned long elapsed = (millis() - operationStartTime) / 1000;

  if (elapsed > maxDuration) {
    emergencyShutdown(ERROR_TIMEOUT);
    return true;
  }
  return false;
}

void emergencyShutdown(ErrorCode error) {
  // Immediately turn off heater
  digitalWrite(PIN_SSR, LOW);
  ssrState = false;
  ssrDutyCycle = 0;

  // Set error state
  lastError = error;
  systemState = STATE_ERROR;

  // Log error
  if (SERIAL_ENABLE_DEBUG) {
    Serial.print(F("EMERGENCY SHUTDOWN: Error "));
    Serial.println(error);
  }

  // Publish error to MQTT
  if (mqttConnected) {
    String errorTopic = String(MQTT_BASE_TOPIC) + "/" + deviceId + "/error";
    String errorMsg = "E0" + String(error);
    mqttClient.publish(errorTopic.c_str(), errorMsg.c_str(), true);
  }
}

// ===========================================
// State Machine Functions
// ===========================================

void updateStateMachine() {
  unsigned long phaseElapsed = (millis() - phaseStartTime) / 1000;

  switch (systemState) {
    case STATE_IDLE:
    case STATE_ERROR:
    case STATE_COMPLETE:
      // No automatic transitions
      break;

    case STATE_PREHEATING:
      if (currentTemp >= reflowSoakTemp) {
        transitionToState(STATE_SOAKING);
      }
      break;

    case STATE_SOAKING:
      if (phaseElapsed >= reflowSoakDuration) {
        transitionToState(STATE_REFLOW);
      }
      break;

    case STATE_REFLOW:
      if (currentTemp >= reflowPeakTemp) {
        // Start cooling after reaching peak
        transitionToState(STATE_COOLING);
      }
      break;

    case STATE_COOLING:
      if (currentTemp <= REFLOW_COOLING_TEMP) {
        transitionToState(STATE_COMPLETE);
      }
      break;

    case STATE_HEATING:
      if (operatingMode == MODE_FILAMENT) {
        if (currentTemp >= filamentTemp - 2) {
          transitionToState(STATE_MAINTAINING);
        }
      }
      break;

    case STATE_MAINTAINING:
      if (operatingMode == MODE_FILAMENT) {
        unsigned long totalElapsed = (millis() - operationStartTime) / 1000;
        if (totalElapsed >= (filamentDuration * 60UL)) {
          transitionToState(STATE_COMPLETE);
        }
      }
      break;
  }
}

void startOperation() {
  if (systemState != STATE_IDLE) {
    return;
  }

  operationStartTime = millis();
  phaseStartTime = millis();
  heaterWasOn = false;

  setPIDParams(operatingMode);

  if (operatingMode == MODE_REFLOW) {
    targetTemp = reflowSoakTemp;
    transitionToState(STATE_PREHEATING);
    operationDuration = REFLOW_TIMEOUT;
  } else if (operatingMode == MODE_FILAMENT) {
    targetTemp = filamentTemp;
    transitionToState(STATE_HEATING);
    operationDuration = filamentDuration * 60UL;
  }

  if (SERIAL_ENABLE_DEBUG) {
    Serial.print(F("Starting operation: "));
    Serial.println(getModeString(operatingMode));
  }
}

void stopOperation() {
  // Turn off heater
  digitalWrite(PIN_SSR, LOW);
  ssrState = false;
  ssrDutyCycle = 0;
  targetTemp = 0;

  // Reset state
  transitionToState(STATE_IDLE);
  operatingMode = MODE_IDLE;

  if (SERIAL_ENABLE_DEBUG) {
    Serial.println(F("Operation stopped"));
  }
}

void transitionToState(SystemState newState) {
  systemState = newState;
  phaseStartTime = millis();

  // Update target temperature for reflow profile
  if (operatingMode == MODE_REFLOW) {
    switch (newState) {
      case STATE_PREHEATING:
        targetTemp = reflowSoakTemp;
        break;
      case STATE_SOAKING:
        targetTemp = (REFLOW_SOAK_TEMP_MIN + REFLOW_SOAK_TEMP_MAX) / 2;
        break;
      case STATE_REFLOW:
        targetTemp = reflowPeakTemp;
        break;
      case STATE_COOLING:
        targetTemp = 0;
        ssrDutyCycle = 0;
        break;
      case STATE_COMPLETE:
        targetTemp = 0;
        ssrDutyCycle = 0;
        break;
    }
  }

  if (SERIAL_ENABLE_DEBUG) {
    Serial.print(F("State transition: "));
    Serial.println(getStateString(newState));
  }

  // Publish state change to MQTT
  if (mqttConnected) {
    String stateTopic = String(MQTT_BASE_TOPIC) + "/" + deviceId + "/status";
    mqttClient.publish(stateTopic.c_str(), getStateString(newState).c_str(), true);
  }
}

// ===========================================
// Display Functions
// ===========================================

void updateDisplay() {
  display.clearDisplay();

  switch (currentScreen) {
    case SCREEN_MAIN:
      drawMainScreen();
      break;
    case SCREEN_MENU:
    case SCREEN_MODE_SELECT:
    case SCREEN_REFLOW_SETTINGS:
    case SCREEN_FILAMENT_SETTINGS:
    case SCREEN_NETWORK_SETTINGS:
    case SCREEN_SYSTEM_INFO:
    case SCREEN_FILAMENT_TYPE:
      drawMenuScreen();
      break;
    case SCREEN_EDIT_VALUE:
      drawEditScreen();
      break;
    case SCREEN_CONFIRM_START:
      drawConfirmScreen();
      break;
  }

  if (systemState == STATE_ERROR) {
    drawErrorScreen();
  }

  display.display();
}

void drawMainScreen() {
  // Mode
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Mode: "));
  display.println(getModeString(operatingMode));

  // Current temperature
  display.setCursor(0, 12);
  display.print(F("Current: "));
  display.print(currentTemp, 1);
  display.println(F("C"));

  // Target temperature
  display.setCursor(0, 22);
  display.print(F("Target:  "));
  display.print(targetTemp, 1);
  display.println(F("C"));

  // Status
  display.setCursor(0, 32);
  display.print(F("Status: "));
  display.println(getStateString(systemState));

  // Time
  if (systemState != STATE_IDLE && systemState != STATE_ERROR) {
    display.setCursor(0, 42);
    display.print(F("Time: "));
    unsigned long elapsed = (millis() - operationStartTime) / 1000;
    display.print(formatTime(elapsed));
    display.print(F(" / "));
    display.println(formatTime(operationDuration));
  }

  // Status indicators
  display.setCursor(0, 54);
  display.print(F("[MENU]"));

  display.setCursor(80, 54);
  display.print(F("WiFi:"));
  display.print(wifiConnected ? "+" : "-");

  display.setCursor(110, 54);
  display.print(F("MQ:"));
  display.print(mqttConnected ? "+" : "-");
}

void drawMenuScreen() {
  const char** items;
  int itemCount;
  String title;

  switch (currentScreen) {
    case SCREEN_MENU:
      items = mainMenuItems;
      itemCount = mainMenuCount;
      title = "Main Menu";
      break;
    case SCREEN_MODE_SELECT:
      items = modeMenuItems;
      itemCount = modeMenuCount;
      title = "Select Mode";
      break;
    case SCREEN_REFLOW_SETTINGS:
      items = reflowMenuItems;
      itemCount = reflowMenuCount;
      title = "Reflow Settings";
      break;
    case SCREEN_FILAMENT_SETTINGS:
      items = filamentMenuItems;
      itemCount = filamentMenuCount;
      title = "Filament Settings";
      break;
    case SCREEN_FILAMENT_TYPE:
      items = filamentTypeItems;
      itemCount = filamentTypeCount;
      title = "Filament Type";
      break;
    case SCREEN_NETWORK_SETTINGS:
      items = networkMenuItems;
      itemCount = networkMenuCount;
      title = "Network";
      break;
    case SCREEN_SYSTEM_INFO:
      // Special case - show system info
      display.setCursor(0, 0);
      display.println(F("System Info"));
      display.drawLine(0, 9, 128, 9, SSD1306_WHITE);
      display.setCursor(0, 12);
      display.print(F("FW: "));
      display.println(FIRMWARE_VERSION);
      display.print(F("WiFi: "));
      display.println(wifiConnected ? "Connected" : "Disconnected");
      display.print(F("MQTT: "));
      display.println(mqttConnected ? "Connected" : "Disconnected");
      display.print(F("IP: "));
      display.println(wifiConnected ? WiFi.localIP().toString() : "N/A");
      display.setCursor(0, 54);
      display.print(F("< Back"));
      return;
    default:
      return;
  }

  // Draw title
  display.setCursor(0, 0);
  display.println(title);
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  // Draw menu items (show up to 4 items)
  int startIndex = max(0, menuIndex - 2);
  int endIndex = min(itemCount, startIndex + 4);

  for (int i = startIndex; i < endIndex; i++) {
    int y = 12 + (i - startIndex) * 12;
    display.setCursor(0, y);

    if (i == menuIndex) {
      display.print(F("> "));
    } else {
      display.print(F("  "));
    }
    display.println(items[i]);
  }

  // Show current value for settings
  if (currentScreen == SCREEN_REFLOW_SETTINGS && menuIndex < 3) {
    display.setCursor(80, 12 + (menuIndex - startIndex) * 12);
    switch (menuIndex) {
      case 0: display.print(reflowPeakTemp); display.print(F("C")); break;
      case 1: display.print(reflowSoakTemp); display.print(F("C")); break;
      case 2: display.print(reflowSoakDuration); display.print(F("s")); break;
    }
  } else if (currentScreen == SCREEN_FILAMENT_SETTINGS && menuIndex < 3) {
    display.setCursor(80, 12 + (menuIndex - startIndex) * 12);
    switch (menuIndex) {
      case 0: /* Type shown in menu */ break;
      case 1: display.print(filamentTemp); display.print(F("C")); break;
      case 2: display.print(filamentDuration); display.print(F("m")); break;
    }
  }
}

void drawEditScreen() {
  display.setCursor(0, 0);
  display.print(F("Edit: "));
  display.println(editLabel);
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(30, 25);
  display.print(editValue);

  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(F("Min: "));
  display.print(editMin);
  display.setCursor(70, 54);
  display.print(F("Max: "));
  display.print(editMax);

  // Draw a simple bar
  int percent = map(editValue, editMin, editMax, 0, 100);
  drawProgressBar(10, 45, 108, 6, percent);
}

void drawConfirmScreen() {
  display.setCursor(20, 10);
  display.println(F("Start Operation?"));

  display.setCursor(20, 25);
  display.print(F("Mode: "));
  display.println(getModeString(operatingMode));

  display.setCursor(20, 40);
  display.println(F("Press to confirm"));

  display.setCursor(10, 54);
  display.print(F("[OK]      [Hold=Cancel]"));
}

void drawErrorScreen() {
  display.fillRect(10, 15, 108, 35, SSD1306_BLACK);
  display.drawRect(10, 15, 108, 35, SSD1306_WHITE);

  display.setCursor(35, 20);
  display.print(F("ERROR E0"));
  display.println(lastError);

  display.setCursor(20, 35);
  switch (lastError) {
    case ERROR_OVER_TEMP:
      display.println(F("Over temperature!"));
      break;
    case ERROR_SENSOR_FAIL:
      display.println(F("Sensor failure!"));
      break;
    case ERROR_THERMAL_RUNAWAY:
      display.println(F("Thermal runaway!"));
      break;
    case ERROR_TIMEOUT:
      display.println(F("Timeout exceeded"));
      break;
    default:
      display.println(F("Unknown error"));
      break;
  }
}

void drawProgressBar(int x, int y, int width, int height, int percent) {
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  int fillWidth = (width - 2) * percent / 100;
  display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
}

// ===========================================
// Input Functions
// ===========================================

void IRAM_ATTR encoderISR() {
  static unsigned long lastInterruptTime = 0;
  unsigned long now = millis();

  // Debounce: ignore interrupts within 5ms of each other
  if (now - lastInterruptTime < 5) {
    return;
  }
  lastInterruptTime = now;

  int clkState = digitalRead(PIN_ENCODER_CLK);
  int dtState = digitalRead(PIN_ENCODER_DT);

  if (clkState != lastEncoderCLK && clkState == LOW) {
    if (dtState != clkState) {
      encoderPosition++;
    } else {
      encoderPosition--;
    }
  }
  lastEncoderCLK = clkState;
}

void handleEncoder() {
  static int lastPosition = 0;

  if (encoderPosition != lastPosition) {
    int direction = (encoderPosition > lastPosition) ? 1 : -1;

    // Acceleration
    unsigned long now = millis();
    int step = 1;
    if (now - lastEncoderTime < ENCODER_ACCEL_THRESHOLD) {
      step = 5;
    }
    lastEncoderTime = now;

    if (currentScreen == SCREEN_EDIT_VALUE) {
      editValue += direction * step;
      editValue = constrain(editValue, editMin, editMax);
    } else {
      handleMenuNavigation(direction);
    }

    lastPosition = encoderPosition;
  }
}

void handleButton() {
  bool buttonState = digitalRead(PIN_BUTTON) == LOW;
  unsigned long currentMillis = millis();

  if (buttonState && !buttonPressed) {
    // Button just pressed
    buttonPressed = true;
    buttonPressStart = currentMillis;
    buttonLongPressHandled = false;
  } else if (buttonState && buttonPressed) {
    // Button held
    unsigned long holdTime = currentMillis - buttonPressStart;

    // Long press for start/stop (5 seconds) - only on main screen
    if (holdTime >= BUTTON_START_PRESS_TIME && !buttonLongPressHandled) {
      buttonLongPressHandled = true;

      if (currentScreen == SCREEN_MAIN) {
        if (systemState == STATE_IDLE && operatingMode != MODE_IDLE) {
          // Show confirmation screen
          currentScreen = SCREEN_CONFIRM_START;
        } else if (systemState != STATE_IDLE && systemState != STATE_ERROR) {
          // Stop operation
          stopOperation();
        }
      }
    }
    // Long press for back (2 seconds)
    else if (holdTime >= BUTTON_LONG_PRESS_TIME && !buttonLongPressHandled) {
      if (currentScreen != SCREEN_MAIN && currentScreen != SCREEN_CONFIRM_START) {
        buttonLongPressHandled = true;
        handleMenuBack();
      }
    }
  } else if (!buttonState && buttonPressed) {
    // Button released
    unsigned long holdTime = currentMillis - buttonPressStart;

    // Short press (only if long press wasn't handled)
    if (holdTime < BUTTON_LONG_PRESS_TIME && !buttonLongPressHandled) {
      if (currentScreen == SCREEN_MAIN) {
        // Open menu
        currentScreen = SCREEN_MENU;
        menuIndex = 0;
      } else if (currentScreen == SCREEN_CONFIRM_START) {
        // Confirm start
        startOperation();
        currentScreen = SCREEN_MAIN;
      } else if (currentScreen == SCREEN_EDIT_VALUE) {
        // Save value and return
        if (editTarget != nullptr) {
          *editTarget = editValue;
          saveSettings();
        }
        handleMenuBack();
      } else if (systemState == STATE_ERROR) {
        // Clear error and return to idle
        lastError = ERROR_NONE;
        systemState = STATE_IDLE;
        currentScreen = SCREEN_MAIN;
      } else {
        handleMenuSelect();
      }
    }

    buttonPressed = false;
    buttonLongPressHandled = false;
  }
}

void handleMenuNavigation(int direction) {
  int maxIndex = 0;

  switch (currentScreen) {
    case SCREEN_MENU: maxIndex = mainMenuCount - 1; break;
    case SCREEN_MODE_SELECT: maxIndex = modeMenuCount - 1; break;
    case SCREEN_REFLOW_SETTINGS: maxIndex = reflowMenuCount - 1; break;
    case SCREEN_FILAMENT_SETTINGS: maxIndex = filamentMenuCount - 1; break;
    case SCREEN_FILAMENT_TYPE: maxIndex = filamentTypeCount - 1; break;
    case SCREEN_NETWORK_SETTINGS: maxIndex = networkMenuCount - 1; break;
    default: return;
  }

  menuIndex += direction;
  menuIndex = constrain(menuIndex, 0, maxIndex);
}

void handleMenuSelect() {
  switch (currentScreen) {
    case SCREEN_MENU:
      switch (menuIndex) {
        case 0: currentScreen = SCREEN_MODE_SELECT; menuIndex = 0; break;
        case 1: currentScreen = SCREEN_REFLOW_SETTINGS; menuIndex = 0; break;
        case 2: currentScreen = SCREEN_FILAMENT_SETTINGS; menuIndex = 0; break;
        case 3: currentScreen = SCREEN_NETWORK_SETTINGS; menuIndex = 0; break;
        case 4: currentScreen = SCREEN_SYSTEM_INFO; break;
        case 5: currentScreen = SCREEN_MAIN; break;
      }
      break;

    case SCREEN_MODE_SELECT:
      switch (menuIndex) {
        case 0:
          operatingMode = MODE_REFLOW;
          currentScreen = SCREEN_MAIN;
          break;
        case 1:
          operatingMode = MODE_FILAMENT;
          currentScreen = SCREEN_MAIN;
          break;
        case 2:
          currentScreen = SCREEN_MENU;
          menuIndex = 0;
          break;
      }
      break;

    case SCREEN_REFLOW_SETTINGS:
      switch (menuIndex) {
        case 0:
          editLabel = "Peak Temp (C)";
          editValue = reflowPeakTemp;
          editMin = REFLOW_PEAK_TEMP_MIN;
          editMax = REFLOW_PEAK_TEMP_MAX;
          editTarget = &reflowPeakTemp;
          currentScreen = SCREEN_EDIT_VALUE;
          break;
        case 1:
          editLabel = "Soak Temp (C)";
          editValue = reflowSoakTemp;
          editMin = REFLOW_SOAK_TEMP_CONFIG_MIN;
          editMax = REFLOW_SOAK_TEMP_CONFIG_MAX;
          editTarget = &reflowSoakTemp;
          currentScreen = SCREEN_EDIT_VALUE;
          break;
        case 2:
          editLabel = "Soak Time (s)";
          editValue = reflowSoakDuration;
          editMin = REFLOW_SOAK_DURATION_MIN;
          editMax = REFLOW_SOAK_DURATION_MAX;
          editTarget = &reflowSoakDuration;
          currentScreen = SCREEN_EDIT_VALUE;
          break;
        case 3:
          currentScreen = SCREEN_MENU;
          menuIndex = 1;
          break;
      }
      break;

    case SCREEN_FILAMENT_SETTINGS:
      switch (menuIndex) {
        case 0:
          currentScreen = SCREEN_FILAMENT_TYPE;
          menuIndex = (int)selectedFilament;
          break;
        case 1:
          editLabel = "Temp (C)";
          editValue = filamentTemp;
          editMin = FILAMENT_TEMP_MIN;
          editMax = FILAMENT_TEMP_MAX;
          editTarget = &filamentTemp;
          currentScreen = SCREEN_EDIT_VALUE;
          break;
        case 2:
          editLabel = "Duration (min)";
          editValue = filamentDuration;
          editMin = FILAMENT_TIME_MIN;
          editMax = FILAMENT_TIME_MAX;
          editTarget = &filamentDuration;
          currentScreen = SCREEN_EDIT_VALUE;
          break;
        case 3:
          currentScreen = SCREEN_MENU;
          menuIndex = 2;
          break;
      }
      break;

    case SCREEN_FILAMENT_TYPE:
      if (menuIndex == filamentTypeCount - 1) {
        // Back
        currentScreen = SCREEN_FILAMENT_SETTINGS;
        menuIndex = 0;
      } else {
        selectedFilament = (FilamentType)menuIndex;
        // Set preset values
        switch (selectedFilament) {
          case FILAMENT_PLA:
            filamentTemp = FILAMENT_PLA_TEMP;
            filamentDuration = FILAMENT_PLA_TIME;
            break;
          case FILAMENT_PETG:
            filamentTemp = FILAMENT_PETG_TEMP;
            filamentDuration = FILAMENT_PETG_TIME;
            break;
          case FILAMENT_ABS:
            filamentTemp = FILAMENT_ABS_TEMP;
            filamentDuration = FILAMENT_ABS_TIME;
            break;
          case FILAMENT_NYLON:
            filamentTemp = FILAMENT_NYLON_TEMP;
            filamentDuration = FILAMENT_NYLON_TIME;
            break;
          case FILAMENT_TPU:
            filamentTemp = FILAMENT_TPU_TEMP;
            filamentDuration = FILAMENT_TPU_TIME;
            break;
          case FILAMENT_CUSTOM:
            // Keep current values
            break;
        }
        saveSettings();
        currentScreen = SCREEN_FILAMENT_SETTINGS;
        menuIndex = 0;
      }
      break;

    case SCREEN_NETWORK_SETTINGS:
      if (menuIndex == networkMenuCount - 1) {
        currentScreen = SCREEN_MENU;
        menuIndex = 3;
      }
      break;

    case SCREEN_SYSTEM_INFO:
      currentScreen = SCREEN_MENU;
      menuIndex = 4;
      break;
  }
}

void handleMenuBack() {
  switch (currentScreen) {
    case SCREEN_MENU:
      currentScreen = SCREEN_MAIN;
      break;
    case SCREEN_MODE_SELECT:
    case SCREEN_REFLOW_SETTINGS:
    case SCREEN_FILAMENT_SETTINGS:
    case SCREEN_NETWORK_SETTINGS:
    case SCREEN_SYSTEM_INFO:
      currentScreen = SCREEN_MENU;
      menuIndex = 0;
      break;
    case SCREEN_FILAMENT_TYPE:
      currentScreen = SCREEN_FILAMENT_SETTINGS;
      menuIndex = 0;
      break;
    case SCREEN_EDIT_VALUE:
      // Cancel edit, restore previous value
      if (currentScreen == SCREEN_REFLOW_SETTINGS) {
        currentScreen = SCREEN_REFLOW_SETTINGS;
      } else {
        currentScreen = SCREEN_FILAMENT_SETTINGS;
      }
      break;
    case SCREEN_CONFIRM_START:
      currentScreen = SCREEN_MAIN;
      break;
  }
}

// ===========================================
// WiFi Functions
// ===========================================

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  if (SERIAL_ENABLE_DEBUG) {
    Serial.print(F("Connecting to WiFi: "));
    Serial.println(wifiSSID);
  }

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    if (SERIAL_ENABLE_DEBUG) {
      Serial.print(F("."));
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    if (SERIAL_ENABLE_DEBUG) {
      Serial.println();
      Serial.print(F("WiFi connected. IP: "));
      Serial.println(WiFi.localIP());
    }
  } else {
    if (SERIAL_ENABLE_DEBUG) {
      Serial.println();
      Serial.println(F("WiFi connection failed, starting AP mode"));
    }
    startAPMode();
  }
}

void handleWiFiConnection() {
  if (!apMode) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
    } else {
      if (wifiConnected) {
        wifiConnected = false;
        if (SERIAL_ENABLE_DEBUG) {
          Serial.println(F("WiFi disconnected, attempting reconnect..."));
        }
      }
      // WiFi auto-reconnect is handled by the ESP8266
    }
  }
}

void startAPMode() {
  apMode = true;
  WiFi.mode(WIFI_AP);

  String apSSID = AP_SSID_PREFIX + deviceId.substring(0, 6);
  WiFi.softAP(apSSID.c_str(), AP_PASSWORD);

  if (SERIAL_ENABLE_DEBUG) {
    Serial.print(F("AP Mode started. SSID: "));
    Serial.println(apSSID);
    Serial.print(F("AP IP: "));
    Serial.println(WiFi.softAPIP());
  }
}

// ===========================================
// MQTT Functions
// ===========================================

void setupMQTT() {
  mqttClient.setServer(mqttBroker.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);
}

void handleMQTT() {
  if (!wifiConnected || apMode) {
    mqttConnected = false;
    return;
  }

  if (!mqttClient.connected()) {
    mqttConnected = false;

    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > mqttReconnectDelay) {
      lastMqttReconnectAttempt = now;

      String clientId = "toaster-oven-" + deviceId;

      if (SERIAL_ENABLE_DEBUG) {
        Serial.print(F("Attempting MQTT connection to "));
        Serial.print(mqttBroker);
        Serial.print(F(":"));
        Serial.println(mqttPort);
      }

      bool connected = false;
      String willTopic = String(MQTT_BASE_TOPIC) + "/" + deviceId + "/availability";

      if (mqttUser.length() > 0) {
        connected = mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPassword.c_str(),
                                        willTopic.c_str(), MQTT_QOS, true, "offline");
      } else {
        connected = mqttClient.connect(clientId.c_str(), willTopic.c_str(), MQTT_QOS, true, "offline");
      }

      if (connected) {
        mqttConnected = true;
        mqttReconnectDelay = MQTT_RECONNECT_DELAY_BASE;

        if (SERIAL_ENABLE_DEBUG) {
          Serial.println(F("MQTT connected"));
        }

        // Subscribe to command topics
        String baseTopic = String(MQTT_BASE_TOPIC) + "/" + deviceId;
        mqttClient.subscribe((baseTopic + "/command/#").c_str());
        mqttClient.subscribe((baseTopic + "/config/update").c_str());

        // Publish availability
        mqttClient.publish(willTopic.c_str(), "online", true);

        // Publish Home Assistant discovery
        publishDiscovery();
      } else {
        // Exponential backoff
        mqttReconnectDelay = min(mqttReconnectDelay * 2, (unsigned long)MQTT_RECONNECT_DELAY_MAX);

        if (SERIAL_ENABLE_DEBUG) {
          Serial.print(F("MQTT connection failed, rc="));
          Serial.println(mqttClient.state());
        }
      }
    }
  } else {
    mqttClient.loop();
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String payloadStr = "";
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }

  if (SERIAL_ENABLE_DEBUG) {
    Serial.print(F("MQTT received: "));
    Serial.print(topicStr);
    Serial.print(F(" -> "));
    Serial.println(payloadStr);
  }

  // Parse topic
  String baseTopic = String(MQTT_BASE_TOPIC) + "/" + deviceId;

  if (topicStr == baseTopic + "/command/stop") {
    // Stop operation (allowed remotely)
    if (systemState != STATE_IDLE && systemState != STATE_ERROR) {
      stopOperation();
    }
  } else if (topicStr == baseTopic + "/command/set_temp") {
    // Set target temperature (only within limits)
    float newTemp = payloadStr.toFloat();
    float maxTemp = (operatingMode == MODE_REFLOW) ? REFLOW_MAX_TEMP : FILAMENT_MAX_TEMP;
    float minTemp = (operatingMode == MODE_FILAMENT) ? FILAMENT_TEMP_MIN : 0;

    if (newTemp >= minTemp && newTemp <= maxTemp) {
      targetTemp = newTemp;
      if (operatingMode == MODE_FILAMENT) {
        filamentTemp = (int)newTemp;
      }
    }
  } else if (topicStr == baseTopic + "/command/set_mode") {
    // Set mode (only when idle)
    if (systemState == STATE_IDLE) {
      if (payloadStr == "reflow") {
        operatingMode = MODE_REFLOW;
      } else if (payloadStr == "filament") {
        operatingMode = MODE_FILAMENT;
      }
    }
  }
  // Note: Start command is NOT implemented (per FSD security requirement)
}

void publishStatus() {
  String baseTopic = String(MQTT_BASE_TOPIC) + "/" + deviceId;

  // State
  String state = (systemState == STATE_IDLE) ? "OFF" : "ON";
  mqttClient.publish((baseTopic + "/state").c_str(), state.c_str(), true);

  // Current temperature
  mqttClient.publish((baseTopic + "/temperature/current").c_str(),
                     String(currentTemp, 1).c_str(), true);

  // Target temperature
  mqttClient.publish((baseTopic + "/temperature/target").c_str(),
                     String(targetTemp, 1).c_str(), true);

  // Mode
  String mode = getModeString(operatingMode);
  mode.toLowerCase();
  mqttClient.publish((baseTopic + "/mode").c_str(), mode.c_str(), true);

  // Status
  String status = getStateString(systemState);
  status.toLowerCase();
  mqttClient.publish((baseTopic + "/status").c_str(), status.c_str(), true);

  // Remaining time
  if (systemState != STATE_IDLE && systemState != STATE_ERROR) {
    unsigned long elapsed = (millis() - operationStartTime) / 1000;
    unsigned long remaining = (operationDuration > elapsed) ? (operationDuration - elapsed) : 0;
    mqttClient.publish((baseTopic + "/time/remaining").c_str(),
                       String(remaining).c_str(), false);
  }

  // WiFi RSSI
  mqttClient.publish((baseTopic + "/wifi/rssi").c_str(),
                     String(WiFi.RSSI()).c_str(), false);
}

void publishDiscovery() {
  String baseTopic = String(MQTT_BASE_TOPIC) + "/" + deviceId;

  // Create discovery payload
  StaticJsonDocument<1024> doc;

  doc["name"] = "Toaster Oven Controller";
  doc["unique_id"] = "toaster_oven_" + deviceId;

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"][0] = "toaster_oven_" + deviceId;
  device["name"] = "Reflow Oven";
  device["model"] = DEVICE_MODEL;
  device["manufacturer"] = DEVICE_MANUFACTURER;
  device["sw_version"] = FIRMWARE_VERSION;

  doc["mode_command_topic"] = baseTopic + "/command/set_mode";
  doc["mode_state_topic"] = baseTopic + "/mode";
  doc["temperature_command_topic"] = baseTopic + "/command/set_temp";
  doc["temperature_state_topic"] = baseTopic + "/temperature/target";
  doc["current_temperature_topic"] = baseTopic + "/temperature/current";

  JsonArray modes = doc.createNestedArray("modes");
  modes.add("reflow");
  modes.add("filament");
  modes.add("off");

  doc["min_temp"] = 40;
  doc["max_temp"] = 260;
  doc["temp_step"] = 5;
  doc["availability_topic"] = baseTopic + "/availability";

  String payload;
  serializeJson(doc, payload);

  String discoveryTopic = "homeassistant/climate/toaster_oven_" + deviceId + "/config";
  mqttClient.publish(discoveryTopic.c_str(), payload.c_str(), true);

  if (SERIAL_ENABLE_DEBUG) {
    Serial.println(F("Published HA discovery"));
  }
}

// ===========================================
// Web Server Functions
// ===========================================

void handleWebSetup();

void setupWebServer() {
  webServer.on("/", HTTP_GET, handleWebRoot);
  webServer.on("/setup", HTTP_GET, handleWebSetup);
  webServer.on("/api/status", HTTP_GET, handleWebAPI);
  webServer.on("/api/stop", HTTP_POST, []() {
    if (systemState != STATE_IDLE && systemState != STATE_ERROR) {
      stopOperation();
    }
    webServer.send(200, "application/json", "{\"success\":true}");
  });
  webServer.on("/api/config", HTTP_GET, handleWebConfig);
  webServer.on("/api/config", HTTP_POST, []() {
    // Handle configuration updates
    if (webServer.hasArg("plain")) {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, webServer.arg("plain"));

      if (!error) {
        if (doc.containsKey("temp_offset")) {
          tempCalibOffset = doc["temp_offset"].as<float>();
        }
        if (doc.containsKey("wifi_ssid")) {
          wifiSSID = doc["wifi_ssid"].as<String>();
        }
        if (doc.containsKey("wifi_password")) {
          wifiPassword = doc["wifi_password"].as<String>();
        }
        if (doc.containsKey("mqtt_broker")) {
          mqttBroker = doc["mqtt_broker"].as<String>();
        }
        if (doc.containsKey("mqtt_port")) {
          mqttPort = doc["mqtt_port"].as<int>();
        }
        if (doc.containsKey("mqtt_user")) {
          mqttUser = doc["mqtt_user"].as<String>();
        }
        if (doc.containsKey("mqtt_password")) {
          mqttPassword = doc["mqtt_password"].as<String>();
        }
        saveSettings();
        webServer.send(200, "application/json", "{\"success\":true,\"restart\":true}");
        delay(1000);
        ESP.restart();
      } else {
        webServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      }
    } else {
      webServer.send(400, "application/json", "{\"error\":\"No data\"}");
    }
  });
  webServer.onNotFound(handleWebNotFound);

  webServer.begin();

  if (SERIAL_ENABLE_DEBUG) {
    Serial.println(F("Web server started"));
  }
}

void handleWebRoot() {
  String html = F(R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Reflow Oven Controller</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
           background: #1a1a2e; color: #eee; padding: 20px; }
    .container { max-width: 600px; margin: 0 auto; }
    h1 { text-align: center; margin-bottom: 20px; color: #00d9ff; }
    .card { background: #16213e; border-radius: 10px; padding: 20px; margin-bottom: 20px; }
    .card h2 { font-size: 1.2em; margin-bottom: 15px; color: #00d9ff; }
    .stat { display: flex; justify-content: space-between; padding: 10px 0;
            border-bottom: 1px solid #0f3460; }
    .stat:last-child { border-bottom: none; }
    .stat-label { color: #888; }
    .stat-value { font-weight: bold; font-size: 1.1em; }
    .temp-large { font-size: 3em; text-align: center; color: #ff6b6b; }
    .status { display: inline-block; padding: 5px 15px; border-radius: 20px;
              font-size: 0.9em; font-weight: bold; }
    .status-idle { background: #3d5a80; }
    .status-heating { background: #ff6b6b; }
    .status-cooling { background: #4ecdc4; }
    .status-error { background: #c1121f; }
    .status-complete { background: #2ec4b6; }
    .btn { display: block; width: 100%; padding: 15px; border: none;
           border-radius: 8px; font-size: 1em; cursor: pointer;
           transition: background 0.3s; }
    .btn-stop { background: #c1121f; color: white; }
    .btn-stop:hover { background: #a00; }
    .btn-stop:disabled { background: #555; cursor: not-allowed; }
    .indicators { display: flex; gap: 15px; justify-content: center; margin-top: 10px; }
    .indicator { display: flex; align-items: center; gap: 5px; }
    .dot { width: 10px; height: 10px; border-radius: 50%; }
    .dot-on { background: #2ec4b6; }
    .dot-off { background: #c1121f; }
    .warning { background: #ff9f1c; color: #000; padding: 15px; border-radius: 8px;
               margin-bottom: 20px; text-align: center; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Reflow Oven Controller</h1>

    <div class="warning">
      ⚠️ Start operation can only be done via physical controls
    </div>

    <div class="card">
      <div class="temp-large" id="currentTemp">--°C</div>
      <div class="stat">
        <span class="stat-label">Target</span>
        <span class="stat-value" id="targetTemp">--°C</span>
      </div>
    </div>

    <div class="card">
      <h2>Status</h2>
      <div class="stat">
        <span class="stat-label">Mode</span>
        <span class="stat-value" id="mode">--</span>
      </div>
      <div class="stat">
        <span class="stat-label">State</span>
        <span class="status status-idle" id="status">IDLE</span>
      </div>
      <div class="stat">
        <span class="stat-label">Time</span>
        <span class="stat-value" id="time">--:-- / --:--</span>
      </div>
      <div class="indicators">
        <div class="indicator">
          <span class="dot dot-off" id="wifiDot"></span>
          <span>WiFi</span>
        </div>
        <div class="indicator">
          <span class="dot dot-off" id="mqttDot"></span>
          <span>MQTT</span>
        </div>
        <div class="indicator">
          <span class="dot dot-off" id="heaterDot"></span>
          <span>Heater</span>
        </div>
      </div>
    </div>

    <button class="btn btn-stop" id="stopBtn" onclick="stopOven()" disabled>
      STOP OVEN
    </button>
  </div>

  <script>
    function updateStatus() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          document.getElementById('currentTemp').textContent = data.currentTemp.toFixed(1) + '°C';
          document.getElementById('targetTemp').textContent = data.targetTemp.toFixed(1) + '°C';
          document.getElementById('mode').textContent = data.mode;

          const statusEl = document.getElementById('status');
          statusEl.textContent = data.state;
          statusEl.className = 'status status-' + data.state.toLowerCase();

          document.getElementById('time').textContent = data.elapsed + ' / ' + data.duration;

          document.getElementById('wifiDot').className = 'dot ' + (data.wifi ? 'dot-on' : 'dot-off');
          document.getElementById('mqttDot').className = 'dot ' + (data.mqtt ? 'dot-on' : 'dot-off');
          document.getElementById('heaterDot').className = 'dot ' + (data.heater ? 'dot-on' : 'dot-off');

          document.getElementById('stopBtn').disabled = (data.state === 'IDLE' || data.state === 'ERROR');
        })
        .catch(err => console.error('Error:', err));
    }

    function stopOven() {
      if (confirm('Are you sure you want to stop the oven?')) {
        fetch('/api/stop', { method: 'POST' })
          .then(r => r.json())
          .then(data => {
            if (data.success) {
              alert('Oven stopped');
              updateStatus();
            }
          })
          .catch(err => alert('Error stopping oven'));
      }
    }

    updateStatus();
    setInterval(updateStatus, 2000);
  </script>
</body>
</html>
)rawliteral");

  webServer.send(200, "text/html", html);
}

void handleWebAPI() {
  StaticJsonDocument<512> doc;

  doc["currentTemp"] = currentTemp;
  doc["targetTemp"] = targetTemp;
  doc["mode"] = getModeString(operatingMode);
  doc["state"] = getStateString(systemState);

  unsigned long elapsed = 0;
  if (systemState != STATE_IDLE && systemState != STATE_ERROR) {
    elapsed = (millis() - operationStartTime) / 1000;
  }
  doc["elapsed"] = formatTime(elapsed);
  doc["duration"] = formatTime(operationDuration);

  doc["wifi"] = wifiConnected;
  doc["mqtt"] = mqttConnected;
  doc["heater"] = ssrState;
  doc["error"] = lastError;

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleWebSetup() {
  String html = F(R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Reflow Oven - Setup</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
           background: #1a1a2e; color: #eee; padding: 20px; }
    .container { max-width: 500px; margin: 0 auto; }
    h1 { text-align: center; margin-bottom: 20px; color: #00d9ff; }
    .card { background: #16213e; border-radius: 10px; padding: 20px; margin-bottom: 20px; }
    .card h2 { font-size: 1.1em; margin-bottom: 15px; color: #00d9ff; border-bottom: 1px solid #0f3460; padding-bottom: 10px; }
    label { display: block; margin-bottom: 5px; color: #888; font-size: 0.9em; }
    input[type="text"], input[type="password"], input[type="number"] {
      width: 100%; padding: 12px; border: 1px solid #0f3460; border-radius: 6px;
      background: #0f3460; color: #fff; margin-bottom: 15px; font-size: 1em;
    }
    input:focus { outline: none; border-color: #00d9ff; }
    .btn { display: block; width: 100%; padding: 15px; border: none;
           border-radius: 8px; font-size: 1em; cursor: pointer; margin-top: 10px; }
    .btn-save { background: #2ec4b6; color: white; }
    .btn-save:hover { background: #25a99d; }
    .btn-back { background: #3d5a80; color: white; text-align: center; text-decoration: none; display: block; }
    .btn-back:hover { background: #4a6d94; }
    .msg { padding: 15px; border-radius: 8px; margin-bottom: 15px; display: none; }
    .msg-success { background: #2ec4b6; }
    .msg-error { background: #c1121f; }
    .note { font-size: 0.85em; color: #888; margin-top: -10px; margin-bottom: 15px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Setup</h1>
    <div class="msg" id="msg"></div>

    <div class="card">
      <h2>WiFi Settings</h2>
      <label>Network Name (SSID)</label>
      <input type="text" id="wifi_ssid" maxlength="32">
      <label>Password</label>
      <input type="password" id="wifi_password" maxlength="64">
      <p class="note">Leave password blank to keep existing</p>
    </div>

    <div class="card">
      <h2>MQTT Settings</h2>
      <label>Broker Address</label>
      <input type="text" id="mqtt_broker" maxlength="64" placeholder="192.168.1.100">
      <label>Port</label>
      <input type="number" id="mqtt_port" value="1883" min="1" max="65535">
      <label>Username (optional)</label>
      <input type="text" id="mqtt_user" maxlength="32">
      <label>Password (optional)</label>
      <input type="password" id="mqtt_password" maxlength="32">
      <p class="note">Leave blank if no authentication required</p>
    </div>

    <button class="btn btn-save" onclick="saveConfig()">Save & Restart</button>
    <a href="/" class="btn btn-back">Back to Dashboard</a>
  </div>

  <script>
    fetch('/api/config')
      .then(r => r.json())
      .then(data => {
        document.getElementById('wifi_ssid').value = data.wifi_ssid || '';
        document.getElementById('mqtt_broker').value = data.mqtt_broker || '';
        document.getElementById('mqtt_port').value = data.mqtt_port || 1883;
        document.getElementById('mqtt_user').value = data.mqtt_user || '';
      });

    function saveConfig() {
      const config = {
        wifi_ssid: document.getElementById('wifi_ssid').value,
        mqtt_broker: document.getElementById('mqtt_broker').value,
        mqtt_port: parseInt(document.getElementById('mqtt_port').value) || 1883,
        mqtt_user: document.getElementById('mqtt_user').value
      };

      const wifiPass = document.getElementById('wifi_password').value;
      if (wifiPass) config.wifi_password = wifiPass;

      const mqttPass = document.getElementById('mqtt_password').value;
      if (mqttPass) config.mqtt_password = mqttPass;

      fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
      })
      .then(r => r.json())
      .then(data => {
        const msg = document.getElementById('msg');
        if (data.success) {
          msg.textContent = 'Settings saved! Restarting...';
          msg.className = 'msg msg-success';
          msg.style.display = 'block';
          setTimeout(() => { location.href = '/'; }, 3000);
        } else {
          msg.textContent = 'Error: ' + (data.error || 'Unknown error');
          msg.className = 'msg msg-error';
          msg.style.display = 'block';
        }
      })
      .catch(err => {
        const msg = document.getElementById('msg');
        msg.textContent = 'Connection error';
        msg.className = 'msg msg-error';
        msg.style.display = 'block';
      });
    }
  </script>
</body>
</html>
)rawliteral");

  webServer.send(200, "text/html", html);
}

void handleWebConfig() {
  StaticJsonDocument<512> doc;

  doc["temp_offset"] = tempCalibOffset;
  doc["wifi_ssid"] = wifiSSID;
  doc["mqtt_broker"] = mqttBroker;
  doc["mqtt_port"] = mqttPort;
  doc["mqtt_user"] = mqttUser;
  doc["firmware"] = FIRMWARE_VERSION;

  String response;
  serializeJson(doc, response);
  webServer.send(200, "application/json", response);
}

void handleWebNotFound() {
  webServer.send(404, "text/plain", "Not Found");
}

// ===========================================
// EEPROM Functions
// ===========================================

void loadSettings() {
  if (EEPROM.read(EEPROM_ADDR_SIGNATURE) != EEPROM_SIGNATURE) {
    if (SERIAL_ENABLE_DEBUG) {
      Serial.println(F("EEPROM not initialized, using defaults"));
    }
    saveSettings();  // Initialize with defaults
    return;
  }

  // Read WiFi SSID
  char ssidBuf[33];
  for (int i = 0; i < 32; i++) {
    ssidBuf[i] = EEPROM.read(EEPROM_ADDR_WIFI_SSID + i);
  }
  ssidBuf[32] = '\0';
  wifiSSID = String(ssidBuf);

  // Read WiFi Password
  char passBuf[65];
  for (int i = 0; i < 64; i++) {
    passBuf[i] = EEPROM.read(EEPROM_ADDR_WIFI_PASS + i);
  }
  passBuf[64] = '\0';
  wifiPassword = String(passBuf);

  // Read MQTT Broker
  char mqttBuf[65];
  for (int i = 0; i < 64; i++) {
    mqttBuf[i] = EEPROM.read(EEPROM_ADDR_MQTT_BROKER + i);
  }
  mqttBuf[64] = '\0';
  mqttBroker = String(mqttBuf);

  // Read MQTT Port
  mqttPort = EEPROM.read(EEPROM_ADDR_MQTT_PORT) | (EEPROM.read(EEPROM_ADDR_MQTT_PORT + 1) << 8);

  // Read MQTT User
  char mqttUserBuf[33];
  for (int i = 0; i < 32; i++) {
    mqttUserBuf[i] = EEPROM.read(EEPROM_ADDR_MQTT_USER + i);
  }
  mqttUserBuf[32] = '\0';
  mqttUser = String(mqttUserBuf);

  // Read MQTT Password
  char mqttPassBuf[33];
  for (int i = 0; i < 32; i++) {
    mqttPassBuf[i] = EEPROM.read(EEPROM_ADDR_MQTT_PASS + i);
  }
  mqttPassBuf[32] = '\0';
  mqttPassword = String(mqttPassBuf);

  // Read temperature offset
  EEPROM.get(EEPROM_ADDR_TEMP_OFFSET, tempCalibOffset);

  // Read reflow settings
  reflowPeakTemp = EEPROM.read(EEPROM_ADDR_REFLOW_PEAK) | (EEPROM.read(EEPROM_ADDR_REFLOW_PEAK + 1) << 8);
  reflowSoakTemp = EEPROM.read(EEPROM_ADDR_REFLOW_SOAK) | (EEPROM.read(EEPROM_ADDR_REFLOW_SOAK + 1) << 8);
  reflowSoakDuration = EEPROM.read(EEPROM_ADDR_REFLOW_SOAK_TIME) | (EEPROM.read(EEPROM_ADDR_REFLOW_SOAK_TIME + 1) << 8);

  // Validate ranges
  reflowPeakTemp = constrain(reflowPeakTemp, REFLOW_PEAK_TEMP_MIN, REFLOW_PEAK_TEMP_MAX);
  reflowSoakTemp = constrain(reflowSoakTemp, REFLOW_SOAK_TEMP_CONFIG_MIN, REFLOW_SOAK_TEMP_CONFIG_MAX);
  reflowSoakDuration = constrain(reflowSoakDuration, REFLOW_SOAK_DURATION_MIN, REFLOW_SOAK_DURATION_MAX);

  if (SERIAL_ENABLE_DEBUG) {
    Serial.println(F("Settings loaded from EEPROM"));
  }
}

void saveSettings() {
  // Write signature
  EEPROM.write(EEPROM_ADDR_SIGNATURE, EEPROM_SIGNATURE);

  // Write WiFi SSID
  for (int i = 0; i < 32; i++) {
    char c = (i < wifiSSID.length()) ? wifiSSID[i] : '\0';
    EEPROM.write(EEPROM_ADDR_WIFI_SSID + i, c);
  }

  // Write WiFi Password
  for (int i = 0; i < 64; i++) {
    char c = (i < wifiPassword.length()) ? wifiPassword[i] : '\0';
    EEPROM.write(EEPROM_ADDR_WIFI_PASS + i, c);
  }

  // Write MQTT Broker
  for (int i = 0; i < 64; i++) {
    char c = (i < mqttBroker.length()) ? mqttBroker[i] : '\0';
    EEPROM.write(EEPROM_ADDR_MQTT_BROKER + i, c);
  }

  // Write MQTT Port
  EEPROM.write(EEPROM_ADDR_MQTT_PORT, mqttPort & 0xFF);
  EEPROM.write(EEPROM_ADDR_MQTT_PORT + 1, (mqttPort >> 8) & 0xFF);

  // Write MQTT User
  for (int i = 0; i < 32; i++) {
    char c = (i < mqttUser.length()) ? mqttUser[i] : '\0';
    EEPROM.write(EEPROM_ADDR_MQTT_USER + i, c);
  }

  // Write MQTT Password
  for (int i = 0; i < 32; i++) {
    char c = (i < mqttPassword.length()) ? mqttPassword[i] : '\0';
    EEPROM.write(EEPROM_ADDR_MQTT_PASS + i, c);
  }

  // Write temperature offset
  EEPROM.put(EEPROM_ADDR_TEMP_OFFSET, tempCalibOffset);

  // Write reflow settings
  EEPROM.write(EEPROM_ADDR_REFLOW_PEAK, reflowPeakTemp & 0xFF);
  EEPROM.write(EEPROM_ADDR_REFLOW_PEAK + 1, (reflowPeakTemp >> 8) & 0xFF);
  EEPROM.write(EEPROM_ADDR_REFLOW_SOAK, reflowSoakTemp & 0xFF);
  EEPROM.write(EEPROM_ADDR_REFLOW_SOAK + 1, (reflowSoakTemp >> 8) & 0xFF);
  EEPROM.write(EEPROM_ADDR_REFLOW_SOAK_TIME, reflowSoakDuration & 0xFF);
  EEPROM.write(EEPROM_ADDR_REFLOW_SOAK_TIME + 1, (reflowSoakDuration >> 8) & 0xFF);

  EEPROM.commit();

  if (SERIAL_ENABLE_DEBUG) {
    Serial.println(F("Settings saved to EEPROM"));
  }
}

// ===========================================
// Utility Functions
// ===========================================

String getStateString(SystemState state) {
  switch (state) {
    case STATE_IDLE: return "IDLE";
    case STATE_PREHEATING: return "PREHEATING";
    case STATE_SOAKING: return "SOAKING";
    case STATE_REFLOW: return "REFLOW";
    case STATE_COOLING: return "COOLING";
    case STATE_HEATING: return "HEATING";
    case STATE_MAINTAINING: return "MAINTAINING";
    case STATE_COMPLETE: return "COMPLETE";
    case STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

String getModeString(OperatingMode mode) {
  switch (mode) {
    case MODE_IDLE: return "Idle";
    case MODE_REFLOW: return "Reflow";
    case MODE_FILAMENT: return "Filament";
    default: return "Unknown";
  }
}

String formatTime(unsigned long seconds) {
  unsigned long hours = seconds / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  unsigned long secs = seconds % 60;

  char buf[12];
  if (hours > 0) {
    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", hours, minutes, secs);
  } else {
    snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, secs);
  }
  return String(buf);
}

String getMacAddress() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}
