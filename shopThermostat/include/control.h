#ifndef CONTROL_H
#define CONTROL_H

#include <Arduino.h>
#include "config.h"
#include "temperature.h"
#include "storage.h"

// ============================================================================
// THERMOSTAT CONTROL LOGIC
// ============================================================================

struct ZoneState {
    bool relayOn;
    bool thermalRunaway;
    bool sensorFault;
    bool maxRuntimeExceeded;
    unsigned long relayOnTime;
    unsigned long lastStateChange;
    unsigned long totalRuntime;      // Runtime today
    unsigned long cycleCount;
};

class ThermostatController {
private:
    ZoneState _zoneState[ZONE_COUNT];
    ConfigManager* _config;
    TemperatureManager* _temps;
    bool _safeMode;
    String _lastError;

public:
    ThermostatController(ConfigManager* config, TemperatureManager* temps)
        : _config(config), _temps(temps), _safeMode(false) {
        for (int i = 0; i < ZONE_COUNT; i++) {
            _zoneState[i].relayOn = false;
            _zoneState[i].thermalRunaway = false;
            _zoneState[i].sensorFault = false;
            _zoneState[i].maxRuntimeExceeded = false;
            _zoneState[i].relayOnTime = 0;
            _zoneState[i].lastStateChange = 0;
            _zoneState[i].totalRuntime = 0;
            _zoneState[i].cycleCount = 0;
        }
    }

    void begin() {
        // Initialize relay pins
        pinMode(PIN_RELAY_PUMP, OUTPUT);
        pinMode(PIN_RELAY_HEATER, OUTPUT);

        // Start with relays OFF
        setRelay(ZONE_FLOOR, false);
        setRelay(ZONE_AIR, false);

        Serial.println(F("Thermostat controller initialized"));
    }

    void update() {
        const TemperatureManager::Readings& readings = _temps->getReadings();

        // Update zone control
        updateZone(ZONE_FLOOR, readings.floor, readings.valid[SENSOR_FLOOR]);
        updateZone(ZONE_AIR, readings.air, readings.valid[SENSOR_AIR]);

        // Check for smart pump control if enabled
        if (_config->water.enabled && _config->water.smartPumpControl) {
            applySmartPumpControl(readings);
        }
    }

    void updateZone(ZoneId zone, float currentTemp, bool sensorValid) {
        ZoneConfig& zoneConfig = _config->zones[zone];
        ZoneState& state = _zoneState[zone];

        // Check for sensor fault
        if (!sensorValid || _temps->isSensorFault(zone == ZONE_FLOOR ? SENSOR_FLOOR : SENSOR_AIR)) {
            state.sensorFault = true;
            setRelay(zone, false);
            _lastError = "Sensor fault on " + String(zone == ZONE_FLOOR ? "floor" : "air");
            return;
        }
        state.sensorFault = false;

        // Check for thermal runaway
        float runawayLimit = (zone == ZONE_FLOOR) ? FLOOR_THERMAL_RUNAWAY : AIR_THERMAL_RUNAWAY;
        if (currentTemp > runawayLimit) {
            state.thermalRunaway = true;
            setRelay(zone, false);
            _lastError = "Thermal runaway on " + String(zone == ZONE_FLOOR ? "floor" : "air");
            Serial.print(F("THERMAL RUNAWAY: "));
            Serial.println(_lastError);
            return;
        }

        // Don't auto-recover from thermal runaway - requires manual reset
        if (state.thermalRunaway) {
            setRelay(zone, false);
            return;
        }

        // Check if zone is enabled
        if (!zoneConfig.enabled) {
            setRelay(zone, false);
            return;
        }

        // Check for manual override
        if (zoneConfig.override != OVERRIDE_AUTO) {
            // Check override timeout
            if (zoneConfig.overrideTime > 0 &&
                millis() - zoneConfig.overrideTime > MANUAL_OVERRIDE_TIMEOUT_MS) {
                zoneConfig.override = OVERRIDE_AUTO;
                zoneConfig.overrideTime = 0;
            } else {
                bool shouldBeOn = (zoneConfig.override == OVERRIDE_ON);
                setRelay(zone, shouldBeOn);
                return;
            }
        }

        // Check maximum runtime
        if (state.relayOn) {
            unsigned long runtime = millis() - state.relayOnTime;
            if (runtime > _config->system.maxRuntime) {
                state.maxRuntimeExceeded = true;
                setRelay(zone, false);
                _lastError = "Max runtime exceeded on " + String(zone == ZONE_FLOOR ? "floor" : "air");
                Serial.println(_lastError);
                return;
            }
        }

        // Check minimum cycle time
        if (state.lastStateChange > 0) {
            unsigned long sinceChange = millis() - state.lastStateChange;
            if (sinceChange < _config->system.minCycleTime) {
                return; // Don't change state yet
            }
        }

        // Get effective target (may be modified by schedule)
        float target = getEffectiveTarget(zone);

        // Hysteresis control logic
        float hysteresis = zoneConfig.hysteresis;
        float lowThreshold = target - (hysteresis / 2.0f);
        float highThreshold = target + (hysteresis / 2.0f);

        bool shouldBeOn = state.relayOn; // Start with current state

        if (currentTemp < lowThreshold) {
            shouldBeOn = true;
        } else if (currentTemp > highThreshold) {
            shouldBeOn = false;
        }

        // Apply state change if needed
        if (shouldBeOn != state.relayOn) {
            setRelay(zone, shouldBeOn);
        }
    }

    void applySmartPumpControl(const TemperatureManager::Readings& readings) {
        // Only apply smart control to floor pump
        if (!_zoneState[ZONE_FLOOR].relayOn) return;

        // If delta-T is too low, don't pump cold water
        if (readings.valid[SENSOR_WATER_IN] && readings.valid[SENSOR_WATER_OUT]) {
            if (readings.waterDelta < _config->water.deltaTWarningLow) {
                // Tank has no heat to give, turn off pump
                setRelay(ZONE_FLOOR, false);
                Serial.println(F("Smart pump: Off due to low delta-T"));
            }
        }
    }

    float getEffectiveTarget(ZoneId zone) {
        // This can be overridden by scheduler
        return _config->zones[zone].targetTemp;
    }

    void setEffectiveTarget(ZoneId zone, float target) {
        // Clamp to valid range
        if (zone == ZONE_FLOOR) {
            target = constrain(target, MIN_FLOOR_TARGET, MAX_FLOOR_TARGET);
        } else {
            target = constrain(target, MIN_AIR_TARGET, MAX_AIR_TARGET);
        }
        _config->zones[zone].targetTemp = target;
    }

    void setRelay(ZoneId zone, bool on) {
        ZoneState& state = _zoneState[zone];

        if (on != state.relayOn) {
            state.lastStateChange = millis();
            state.cycleCount++;

            if (on) {
                state.relayOnTime = millis();
            } else {
                // Add to total runtime
                if (state.relayOnTime > 0) {
                    state.totalRuntime += (millis() - state.relayOnTime);
                }
                state.relayOnTime = 0;
            }
        }

        state.relayOn = on;

        // Apply to hardware
        if (zone == ZONE_FLOOR) {
            digitalWrite(PIN_RELAY_PUMP, on ? RELAY_ON : RELAY_OFF);
        } else {
            digitalWrite(PIN_RELAY_HEATER, on ? RELAY_ON : RELAY_OFF);
        }
    }

    void setOverride(ZoneId zone, OverrideMode mode) {
        _config->zones[zone].override = mode;
        if (mode != OVERRIDE_AUTO) {
            _config->zones[zone].overrideTime = millis();
        } else {
            _config->zones[zone].overrideTime = 0;
        }
    }

    void resetThermalRunaway(ZoneId zone) {
        _zoneState[zone].thermalRunaway = false;
        _zoneState[zone].maxRuntimeExceeded = false;
        Serial.print(F("Thermal runaway reset for zone "));
        Serial.println(zone);
    }

    void resetRuntimeCounter(ZoneId zone) {
        _zoneState[zone].totalRuntime = 0;
    }

    void resetAllRuntimeCounters() {
        for (int i = 0; i < ZONE_COUNT; i++) {
            _zoneState[i].totalRuntime = 0;
        }
    }

    // Getters
    bool isRelayOn(ZoneId zone) const {
        return _zoneState[zone].relayOn;
    }

    bool isThermalRunaway(ZoneId zone) const {
        return _zoneState[zone].thermalRunaway;
    }

    bool isSensorFault(ZoneId zone) const {
        return _zoneState[zone].sensorFault;
    }

    bool isMaxRuntimeExceeded(ZoneId zone) const {
        return _zoneState[zone].maxRuntimeExceeded;
    }

    unsigned long getRuntime(ZoneId zone) const {
        const ZoneState& state = _zoneState[zone];
        unsigned long runtime = state.totalRuntime;
        if (state.relayOn && state.relayOnTime > 0) {
            runtime += (millis() - state.relayOnTime);
        }
        return runtime;
    }

    unsigned long getCurrentSessionRuntime(ZoneId zone) const {
        const ZoneState& state = _zoneState[zone];
        if (state.relayOn && state.relayOnTime > 0) {
            return millis() - state.relayOnTime;
        }
        return 0;
    }

    unsigned long getCycleCount(ZoneId zone) const {
        return _zoneState[zone].cycleCount;
    }

    const String& getLastError() const {
        return _lastError;
    }

    bool isInSafeMode() const {
        return _safeMode;
    }

    void enterSafeMode() {
        _safeMode = true;
        setRelay(ZONE_FLOOR, false);
        setRelay(ZONE_AIR, false);
        Serial.println(F("Entered safe mode - all heating disabled"));
    }

    void exitSafeMode() {
        _safeMode = false;
        Serial.println(F("Exited safe mode"));
    }

    // Format runtime as string
    static String formatRuntime(unsigned long ms) {
        unsigned long seconds = ms / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;

        seconds %= 60;
        minutes %= 60;

        char buf[16];
        if (hours > 0) {
            snprintf(buf, sizeof(buf), "%luh %lum", hours, minutes);
        } else if (minutes > 0) {
            snprintf(buf, sizeof(buf), "%lum %lus", minutes, seconds);
        } else {
            snprintf(buf, sizeof(buf), "%lus", seconds);
        }
        return String(buf);
    }

    // Get status string for zone
    String getZoneStatus(ZoneId zone) const {
        if (_zoneState[zone].thermalRunaway) return "RUNAWAY";
        if (_zoneState[zone].sensorFault) return "FAULT";
        if (_zoneState[zone].maxRuntimeExceeded) return "MAX_RUN";
        if (_config->zones[zone].override == OVERRIDE_ON) return "FORCE_ON";
        if (_config->zones[zone].override == OVERRIDE_OFF) return "FORCE_OFF";
        if (!_config->zones[zone].enabled) return "DISABLED";
        return _zoneState[zone].relayOn ? "HEATING" : "IDLE";
    }
};

#endif // CONTROL_H
