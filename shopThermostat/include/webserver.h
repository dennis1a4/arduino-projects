#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "storage.h"
#include "temperature.h"
#include "control.h"
#include "scheduler.h"
#include "wifi_manager.h"

// ============================================================================
// ASYNC WEB SERVER
// ============================================================================

class WebServerManager {
private:
    AsyncWebServer _server;
    ConfigManager* _config;
    TemperatureManager* _temps;
    ThermostatController* _controller;
    ScheduleManager* _scheduler;
    WiFiConnectionManager* _wifi;

public:
    WebServerManager(ConfigManager* config, TemperatureManager* temps,
                     ThermostatController* controller, ScheduleManager* scheduler,
                     WiFiConnectionManager* wifi)
        : _server(80),
          _config(config),
          _temps(temps),
          _controller(controller),
          _scheduler(scheduler),
          _wifi(wifi) {}

    void begin() {
        // Serve static files from LittleFS
        _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

        // API endpoints
        setupAPIEndpoints();

        // Start server
        _server.begin();
        Serial.println(F("Web server started"));
    }

    void setupAPIEndpoints() {
        // Get system status
        _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleGetStatus(request);
        });

        // Get temperatures
        _server.on("/api/temps", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleGetTemps(request);
        });

        // Get water system status
        _server.on("/api/water", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleGetWater(request);
        });

        // Get/set configuration
        _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleGetConfig(request);
        });

        // Set zone settings
        _server.on("/api/zone", HTTP_POST, [this](AsyncWebServerRequest *request) {
            // Body handled in onBody
        }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSetZone(request, data, len);
        });

        // Set override
        _server.on("/api/override", HTTP_POST, [this](AsyncWebServerRequest *request) {
            // Body handled in onBody
        }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSetOverride(request, data, len);
        });

        // Get schedules
        _server.on("/api/schedules", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleGetSchedules(request);
        });

        // Set schedule
        _server.on("/api/schedule", HTTP_POST, [this](AsyncWebServerRequest *request) {
            // Body handled in onBody
        }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSetSchedule(request, data, len);
        });

        // Delete schedule
        _server.on("/api/schedule/delete", HTTP_POST, [this](AsyncWebServerRequest *request) {
            // Body handled in onBody
        }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleDeleteSchedule(request, data, len);
        });

        // MQTT settings
        _server.on("/api/mqtt", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleGetMqtt(request);
        });

        _server.on("/api/mqtt", HTTP_POST, [this](AsyncWebServerRequest *request) {
            // Body handled in onBody
        }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSetMqtt(request, data, len);
        });

        // WiFi settings
        _server.on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleGetWifi(request);
        });

        _server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleWifiScan(request);
        });

        _server.on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
            // Body handled in onBody
        }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSetWifi(request, data, len);
        });

        // System settings
        _server.on("/api/system", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleGetSystem(request);
        });

        _server.on("/api/system", HTTP_POST, [this](AsyncWebServerRequest *request) {
            // Body handled in onBody
        }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSetSystem(request, data, len);
        });

        // Sensor discovery
        _server.on("/api/sensors/discover", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleSensorDiscover(request);
        });

        // Set sensor addresses
        _server.on("/api/sensors", HTTP_POST, [this](AsyncWebServerRequest *request) {
            // Body handled in onBody
        }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSetSensors(request, data, len);
        });

        // Reboot
        _server.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) {
            request->send(200, "application/json", "{\"status\":\"rebooting\"}");
            delay(500);
            ESP.restart();
        });

        // Reset thermal runaway
        _server.on("/api/reset/thermal", HTTP_POST, [this](AsyncWebServerRequest *request) {
            // Body handled in onBody
        }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleResetThermal(request, data, len);
        });

        // Save config
        _server.on("/api/save", HTTP_POST, [this](AsyncWebServerRequest *request) {
            if (_config->save()) {
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"save failed\"}");
            }
        });

        // Captive portal redirect
        _server.onNotFound([this](AsyncWebServerRequest *request) {
            if (_wifi->isAPMode()) {
                request->redirect("http://" + _wifi->getIPAddress() + "/");
            } else {
                request->send(404, "text/plain", "Not found");
            }
        });
    }

    void handleGetStatus(AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;

        const TemperatureManager::Readings& readings = _temps->getReadings();

        // Temperatures
        JsonObject temps = doc.createNestedObject("temperatures");
        temps["floor"] = readings.valid[SENSOR_FLOOR] ? readings.floor : (JsonVariant)nullptr;
        temps["air"] = readings.valid[SENSOR_AIR] ? readings.air : (JsonVariant)nullptr;
        temps["outdoor"] = readings.valid[SENSOR_OUTDOOR] ? readings.outdoor : (JsonVariant)nullptr;
        temps["water_in"] = readings.valid[SENSOR_WATER_IN] ? readings.waterIn : (JsonVariant)nullptr;
        temps["water_out"] = readings.valid[SENSOR_WATER_OUT] ? readings.waterOut : (JsonVariant)nullptr;
        temps["water_delta"] = (readings.valid[SENSOR_WATER_IN] && readings.valid[SENSOR_WATER_OUT]) ?
                               readings.waterDelta : (JsonVariant)nullptr;

        // Zones
        JsonObject zones = doc.createNestedObject("zones");

        JsonObject floor = zones.createNestedObject("floor");
        floor["target"] = _config->zones[ZONE_FLOOR].targetTemp;
        floor["hysteresis"] = _config->zones[ZONE_FLOOR].hysteresis;
        floor["enabled"] = _config->zones[ZONE_FLOOR].enabled;
        floor["relay"] = _controller->isRelayOn(ZONE_FLOOR);
        floor["override"] = (int)_config->zones[ZONE_FLOOR].override;
        floor["status"] = _controller->getZoneStatus(ZONE_FLOOR);

        JsonObject air = zones.createNestedObject("air");
        air["target"] = _config->zones[ZONE_AIR].targetTemp;
        air["hysteresis"] = _config->zones[ZONE_AIR].hysteresis;
        air["enabled"] = _config->zones[ZONE_AIR].enabled;
        air["relay"] = _controller->isRelayOn(ZONE_AIR);
        air["override"] = (int)_config->zones[ZONE_AIR].override;
        air["status"] = _controller->getZoneStatus(ZONE_AIR);

        // Water system
        JsonObject water = doc.createNestedObject("water");
        water["enabled"] = _config->water.enabled;
        water["flow_status"] = TemperatureManager::getFlowStatusString(_temps->getFlowStatus());
        water["pump_runtime"] = _controller->getRuntime(ZONE_FLOOR);

        // System
        JsonObject system = doc.createNestedObject("system");
        system["wifi"] = _wifi->isConnected();
        system["wifi_rssi"] = WiFi.RSSI();
        system["ip"] = _wifi->getIPAddress();
        system["uptime"] = _scheduler->getUptimeSeconds();
        system["time"] = _scheduler->getCurrentTimeString();
        system["schedule_active"] = _scheduler->isScheduleActive();
        system["temp_unit"] = _config->getTempUnit();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    void handleGetTemps(AsyncWebServerRequest *request) {
        StaticJsonDocument<512> doc;

        const TemperatureManager::Readings& readings = _temps->getReadings();

        doc["floor"] = readings.valid[SENSOR_FLOOR] ? readings.floor : (JsonVariant)nullptr;
        doc["air"] = readings.valid[SENSOR_AIR] ? readings.air : (JsonVariant)nullptr;
        doc["outdoor"] = readings.valid[SENSOR_OUTDOOR] ? readings.outdoor : (JsonVariant)nullptr;
        doc["water_inlet"] = readings.valid[SENSOR_WATER_IN] ? readings.waterIn : (JsonVariant)nullptr;
        doc["water_outlet"] = readings.valid[SENSOR_WATER_OUT] ? readings.waterOut : (JsonVariant)nullptr;
        doc["water_delta"] = (readings.valid[SENSOR_WATER_IN] && readings.valid[SENSOR_WATER_OUT]) ?
                             readings.waterDelta : (JsonVariant)nullptr;
        doc["timestamp"] = millis();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    void handleGetWater(AsyncWebServerRequest *request) {
        StaticJsonDocument<512> doc;

        const TemperatureManager::Readings& readings = _temps->getReadings();

        doc["inlet_temp"] = readings.valid[SENSOR_WATER_IN] ? readings.waterIn : (JsonVariant)nullptr;
        doc["outlet_temp"] = readings.valid[SENSOR_WATER_OUT] ? readings.waterOut : (JsonVariant)nullptr;
        doc["delta_t"] = (readings.valid[SENSOR_WATER_IN] && readings.valid[SENSOR_WATER_OUT]) ?
                         readings.waterDelta : (JsonVariant)nullptr;
        doc["flow_status"] = TemperatureManager::getFlowStatusString(_temps->getFlowStatus());
        doc["pump_runtime_today"] = _controller->getRuntime(ZONE_FLOOR);
        doc["enabled"] = _config->water.enabled;
        doc["smart_pump"] = _config->water.smartPumpControl;
        doc["timestamp"] = millis();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    void handleGetConfig(AsyncWebServerRequest *request) {
        StaticJsonDocument<1536> doc;

        // Zones
        JsonObject zones = doc.createNestedObject("zones");
        JsonObject floor = zones.createNestedObject("floor");
        floor["target"] = _config->zones[ZONE_FLOOR].targetTemp;
        floor["hysteresis"] = _config->zones[ZONE_FLOOR].hysteresis;
        floor["enabled"] = _config->zones[ZONE_FLOOR].enabled;

        JsonObject air = zones.createNestedObject("air");
        air["target"] = _config->zones[ZONE_AIR].targetTemp;
        air["hysteresis"] = _config->zones[ZONE_AIR].hysteresis;
        air["enabled"] = _config->zones[ZONE_AIR].enabled;

        // Water
        JsonObject water = doc.createNestedObject("water");
        water["enabled"] = _config->water.enabled;
        water["delta_t_warning_low"] = _config->water.deltaTWarningLow;
        water["delta_t_warning_high"] = _config->water.deltaTWarningHigh;
        water["smart_pump_control"] = _config->water.smartPumpControl;

        // System
        JsonObject system = doc.createNestedObject("system");
        system["device_name"] = _config->system.deviceName;
        system["timezone"] = _config->system.timezone;
        system["temp_unit"] = _config->system.useFahrenheit ? "F" : "C";
        system["max_runtime"] = _config->system.maxRuntime;
        system["min_cycle_time"] = _config->system.minCycleTime;

        // Sensors
        JsonObject sensors = doc.createNestedObject("sensors");
        const char* sensorNames[] = {"floor", "air", "outdoor", "water_in", "water_out"};
        for (int i = 0; i < SENSOR_COUNT; i++) {
            JsonObject sensor = sensors.createNestedObject(sensorNames[i]);
            sensor["address"] = _config->sensors.addresses[i];
            sensor["calibration"] = _config->sensors.calibration[i];
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    void handleSetZone(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        String zone = doc["zone"] | "";
        ZoneId zoneId = (zone == "floor") ? ZONE_FLOOR : ZONE_AIR;

        if (doc.containsKey("target")) {
            float target = doc["target"];
            if (zoneId == ZONE_FLOOR) {
                target = constrain(target, MIN_FLOOR_TARGET, MAX_FLOOR_TARGET);
            } else {
                target = constrain(target, MIN_AIR_TARGET, MAX_AIR_TARGET);
            }
            _config->zones[zoneId].targetTemp = target;
        }

        if (doc.containsKey("hysteresis")) {
            _config->zones[zoneId].hysteresis = doc["hysteresis"];
        }

        if (doc.containsKey("enabled")) {
            _config->zones[zoneId].enabled = doc["enabled"];
        }

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }

    void handleSetOverride(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        String zone = doc["zone"] | "";
        String mode = doc["mode"] | "auto";

        ZoneId zoneId = (zone == "floor") ? ZONE_FLOOR : ZONE_AIR;
        OverrideMode overrideMode = OVERRIDE_AUTO;

        if (mode == "on") overrideMode = OVERRIDE_ON;
        else if (mode == "off") overrideMode = OVERRIDE_OFF;

        _controller->setOverride(zoneId, overrideMode);

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }

    void handleGetSchedules(AsyncWebServerRequest *request) {
        StaticJsonDocument<2048> doc;
        JsonArray schedules = doc.createNestedArray("schedules");

        for (int i = 0; i < MAX_SCHEDULES; i++) {
            Schedule& sched = _config->schedules[i];

            JsonObject s = schedules.createNestedObject();
            s["index"] = i;
            s["enabled"] = sched.enabled;
            s["zone"] = (sched.zone == ZONE_FLOOR) ? "floor" : "air";
            s["target_temp"] = sched.targetTemp;

            // Days array
            JsonArray days = s.createNestedArray("days");
            for (int d = 0; d <= 6; d++) {
                if (sched.days & (1 << d)) {
                    days.add(d);
                }
            }

            // Times
            char buf[6];
            snprintf(buf, sizeof(buf), "%02d:%02d", sched.startHour, sched.startMinute);
            s["start_time"] = String(buf);
            snprintf(buf, sizeof(buf), "%02d:%02d", sched.endHour, sched.endMinute);
            s["end_time"] = String(buf);
        }

        doc["active_index"] = _scheduler->getActiveScheduleIndex();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    void handleSetSchedule(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        int index = doc["index"] | -1;
        if (index < 0 || index >= MAX_SCHEDULES) {
            request->send(400, "application/json", "{\"error\":\"invalid index\"}");
            return;
        }

        Schedule& sched = _config->schedules[index];

        sched.enabled = doc["enabled"] | false;
        sched.zone = (doc["zone"].as<String>() == "floor") ? ZONE_FLOOR : ZONE_AIR;
        sched.targetTemp = doc["target_temp"] | DEFAULT_AIR_TARGET;

        // Parse days
        sched.days = 0;
        if (doc.containsKey("days")) {
            JsonArray days = doc["days"];
            for (int d : days) {
                if (d >= 0 && d <= 6) {
                    sched.days |= (1 << d);
                }
            }
        }

        // Parse times
        String startTime = doc["start_time"] | "08:00";
        String endTime = doc["end_time"] | "17:00";

        sched.startHour = startTime.substring(0, 2).toInt();
        sched.startMinute = startTime.substring(3, 5).toInt();
        sched.endHour = endTime.substring(0, 2).toInt();
        sched.endMinute = endTime.substring(3, 5).toInt();

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }

    void handleDeleteSchedule(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
        StaticJsonDocument<64> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        int index = doc["index"] | -1;
        if (index < 0 || index >= MAX_SCHEDULES) {
            request->send(400, "application/json", "{\"error\":\"invalid index\"}");
            return;
        }

        // Clear the schedule
        _config->schedules[index].enabled = false;
        _config->schedules[index].days = 0;

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }

    void handleGetMqtt(AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;

        doc["enabled"] = _config->mqtt.enabled;
        doc["broker"] = _config->mqtt.broker;
        doc["port"] = _config->mqtt.port;
        doc["username"] = _config->mqtt.username;
        doc["base_topic"] = _config->mqtt.baseTopic;
        // Don't send password

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    void handleSetMqtt(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        _config->mqtt.enabled = doc["enabled"] | false;
        strlcpy(_config->mqtt.broker, doc["broker"] | "", sizeof(_config->mqtt.broker));
        _config->mqtt.port = doc["port"] | DEFAULT_MQTT_PORT;
        strlcpy(_config->mqtt.username, doc["username"] | "", sizeof(_config->mqtt.username));
        if (doc.containsKey("password") && strlen(doc["password"]) > 0) {
            strlcpy(_config->mqtt.password, doc["password"], sizeof(_config->mqtt.password));
        }
        strlcpy(_config->mqtt.baseTopic, doc["base_topic"] | DEFAULT_MQTT_BASE_TOPIC, sizeof(_config->mqtt.baseTopic));

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }

    void handleGetWifi(AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;

        doc["ssid"] = _wifi->getSSID();
        doc["connected"] = _wifi->isConnected();
        doc["ip"] = _wifi->getIPAddress();
        doc["rssi"] = WiFi.RSSI();
        doc["mac"] = _wifi->getMACAddress();
        doc["ap_mode"] = _wifi->isAPMode();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    void handleWifiScan(AsyncWebServerRequest *request) {
        const int maxNetworks = 10;
        String ssids[maxNetworks];
        int rssis[maxNetworks];

        int count = _wifi->scanNetworks(ssids, rssis, maxNetworks);

        StaticJsonDocument<1024> doc;
        JsonArray networks = doc.createNestedArray("networks");

        for (int i = 0; i < count; i++) {
            JsonObject net = networks.createNestedObject();
            net["ssid"] = ssids[i];
            net["rssi"] = rssis[i];
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    void handleSetWifi(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        const char* ssid = doc["ssid"] | "";
        const char* password = doc["password"] | "";

        if (strlen(ssid) == 0) {
            request->send(400, "application/json", "{\"error\":\"ssid required\"}");
            return;
        }

        // Save and try to connect
        strlcpy(_config->wifi.ssid, ssid, sizeof(_config->wifi.ssid));
        strlcpy(_config->wifi.password, password, sizeof(_config->wifi.password));
        _config->save();

        request->send(200, "application/json", "{\"status\":\"connecting\"}");

        // Connect after sending response
        delay(100);
        _wifi->connectToNetwork(ssid, password);
    }

    void handleGetSystem(AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;

        doc["device_name"] = _config->system.deviceName;
        doc["timezone"] = _config->system.timezone;
        doc["temp_unit"] = _config->system.useFahrenheit ? "F" : "C";
        doc["max_runtime"] = _config->system.maxRuntime;
        doc["min_cycle_time"] = _config->system.minCycleTime;
        doc["firmware"] = FIRMWARE_VERSION;
        doc["chip_id"] = String(ESP.getChipId(), HEX);
        doc["free_heap"] = ESP.getFreeHeap();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    void handleSetSystem(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        if (doc.containsKey("device_name")) {
            strlcpy(_config->system.deviceName, doc["device_name"], sizeof(_config->system.deviceName));
        }
        if (doc.containsKey("timezone")) {
            strlcpy(_config->system.timezone, doc["timezone"], sizeof(_config->system.timezone));
        }
        if (doc.containsKey("temp_unit")) {
            _config->system.useFahrenheit = (doc["temp_unit"].as<String>() == "F");
        }
        if (doc.containsKey("max_runtime")) {
            _config->system.maxRuntime = doc["max_runtime"];
        }
        if (doc.containsKey("min_cycle_time")) {
            _config->system.minCycleTime = doc["min_cycle_time"];
        }

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }

    void handleSensorDiscover(AsyncWebServerRequest *request) {
        StaticJsonDocument<512> doc;
        JsonArray sensors = doc.createNestedArray("sensors");

        int count = _temps->getDeviceCount();
        doc["count"] = count;

        // Note: This would need to be enhanced to actually list discovered addresses
        // For now, return configured addresses

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    void handleSetSensors(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        const char* sensorNames[] = {"floor", "air", "outdoor", "water_in", "water_out"};
        for (int i = 0; i < SENSOR_COUNT; i++) {
            if (doc.containsKey(sensorNames[i])) {
                JsonObject sensor = doc[sensorNames[i]];
                if (sensor.containsKey("address")) {
                    strlcpy(_config->sensors.addresses[i], sensor["address"], 17);
                }
                if (sensor.containsKey("calibration")) {
                    _config->sensors.calibration[i] = sensor["calibration"];
                    _temps->setCalibration(i, _config->sensors.calibration[i]);
                }
            }
        }

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }

    void handleResetThermal(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
        StaticJsonDocument<64> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        String zone = doc["zone"] | "";
        ZoneId zoneId = (zone == "floor") ? ZONE_FLOOR : ZONE_AIR;

        _controller->resetThermalRunaway(zoneId);

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }
};

#endif // WEBSERVER_H
