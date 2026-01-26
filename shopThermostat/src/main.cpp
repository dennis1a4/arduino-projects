/**
 * Smart Dual-Zone Shop Thermostat
 *
 * ESP8266-Based thermostat with:
 * - 5 DS18B20 temperature sensors (floor, air, outdoor, water in/out)
 * - Dual-zone control (floor pump, electric heater)
 * - 16x2 LCD display with rotary encoder
 * - WiFi with AP fallback mode
 * - Web interface for configuration
 * - MQTT integration with Home Assistant
 * - Scheduling system
 *
 * Hardware: Wemos D1 Mini
 *
 * Author: Generated from FSD
 * Version: 1.0.0
 */

#include <Arduino.h>

// Include all modules
#include "config.h"
#include "storage.h"
#include "temperature.h"
#include "control.h"
#include "display.h"
#include "encoder.h"
#include "wifi_manager.h"
#include "scheduler.h"
#include "mqtt_handler.h"
#include "webserver.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

ConfigManager config;
TemperatureManager temps;
ThermostatController controller(&config, &temps);
DisplayManager display;
EncoderHandler encoder;
WiFiConnectionManager wifi(&config);
ScheduleManager scheduler(&config, &controller);
MQTTHandler mqtt(&config, &temps, &controller);
WebServerManager webServer(&config, &temps, &controller, &scheduler, &wifi);

// ============================================================================
// TIMING VARIABLES
// ============================================================================

unsigned long lastTempRead = 0;
unsigned long lastControlUpdate = 0;
unsigned long lastScheduleCheck = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastNtpSync = 0;

unsigned long menuTimeout = 0;
bool inMenu = false;

// Status variables for display
bool wifiConnected = false;
bool mqttConnected = false;
String ipAddress = "";
unsigned long uptimeSeconds = 0;
bool scheduleActive = false;
String scheduleInfo = "";

// ============================================================================
// SYSTEM STATE
// ============================================================================

SystemState systemState = STATE_BOOT;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(F("====================================="));
    Serial.println(F("  Smart Dual-Zone Shop Thermostat"));
    Serial.println(F("  Version: " FIRMWARE_VERSION));
    Serial.println(F("====================================="));
    Serial.println();

    // Initialize LittleFS and load configuration
    Serial.println(F("Initializing storage..."));
    if (!config.begin()) {
        Serial.println(F("ERROR: Failed to initialize LittleFS!"));
    } else {
        config.load();
    }

    // Initialize temperature sensors
    Serial.println(F("Initializing temperature sensors..."));
    temps.begin();

    // Set up sensor addresses from config
    TemperatureManager::SensorAddresses addrs;
    if (strlen(config.sensors.addresses[SENSOR_FLOOR]) == 16) {
        temps.stringToAddress(config.sensors.addresses[SENSOR_FLOOR], addrs.floor);
    }
    if (strlen(config.sensors.addresses[SENSOR_AIR]) == 16) {
        temps.stringToAddress(config.sensors.addresses[SENSOR_AIR], addrs.air);
    }
    if (strlen(config.sensors.addresses[SENSOR_OUTDOOR]) == 16) {
        temps.stringToAddress(config.sensors.addresses[SENSOR_OUTDOOR], addrs.outdoor);
    }
    if (strlen(config.sensors.addresses[SENSOR_WATER_IN]) == 16) {
        temps.stringToAddress(config.sensors.addresses[SENSOR_WATER_IN], addrs.waterIn);
    }
    if (strlen(config.sensors.addresses[SENSOR_WATER_OUT]) == 16) {
        temps.stringToAddress(config.sensors.addresses[SENSOR_WATER_OUT], addrs.waterOut);
    }
    temps.setSensorAddresses(addrs);

    // Set calibration offsets
    for (int i = 0; i < SENSOR_COUNT; i++) {
        temps.setCalibration(i, config.sensors.calibration[i]);
    }

    // Run sensor discovery if no sensors configured
    if (temps.getDeviceCount() > 0) {
        Serial.println(F("Running sensor discovery..."));
        temps.discoverSensors();
    }

    // Initialize controller
    Serial.println(F("Initializing controller..."));
    controller.begin();

    // Initialize display
    Serial.println(F("Initializing display..."));
    display.begin();
    display.setReferences(&temps, &controller, &config,
                          &wifiConnected, &mqttConnected, &ipAddress,
                          &uptimeSeconds, &scheduleActive, &scheduleInfo);
    display.showMessage("Shop Thermostat", "Initializing...");

    // Initialize encoder
    Serial.println(F("Initializing encoder..."));
    encoder.begin();

    // Initialize WiFi
    Serial.println(F("Initializing WiFi..."));
    display.showMessage("Connecting", "WiFi...");
    wifi.begin();

    systemState = STATE_WIFI_CONNECT;

    // Wait a bit for WiFi connection
    unsigned long wifiStart = millis();
    while (!wifi.isConnected() && !wifi.isAPMode() && millis() - wifiStart < 15000) {
        wifi.update();
        delay(100);

        // Check for encoder button to force AP mode
        encoder.update();
        if (encoder.getButtonPressDuration() > BUTTON_VERY_LONG_PRESS_MS) {
            Serial.println(F("Button held - entering AP mode"));
            wifi.forceAPMode();
            break;
        }
    }

    // Update connection status
    if (wifi.isConnected()) {
        systemState = STATE_NORMAL;
        wifiConnected = true;
        ipAddress = wifi.getIPAddress();
        display.showMessage("WiFi Connected", ipAddress.c_str());
    } else if (wifi.isAPMode()) {
        systemState = STATE_AP_MODE;
        ipAddress = wifi.getIPAddress();
        display.showMessage("AP Mode", wifi.getAPSSID().c_str());
    } else {
        systemState = STATE_OFFLINE;
        display.showMessage("Offline Mode", "No WiFi");
    }
    delay(1500);

    // Initialize NTP and scheduler (only if WiFi connected)
    if (wifi.isConnected()) {
        Serial.println(F("Initializing NTP..."));
        scheduler.begin();
    }

    // Initialize MQTT (only if WiFi connected and MQTT enabled)
    if (wifi.isConnected() && config.mqtt.enabled) {
        Serial.println(F("Initializing MQTT..."));
        mqtt.begin();
    }

    // Initialize web server
    Serial.println(F("Initializing web server..."));
    webServer.begin();

    // Initial temperature reading
    Serial.println(F("Reading temperatures..."));
    temps.requestTemperatures();
    delay(800); // Wait for conversion
    temps.update();

    Serial.println(F("====================================="));
    Serial.println(F("  Initialization Complete!"));
    Serial.println(F("====================================="));
    Serial.println();

    // Update display to show initial state
    display.update();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    unsigned long now = millis();

    // Always update encoder (needs fast polling)
    encoder.update();

    // Handle encoder events
    handleEncoderEvents();

    // Update WiFi connection
    wifi.update();
    wifiConnected = wifi.isConnected();
    if (wifiConnected) {
        ipAddress = wifi.getIPAddress();
        if (systemState != STATE_NORMAL) {
            systemState = STATE_NORMAL;
        }
    } else if (wifi.isAPMode()) {
        ipAddress = wifi.getIPAddress();
        systemState = STATE_AP_MODE;
    } else if (systemState == STATE_NORMAL) {
        systemState = STATE_OFFLINE;
    }

    // Temperature reading (every 30 seconds)
    if (now - lastTempRead >= TEMP_READ_INTERVAL || lastTempRead == 0) {
        temps.requestTemperatures();
        lastTempRead = now;

        // Update readings after conversion time
        // Note: We'll update on next iteration since we use non-blocking mode
    }

    // Update temperature readings (slightly after request)
    if (now - lastTempRead > 800 && now - lastTempRead < 1000) {
        temps.update();

        // Log temperatures to serial
        const TemperatureManager::Readings& readings = temps.getReadings();
        Serial.print(F("Temps: F="));
        Serial.print(readings.floor, 1);
        Serial.print(F(" A="));
        Serial.print(readings.air, 1);
        Serial.print(F(" O="));
        Serial.print(readings.outdoor, 1);
        Serial.print(F(" WI="));
        Serial.print(readings.waterIn, 1);
        Serial.print(F(" WO="));
        Serial.print(readings.waterOut, 1);
        Serial.print(F(" dT="));
        Serial.println(readings.waterDelta, 1);
    }

    // Control logic update (every 10 seconds)
    if (now - lastControlUpdate >= CONTROL_INTERVAL) {
        controller.update();
        lastControlUpdate = now;
    }

    // Schedule check (every 60 seconds)
    if (now - lastScheduleCheck >= SCHEDULE_INTERVAL) {
        scheduler.update();
        scheduleActive = scheduler.isScheduleActive();
        scheduleInfo = scheduler.getScheduleInfo();
        lastScheduleCheck = now;
    }

    // Display update (every second)
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        // Update uptime
        uptimeSeconds = scheduler.getUptimeSeconds();

        // Check menu timeout
        if (inMenu && now - menuTimeout > MENU_TIMEOUT_MS) {
            display.exitMenu();
            inMenu = false;
            config.save();  // Save any changes made in menu
        }

        // Update display
        if (!inMenu) {
            display.update();
        }
        lastDisplayUpdate = now;
    }

    // MQTT handling
    if (config.mqtt.enabled && wifiConnected) {
        mqtt.update();
        mqttConnected = mqtt.isConnected();

        // Publish state periodically
        if (mqtt.shouldPublish()) {
            mqtt.publishState();
            lastMqttPublish = now;
        }
    }

    // NTP sync (every hour)
    if (wifiConnected && now - lastNtpSync >= NTP_SYNC_INTERVAL) {
        scheduler.syncNTP();
        lastNtpSync = now;
    }

    // Reset runtime counters at midnight
    static int lastDay = -1;
    int currentDay = scheduler.getDayOfWeek();
    if (currentDay != lastDay && lastDay != -1) {
        controller.resetAllRuntimeCounters();
    }
    lastDay = currentDay;

    // Small delay to prevent watchdog issues
    yield();
}

// ============================================================================
// ENCODER EVENT HANDLING
// ============================================================================

void handleEncoderEvents() {
    EncoderHandler::Event event = encoder.getEvent();

    switch (event) {
        case EncoderHandler::EVENT_BUTTON_SHORT:
            if (inMenu) {
                display.menuSelect();
                menuTimeout = millis();  // Reset timeout
            } else {
                display.nextMode();
            }
            break;

        case EncoderHandler::EVENT_BUTTON_LONG:
            if (!inMenu) {
                display.enterMenu();
                inMenu = true;
                menuTimeout = millis();
            } else {
                display.exitMenu();
                inMenu = false;
                config.save();  // Save any changes made in menu
            }
            break;

        case EncoderHandler::EVENT_BUTTON_VERY_LONG:
            // Enter AP mode
            Serial.println(F("Entering AP mode via button"));
            display.showMessage("Entering", "AP Mode...");
            wifi.forceAPMode();
            break;

        case EncoderHandler::EVENT_ROTATE_CW:
            if (inMenu) {
                display.menuDown();
                menuTimeout = millis();
            }
            break;

        case EncoderHandler::EVENT_ROTATE_CCW:
            if (inMenu) {
                display.menuUp();
                menuTimeout = millis();
            }
            break;

        default:
            break;
    }
}
