#ifndef STORAGE_H
#define STORAGE_H

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================================================
// CONFIGURATION STORAGE MANAGER
// ============================================================================

#define CONFIG_FILE "/config.json"

struct Schedule {
    bool enabled;
    uint8_t days;           // Bitmask: bit 0=Sunday, bit 6=Saturday
    uint8_t startHour;
    uint8_t startMinute;
    uint8_t endHour;
    uint8_t endMinute;
    float targetTemp;
    uint8_t zone;           // ZONE_FLOOR or ZONE_AIR
};

struct ZoneConfig {
    float targetTemp;
    float hysteresis;
    bool enabled;
    OverrideMode override;
    unsigned long overrideTime;
};

struct WaterConfig {
    bool enabled;
    float deltaTWarningLow;
    float deltaTWarningHigh;
    bool smartPumpControl;
};

struct MqttConfig {
    bool enabled;
    char broker[64];
    uint16_t port;
    char username[32];
    char password[32];
    char baseTopic[64];
};

struct WifiConfig {
    char ssid[32];
    char password[64];
};

struct SystemConfig {
    char deviceName[32];
    char timezone[32];
    bool useFahrenheit;
    unsigned long maxRuntime;
    unsigned long minCycleTime;
};

struct SensorConfig {
    char addresses[SENSOR_COUNT][17];  // 16 hex chars + null
    float calibration[SENSOR_COUNT];
};

class ConfigManager {
public:
    ZoneConfig zones[ZONE_COUNT];
    WaterConfig water;
    MqttConfig mqtt;
    WifiConfig wifi;
    SystemConfig system;
    SensorConfig sensors;
    Schedule schedules[MAX_SCHEDULES];

private:
    bool _initialized;

public:
    ConfigManager() : _initialized(false) {
        setDefaults();
    }

    void setDefaults() {
        // Floor zone defaults
        zones[ZONE_FLOOR].targetTemp = DEFAULT_FLOOR_TARGET;
        zones[ZONE_FLOOR].hysteresis = DEFAULT_FLOOR_HYSTERESIS;
        zones[ZONE_FLOOR].enabled = true;
        zones[ZONE_FLOOR].override = OVERRIDE_AUTO;
        zones[ZONE_FLOOR].overrideTime = 0;

        // Air zone defaults
        zones[ZONE_AIR].targetTemp = DEFAULT_AIR_TARGET;
        zones[ZONE_AIR].hysteresis = DEFAULT_AIR_HYSTERESIS;
        zones[ZONE_AIR].enabled = true;
        zones[ZONE_AIR].override = OVERRIDE_AUTO;
        zones[ZONE_AIR].overrideTime = 0;

        // Water monitoring defaults
        water.enabled = true;
        water.deltaTWarningLow = DEFAULT_DELTA_T_WARNING_LOW;
        water.deltaTWarningHigh = DEFAULT_DELTA_T_WARNING_HIGH;
        water.smartPumpControl = false;

        // MQTT defaults
        mqtt.enabled = false;
        strcpy(mqtt.broker, "");
        mqtt.port = DEFAULT_MQTT_PORT;
        strcpy(mqtt.username, "");
        strcpy(mqtt.password, "");
        strcpy(mqtt.baseTopic, DEFAULT_MQTT_BASE_TOPIC);

        // WiFi defaults
        strcpy(wifi.ssid, "");
        strcpy(wifi.password, "");

        // System defaults
        strcpy(system.deviceName, "Shop Thermostat");
        strcpy(system.timezone, "America/Winnipeg");
        system.useFahrenheit = false;
        system.maxRuntime = MAX_RUNTIME_MS;
        system.minCycleTime = MIN_CYCLE_TIME_MS;

        // Sensor defaults (empty addresses)
        for (int i = 0; i < SENSOR_COUNT; i++) {
            strcpy(sensors.addresses[i], "");
            sensors.calibration[i] = 0.0f;
        }

        // Schedule defaults (all disabled)
        for (int i = 0; i < MAX_SCHEDULES; i++) {
            schedules[i].enabled = false;
            schedules[i].days = 0;
            schedules[i].startHour = 8;
            schedules[i].startMinute = 0;
            schedules[i].endHour = 17;
            schedules[i].endMinute = 0;
            schedules[i].targetTemp = DEFAULT_AIR_TARGET;
            schedules[i].zone = ZONE_AIR;
        }
    }

    bool begin() {
        if (!LittleFS.begin()) {
            Serial.println(F("Failed to mount LittleFS"));
            return false;
        }
        _initialized = true;
        return true;
    }

    bool load() {
        if (!_initialized) return false;

        File file = LittleFS.open(CONFIG_FILE, "r");
        if (!file) {
            Serial.println(F("Config file not found, using defaults"));
            return false;
        }

        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            Serial.print(F("Failed to parse config: "));
            Serial.println(error.c_str());
            return false;
        }

        // Load WiFi config
        if (doc.containsKey("wifi")) {
            JsonObject wifiObj = doc["wifi"];
            strlcpy(wifi.ssid, wifiObj["ssid"] | "", sizeof(wifi.ssid));
            strlcpy(wifi.password, wifiObj["password"] | "", sizeof(wifi.password));
        }

        // Load MQTT config
        if (doc.containsKey("mqtt")) {
            JsonObject mqttObj = doc["mqtt"];
            mqtt.enabled = mqttObj["enabled"] | false;
            strlcpy(mqtt.broker, mqttObj["broker"] | "", sizeof(mqtt.broker));
            mqtt.port = mqttObj["port"] | DEFAULT_MQTT_PORT;
            strlcpy(mqtt.username, mqttObj["username"] | "", sizeof(mqtt.username));
            strlcpy(mqtt.password, mqttObj["password"] | "", sizeof(mqtt.password));
            strlcpy(mqtt.baseTopic, mqttObj["baseTopic"] | DEFAULT_MQTT_BASE_TOPIC, sizeof(mqtt.baseTopic));
        }

        // Load zone configs
        if (doc.containsKey("zones")) {
            JsonObject zonesObj = doc["zones"];

            if (zonesObj.containsKey("floor")) {
                JsonObject floor = zonesObj["floor"];
                zones[ZONE_FLOOR].targetTemp = floor["target"] | DEFAULT_FLOOR_TARGET;
                zones[ZONE_FLOOR].hysteresis = floor["hysteresis"] | DEFAULT_FLOOR_HYSTERESIS;
                zones[ZONE_FLOOR].enabled = floor["enabled"] | true;
            }

            if (zonesObj.containsKey("air")) {
                JsonObject air = zonesObj["air"];
                zones[ZONE_AIR].targetTemp = air["target"] | DEFAULT_AIR_TARGET;
                zones[ZONE_AIR].hysteresis = air["hysteresis"] | DEFAULT_AIR_HYSTERESIS;
                zones[ZONE_AIR].enabled = air["enabled"] | true;
            }
        }

        // Load water monitoring config
        if (doc.containsKey("water_monitoring")) {
            JsonObject waterObj = doc["water_monitoring"];
            water.enabled = waterObj["enabled"] | true;
            water.deltaTWarningLow = waterObj["delta_t_warning_low"] | DEFAULT_DELTA_T_WARNING_LOW;
            water.deltaTWarningHigh = waterObj["delta_t_warning_high"] | DEFAULT_DELTA_T_WARNING_HIGH;
            water.smartPumpControl = waterObj["smart_pump_control"] | false;
        }

        // Load sensor config
        if (doc.containsKey("sensors")) {
            JsonObject sensorsObj = doc["sensors"];

            const char* sensorNames[] = {"floor", "air", "outdoor", "water_in", "water_out"};
            for (int i = 0; i < SENSOR_COUNT; i++) {
                if (sensorsObj.containsKey(sensorNames[i])) {
                    strlcpy(sensors.addresses[i], sensorsObj[sensorNames[i]] | "", sizeof(sensors.addresses[i]));
                }
            }

            if (sensorsObj.containsKey("calibration")) {
                JsonObject calObj = sensorsObj["calibration"];
                for (int i = 0; i < SENSOR_COUNT; i++) {
                    sensors.calibration[i] = calObj[sensorNames[i]] | 0.0f;
                }
            }
        }

        // Load system config
        if (doc.containsKey("system")) {
            JsonObject sysObj = doc["system"];
            strlcpy(system.deviceName, sysObj["device_name"] | "Shop Thermostat", sizeof(system.deviceName));
            strlcpy(system.timezone, sysObj["timezone"] | "America/Winnipeg", sizeof(system.timezone));
            system.useFahrenheit = sysObj["temp_unit"].as<String>() == "F";
            system.maxRuntime = sysObj["max_runtime"] | MAX_RUNTIME_MS;
            system.minCycleTime = sysObj["min_cycle_time"] | MIN_CYCLE_TIME_MS;
        }

        // Load schedules
        if (doc.containsKey("schedules")) {
            JsonArray schedArray = doc["schedules"];
            int i = 0;
            for (JsonObject sched : schedArray) {
                if (i >= MAX_SCHEDULES) break;

                schedules[i].enabled = sched["enabled"] | false;
                schedules[i].zone = (sched["zone"].as<String>() == "floor") ? ZONE_FLOOR : ZONE_AIR;
                schedules[i].targetTemp = sched["target_temp"] | DEFAULT_AIR_TARGET;

                // Parse days array
                schedules[i].days = 0;
                if (sched.containsKey("days")) {
                    JsonArray daysArray = sched["days"];
                    for (int d : daysArray) {
                        if (d >= 0 && d <= 6) {
                            schedules[i].days |= (1 << d);
                        }
                    }
                }

                // Parse times
                String startTime = sched["start_time"] | "08:00";
                String endTime = sched["end_time"] | "17:00";

                schedules[i].startHour = startTime.substring(0, 2).toInt();
                schedules[i].startMinute = startTime.substring(3, 5).toInt();
                schedules[i].endHour = endTime.substring(0, 2).toInt();
                schedules[i].endMinute = endTime.substring(3, 5).toInt();

                i++;
            }
        }

        Serial.println(F("Configuration loaded successfully"));
        return true;
    }

    bool save() {
        if (!_initialized) return false;

        DynamicJsonDocument doc(4096);

        // Save WiFi config
        JsonObject wifiObj = doc.createNestedObject("wifi");
        wifiObj["ssid"] = wifi.ssid;
        wifiObj["password"] = wifi.password;

        // Save MQTT config
        JsonObject mqttObj = doc.createNestedObject("mqtt");
        mqttObj["enabled"] = mqtt.enabled;
        mqttObj["broker"] = mqtt.broker;
        mqttObj["port"] = mqtt.port;
        mqttObj["username"] = mqtt.username;
        mqttObj["password"] = mqtt.password;
        mqttObj["baseTopic"] = mqtt.baseTopic;

        // Save zone configs
        JsonObject zonesObj = doc.createNestedObject("zones");

        JsonObject floorObj = zonesObj.createNestedObject("floor");
        floorObj["target"] = zones[ZONE_FLOOR].targetTemp;
        floorObj["hysteresis"] = zones[ZONE_FLOOR].hysteresis;
        floorObj["enabled"] = zones[ZONE_FLOOR].enabled;

        JsonObject airObj = zonesObj.createNestedObject("air");
        airObj["target"] = zones[ZONE_AIR].targetTemp;
        airObj["hysteresis"] = zones[ZONE_AIR].hysteresis;
        airObj["enabled"] = zones[ZONE_AIR].enabled;

        // Save water monitoring config
        JsonObject waterObj = doc.createNestedObject("water_monitoring");
        waterObj["enabled"] = water.enabled;
        waterObj["delta_t_warning_low"] = water.deltaTWarningLow;
        waterObj["delta_t_warning_high"] = water.deltaTWarningHigh;
        waterObj["smart_pump_control"] = water.smartPumpControl;

        // Save sensor config
        JsonObject sensorsObj = doc.createNestedObject("sensors");
        const char* sensorNames[] = {"floor", "air", "outdoor", "water_in", "water_out"};
        for (int i = 0; i < SENSOR_COUNT; i++) {
            sensorsObj[sensorNames[i]] = sensors.addresses[i];
        }

        JsonObject calObj = sensorsObj.createNestedObject("calibration");
        for (int i = 0; i < SENSOR_COUNT; i++) {
            calObj[sensorNames[i]] = sensors.calibration[i];
        }

        // Save system config
        JsonObject sysObj = doc.createNestedObject("system");
        sysObj["device_name"] = system.deviceName;
        sysObj["timezone"] = system.timezone;
        sysObj["temp_unit"] = system.useFahrenheit ? "F" : "C";
        sysObj["max_runtime"] = system.maxRuntime;
        sysObj["min_cycle_time"] = system.minCycleTime;

        // Save schedules
        JsonArray schedArray = doc.createNestedArray("schedules");
        for (int i = 0; i < MAX_SCHEDULES; i++) {
            if (schedules[i].enabled || schedules[i].days != 0) {
                JsonObject sched = schedArray.createNestedObject();
                sched["enabled"] = schedules[i].enabled;
                sched["zone"] = (schedules[i].zone == ZONE_FLOOR) ? "floor" : "air";
                sched["target_temp"] = schedules[i].targetTemp;

                // Days array
                JsonArray daysArray = sched.createNestedArray("days");
                for (int d = 0; d <= 6; d++) {
                    if (schedules[i].days & (1 << d)) {
                        daysArray.add(d);
                    }
                }

                // Times
                char timeStr[6];
                snprintf(timeStr, sizeof(timeStr), "%02d:%02d", schedules[i].startHour, schedules[i].startMinute);
                sched["start_time"] = timeStr;
                snprintf(timeStr, sizeof(timeStr), "%02d:%02d", schedules[i].endHour, schedules[i].endMinute);
                sched["end_time"] = timeStr;
            }
        }

        // Write to file
        File file = LittleFS.open(CONFIG_FILE, "w");
        if (!file) {
            Serial.println(F("Failed to open config file for writing"));
            return false;
        }

        if (serializeJson(doc, file) == 0) {
            Serial.println(F("Failed to write config"));
            file.close();
            return false;
        }

        file.close();
        Serial.println(F("Configuration saved successfully"));
        return true;
    }

    bool hasWifiCredentials() {
        return strlen(wifi.ssid) > 0;
    }

    // Temperature conversion helpers
    float toDisplayTemp(float celsius) {
        if (system.useFahrenheit) {
            return celsius * 9.0f / 5.0f + 32.0f;
        }
        return celsius;
    }

    float fromDisplayTemp(float display) {
        if (system.useFahrenheit) {
            return (display - 32.0f) * 5.0f / 9.0f;
        }
        return display;
    }

    const char* getTempUnit() {
        return system.useFahrenheit ? "F" : "C";
    }
};

#endif // STORAGE_H
