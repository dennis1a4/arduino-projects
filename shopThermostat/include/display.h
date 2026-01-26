#ifndef DISPLAY_H
#define DISPLAY_H

#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "temperature.h"
#include "control.h"
#include "storage.h"

// ============================================================================
// LCD DISPLAY MANAGER
// ============================================================================

// Custom characters
#define CHAR_DEGREE     0
#define CHAR_WIFI_ON    1
#define CHAR_WIFI_OFF   2
#define CHAR_HEAT       3
#define CHAR_DROP       4

class DisplayManager {
public:
    enum DisplayMode {
        MODE_TEMPS,         // F:5.2° A:18.5° / P:ON H:OFF
        MODE_TARGETS,       // T:5/20° Out:-2° / WiFi:OK MQTT:OK
        MODE_WATER_TEMPS,   // Tank In:  45.2°C / Tank Out: 42.8°C
        MODE_WATER_DELTA,   // ΔT: 2.4°C Flow:OK / Pump Runtime: 3h
        MODE_SCHEDULE,      // Sched: ACTIVE / 08:00-17:00 20°C
        MODE_SYSTEM         // IP:192.168.1.50 / Up: 5d 3h 22m
    };

private:
    LiquidCrystal_I2C _lcd;
    DisplayMode _currentMode;
    bool _menuActive;
    int _menuIndex;
    bool _adjustingValue;
    unsigned long _lastUpdate;
    bool _backlightOn;

    // Custom character definitions
    uint8_t degreeChar[8] = {0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00};
    uint8_t wifiOnChar[8] = {0x00, 0x0E, 0x11, 0x04, 0x0A, 0x00, 0x04, 0x00};
    uint8_t wifiOffChar[8] = {0x00, 0x0E, 0x11, 0x05, 0x0B, 0x01, 0x05, 0x00};
    uint8_t heatChar[8] = {0x04, 0x02, 0x04, 0x02, 0x04, 0x02, 0x04, 0x00};
    uint8_t dropChar[8] = {0x04, 0x04, 0x0A, 0x0A, 0x11, 0x11, 0x0E, 0x00};

    // References to other components
    TemperatureManager* _temps;
    ThermostatController* _controller;
    ConfigManager* _config;
    bool* _wifiConnected;
    bool* _mqttConnected;
    String* _ipAddress;
    unsigned long* _uptimeSeconds;
    bool* _scheduleActive;
    String* _scheduleInfo;

public:
    DisplayManager()
        : _lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS),
          _currentMode(MODE_TEMPS),
          _menuActive(false),
          _menuIndex(0),
          _adjustingValue(false),
          _lastUpdate(0),
          _backlightOn(true),
          _temps(nullptr),
          _controller(nullptr),
          _config(nullptr),
          _wifiConnected(nullptr),
          _mqttConnected(nullptr),
          _ipAddress(nullptr),
          _uptimeSeconds(nullptr),
          _scheduleActive(nullptr),
          _scheduleInfo(nullptr) {}

    void begin() {
        _lcd.init();
        _lcd.backlight();

        // Create custom characters
        _lcd.createChar(CHAR_DEGREE, degreeChar);
        _lcd.createChar(CHAR_WIFI_ON, wifiOnChar);
        _lcd.createChar(CHAR_WIFI_OFF, wifiOffChar);
        _lcd.createChar(CHAR_HEAT, heatChar);
        _lcd.createChar(CHAR_DROP, dropChar);

        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print(F("Shop Thermostat"));
        _lcd.setCursor(0, 1);
        _lcd.print(F("Starting..."));
    }

    void setReferences(TemperatureManager* temps,
                       ThermostatController* controller,
                       ConfigManager* config,
                       bool* wifiConnected,
                       bool* mqttConnected,
                       String* ipAddress,
                       unsigned long* uptimeSeconds,
                       bool* scheduleActive,
                       String* scheduleInfo) {
        _temps = temps;
        _controller = controller;
        _config = config;
        _wifiConnected = wifiConnected;
        _mqttConnected = mqttConnected;
        _ipAddress = ipAddress;
        _uptimeSeconds = uptimeSeconds;
        _scheduleActive = scheduleActive;
        _scheduleInfo = scheduleInfo;
    }

    void update() {
        if (_menuActive) {
            updateMenu();
        } else {
            updateDisplay();
        }
        _lastUpdate = millis();
    }

    void updateDisplay() {
        if (!_temps || !_controller || !_config) return;

        const TemperatureManager::Readings& readings = _temps->getReadings();

        _lcd.clear();

        switch (_currentMode) {
            case MODE_TEMPS:
                displayTemps(readings);
                break;
            case MODE_TARGETS:
                displayTargets(readings);
                break;
            case MODE_WATER_TEMPS:
                displayWaterTemps(readings);
                break;
            case MODE_WATER_DELTA:
                displayWaterDelta(readings);
                break;
            case MODE_SCHEDULE:
                displaySchedule();
                break;
            case MODE_SYSTEM:
                displaySystem();
                break;
        }
    }

    void displayTemps(const TemperatureManager::Readings& readings) {
        // Line 1: F:5.2° A:18.5°
        _lcd.setCursor(0, 0);
        _lcd.print(F("F:"));
        if (readings.valid[SENSOR_FLOOR]) {
            printTemp(readings.floor);
        } else {
            _lcd.print(F("ERR"));
        }
        _lcd.print(F(" A:"));
        if (readings.valid[SENSOR_AIR]) {
            printTemp(readings.air);
        } else {
            _lcd.print(F("ERR"));
        }

        // Line 2: P:ON H:OFF
        _lcd.setCursor(0, 1);
        _lcd.print(F("P:"));
        _lcd.print(_controller->isRelayOn(ZONE_FLOOR) ? F("ON ") : F("OFF"));
        _lcd.print(F(" H:"));
        _lcd.print(_controller->isRelayOn(ZONE_AIR) ? F("ON ") : F("OFF"));
    }

    void displayTargets(const TemperatureManager::Readings& readings) {
        // Line 1: T:5/20° Out:-2°
        _lcd.setCursor(0, 0);
        _lcd.print(F("T:"));
        _lcd.print((int)_config->zones[ZONE_FLOOR].targetTemp);
        _lcd.print(F("/"));
        _lcd.print((int)_config->zones[ZONE_AIR].targetTemp);
        _lcd.write(CHAR_DEGREE);
        _lcd.print(F(" O:"));
        if (readings.valid[SENSOR_OUTDOOR]) {
            printTemp(readings.outdoor);
        } else {
            _lcd.print(F("--"));
        }

        // Line 2: WiFi:OK MQTT:OK
        _lcd.setCursor(0, 1);
        _lcd.write(_wifiConnected && *_wifiConnected ? CHAR_WIFI_ON : CHAR_WIFI_OFF);
        _lcd.print(_wifiConnected && *_wifiConnected ? F("OK ") : F("-- "));
        _lcd.print(F("MQTT:"));
        _lcd.print(_mqttConnected && *_mqttConnected ? F("OK") : F("--"));
    }

    void displayWaterTemps(const TemperatureManager::Readings& readings) {
        // Line 1: Tank In:  45.2°C
        _lcd.setCursor(0, 0);
        _lcd.print(F("Tank In: "));
        if (readings.valid[SENSOR_WATER_IN]) {
            printTemp(readings.waterIn);
            _lcd.write(CHAR_DEGREE);
            _lcd.print(_config->system.useFahrenheit ? F("F") : F("C"));
        } else {
            _lcd.print(F("ERR"));
        }

        // Line 2: Tank Out: 42.8°C
        _lcd.setCursor(0, 1);
        _lcd.print(F("Tank Out:"));
        if (readings.valid[SENSOR_WATER_OUT]) {
            printTemp(readings.waterOut);
            _lcd.write(CHAR_DEGREE);
            _lcd.print(_config->system.useFahrenheit ? F("F") : F("C"));
        } else {
            _lcd.print(F("ERR"));
        }
    }

    void displayWaterDelta(const TemperatureManager::Readings& readings) {
        // Line 1: ΔT: 2.4°C Flow:OK
        _lcd.setCursor(0, 0);
        _lcd.print(F("dT:"));
        if (readings.valid[SENSOR_WATER_IN] && readings.valid[SENSOR_WATER_OUT]) {
            _lcd.print(readings.waterDelta, 1);
            _lcd.write(CHAR_DEGREE);
            _lcd.print(F(" "));
            FlowStatus fs = _temps->getFlowStatus();
            _lcd.print(TemperatureManager::getFlowStatusString(fs));
        } else {
            _lcd.print(F("-- ERR"));
        }

        // Line 2: Pump: 3h 24m
        _lcd.setCursor(0, 1);
        _lcd.print(F("Pump:"));
        _lcd.print(ThermostatController::formatRuntime(_controller->getRuntime(ZONE_FLOOR)));
    }

    void displaySchedule() {
        // Line 1: Sched: ACTIVE or OFF
        _lcd.setCursor(0, 0);
        _lcd.print(F("Sched: "));
        if (_scheduleActive && *_scheduleActive) {
            _lcd.print(F("ACTIVE"));
        } else {
            _lcd.print(F("OFF"));
        }

        // Line 2: Schedule info or --
        _lcd.setCursor(0, 1);
        if (_scheduleInfo && _scheduleInfo->length() > 0) {
            _lcd.print(*_scheduleInfo);
        } else {
            _lcd.print(F("No active sched"));
        }
    }

    void displaySystem() {
        // Line 1: IP address
        _lcd.setCursor(0, 0);
        _lcd.print(F("IP:"));
        if (_ipAddress && _ipAddress->length() > 0) {
            _lcd.print(*_ipAddress);
        } else {
            _lcd.print(F("Not connected"));
        }

        // Line 2: Uptime
        _lcd.setCursor(0, 1);
        _lcd.print(F("Up:"));
        if (_uptimeSeconds) {
            unsigned long secs = *_uptimeSeconds;
            unsigned long mins = secs / 60;
            unsigned long hours = mins / 60;
            unsigned long days = hours / 24;

            if (days > 0) {
                _lcd.print(days);
                _lcd.print(F("d "));
                _lcd.print(hours % 24);
                _lcd.print(F("h"));
            } else if (hours > 0) {
                _lcd.print(hours);
                _lcd.print(F("h "));
                _lcd.print(mins % 60);
                _lcd.print(F("m"));
            } else {
                _lcd.print(mins);
                _lcd.print(F("m "));
                _lcd.print(secs % 60);
                _lcd.print(F("s"));
            }
        } else {
            _lcd.print(F("--"));
        }
    }

    void printTemp(float temp) {
        if (_config && _config->system.useFahrenheit) {
            temp = temp * 9.0f / 5.0f + 32.0f;
        }

        if (temp >= 100 || temp <= -10) {
            _lcd.print((int)temp);
        } else {
            _lcd.print(temp, 1);
        }
    }

    void nextMode() {
        _currentMode = (DisplayMode)(((int)_currentMode + 1) % DISPLAY_MODE_COUNT);
        update();
    }

    void previousMode() {
        int mode = (int)_currentMode - 1;
        if (mode < 0) mode = DISPLAY_MODE_COUNT - 1;
        _currentMode = (DisplayMode)mode;
        update();
    }

    DisplayMode getCurrentMode() const {
        return _currentMode;
    }

    // Menu system
    void enterMenu() {
        _menuActive = true;
        _menuIndex = 0;
        _adjustingValue = false;
        updateMenu();
    }

    void exitMenu() {
        _menuActive = false;
        update();
    }

    bool isMenuActive() const {
        return _menuActive;
    }

    void menuUp() {
        if (_adjustingValue) {
            adjustValue(1);
        } else {
            _menuIndex = (_menuIndex - 1 + 7) % 7;
        }
        updateMenu();
    }

    void menuDown() {
        if (_adjustingValue) {
            adjustValue(-1);
        } else {
            _menuIndex = (_menuIndex + 1) % 7;
        }
        updateMenu();
    }

    void menuSelect() {
        if (_menuIndex == 6) {
            // Reboot
            _lcd.clear();
            _lcd.print(F("Rebooting..."));
            delay(1000);
            ESP.restart();
        } else if (_menuIndex >= 2 && _menuIndex <= 5) {
            // Adjustable values
            _adjustingValue = !_adjustingValue;
        }
        updateMenu();
    }

    void adjustValue(int direction) {
        if (!_config) return;

        switch (_menuIndex) {
            case 2: // Floor target
                _config->zones[ZONE_FLOOR].targetTemp += direction * 0.5f;
                _config->zones[ZONE_FLOOR].targetTemp =
                    constrain(_config->zones[ZONE_FLOOR].targetTemp, MIN_FLOOR_TARGET, MAX_FLOOR_TARGET);
                break;
            case 3: // Air target
                _config->zones[ZONE_AIR].targetTemp += direction * 0.5f;
                _config->zones[ZONE_AIR].targetTemp =
                    constrain(_config->zones[ZONE_AIR].targetTemp, MIN_AIR_TARGET, MAX_AIR_TARGET);
                break;
            case 4: // Floor override
                {
                    int mode = (int)_config->zones[ZONE_FLOOR].override + direction;
                    mode = (mode + 3) % 3;
                    _config->zones[ZONE_FLOOR].override = (OverrideMode)mode;
                    if (_controller) {
                        _controller->setOverride(ZONE_FLOOR, (OverrideMode)mode);
                    }
                }
                break;
            case 5: // Air override
                {
                    int mode = (int)_config->zones[ZONE_AIR].override + direction;
                    mode = (mode + 3) % 3;
                    _config->zones[ZONE_AIR].override = (OverrideMode)mode;
                    if (_controller) {
                        _controller->setOverride(ZONE_AIR, (OverrideMode)mode);
                    }
                }
                break;
        }
    }

    void updateMenu() {
        _lcd.clear();
        _lcd.setCursor(0, 0);

        const char* menuItems[] = {
            "WiFi Info",
            "MQTT Status",
            "Floor Target",
            "Air Target",
            "Floor Override",
            "Air Override",
            "Reboot System"
        };

        _lcd.print(F(">"));
        _lcd.print(menuItems[_menuIndex]);

        _lcd.setCursor(0, 1);

        switch (_menuIndex) {
            case 0: // WiFi Info
                if (_wifiConnected && *_wifiConnected && _ipAddress) {
                    _lcd.print(*_ipAddress);
                } else {
                    _lcd.print(F("Not connected"));
                }
                break;
            case 1: // MQTT Status
                if (_mqttConnected && *_mqttConnected) {
                    _lcd.print(F("Connected"));
                } else {
                    _lcd.print(F("Disconnected"));
                }
                break;
            case 2: // Floor Target
                if (_adjustingValue) _lcd.print(F("["));
                _lcd.print(_config->zones[ZONE_FLOOR].targetTemp, 1);
                _lcd.write(CHAR_DEGREE);
                _lcd.print(_config->system.useFahrenheit ? F("F") : F("C"));
                if (_adjustingValue) _lcd.print(F("]"));
                break;
            case 3: // Air Target
                if (_adjustingValue) _lcd.print(F("["));
                _lcd.print(_config->zones[ZONE_AIR].targetTemp, 1);
                _lcd.write(CHAR_DEGREE);
                _lcd.print(_config->system.useFahrenheit ? F("F") : F("C"));
                if (_adjustingValue) _lcd.print(F("]"));
                break;
            case 4: // Floor Override
                if (_adjustingValue) _lcd.print(F("["));
                _lcd.print(getOverrideName(_config->zones[ZONE_FLOOR].override));
                if (_adjustingValue) _lcd.print(F("]"));
                break;
            case 5: // Air Override
                if (_adjustingValue) _lcd.print(F("["));
                _lcd.print(getOverrideName(_config->zones[ZONE_AIR].override));
                if (_adjustingValue) _lcd.print(F("]"));
                break;
            case 6: // Reboot
                _lcd.print(F("Press to reboot"));
                break;
        }
    }

    const char* getOverrideName(OverrideMode mode) {
        switch (mode) {
            case OVERRIDE_AUTO: return "Auto";
            case OVERRIDE_ON: return "Force ON";
            case OVERRIDE_OFF: return "Force OFF";
            default: return "Unknown";
        }
    }

    void showMessage(const char* line1, const char* line2 = nullptr) {
        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print(line1);
        if (line2) {
            _lcd.setCursor(0, 1);
            _lcd.print(line2);
        }
    }

    void showError(const char* error) {
        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print(F("ERROR:"));
        _lcd.setCursor(0, 1);
        _lcd.print(error);
    }

    void setBacklight(bool on) {
        _backlightOn = on;
        if (on) {
            _lcd.backlight();
        } else {
            _lcd.noBacklight();
        }
    }

    bool isBacklightOn() const {
        return _backlightOn;
    }

    void toggleBacklight() {
        setBacklight(!_backlightOn);
    }
};

#endif // DISPLAY_H
