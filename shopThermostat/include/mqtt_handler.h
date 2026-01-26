#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "storage.h"
#include "temperature.h"
#include "control.h"

// ============================================================================
// MQTT HANDLER WITH HOME ASSISTANT DISCOVERY
// ============================================================================

class MQTTHandler {
private:
    WiFiClient _wifiClient;
    PubSubClient _mqtt;
    ConfigManager* _config;
    TemperatureManager* _temps;
    ThermostatController* _controller;

    bool _connected;
    unsigned long _lastConnectAttempt;
    unsigned long _reconnectDelay;
    unsigned long _lastPublish;
    bool _discoveryPublished;
    String _deviceId;

    // Topic buffers
    String _baseTopic;

    static MQTTHandler* _instance;

public:
    MQTTHandler(ConfigManager* config, TemperatureManager* temps, ThermostatController* controller)
        : _mqtt(_wifiClient),
          _config(config),
          _temps(temps),
          _controller(controller),
          _connected(false),
          _lastConnectAttempt(0),
          _reconnectDelay(5000),
          _lastPublish(0),
          _discoveryPublished(false) {

        _deviceId = String(ESP.getChipId(), HEX);
        _deviceId.toLowerCase();
        _instance = this;
    }

    void begin() {
        if (!_config->mqtt.enabled) return;

        _baseTopic = String(_config->mqtt.baseTopic);
        _mqtt.setServer(_config->mqtt.broker, _config->mqtt.port);
        _mqtt.setCallback(mqttCallback);
        _mqtt.setKeepAlive(MQTT_KEEPALIVE);

        Serial.println(F("MQTT initialized"));
    }

    void update() {
        if (!_config->mqtt.enabled) return;

        if (!_mqtt.connected()) {
            _connected = false;

            if (millis() - _lastConnectAttempt > _reconnectDelay) {
                _lastConnectAttempt = millis();
                connect();
            }
        } else {
            _connected = true;
            _mqtt.loop();

            // Publish discovery if not done
            if (!_discoveryPublished) {
                publishDiscovery();
                _discoveryPublished = true;
            }
        }
    }

    bool connect() {
        if (!WiFi.isConnected()) return false;

        Serial.print(F("Connecting to MQTT broker: "));
        Serial.println(_config->mqtt.broker);

        String clientId = "ShopThermo-" + _deviceId;
        String willTopic = _baseTopic + "/status";

        bool result;
        if (strlen(_config->mqtt.username) > 0) {
            result = _mqtt.connect(
                clientId.c_str(),
                _config->mqtt.username,
                _config->mqtt.password,
                willTopic.c_str(),
                1,
                true,
                "offline"
            );
        } else {
            result = _mqtt.connect(
                clientId.c_str(),
                willTopic.c_str(),
                1,
                true,
                "offline"
            );
        }

        if (result) {
            Serial.println(F("MQTT connected!"));
            _connected = true;
            _reconnectDelay = 5000;
            _discoveryPublished = false;

            // Publish online status
            _mqtt.publish(willTopic.c_str(), "online", true);

            // Subscribe to command topics
            subscribeToCommands();

            return true;
        } else {
            Serial.print(F("MQTT connection failed, rc="));
            Serial.println(_mqtt.state());

            // Exponential backoff
            _reconnectDelay = min(_reconnectDelay * 2, (unsigned long)MQTT_RECONNECT_DELAY_MAX);

            return false;
        }
    }

    void subscribeToCommands() {
        _mqtt.subscribe((_baseTopic + "/floor/target/set").c_str());
        _mqtt.subscribe((_baseTopic + "/air/target/set").c_str());
        _mqtt.subscribe((_baseTopic + "/floor/mode/set").c_str());
        _mqtt.subscribe((_baseTopic + "/air/mode/set").c_str());
        _mqtt.subscribe((_baseTopic + "/command").c_str());
    }

    static void mqttCallback(char* topic, byte* payload, unsigned int length) {
        if (_instance) {
            _instance->handleMessage(topic, payload, length);
        }
    }

    void handleMessage(char* topic, byte* payload, unsigned int length) {
        String topicStr = String(topic);
        String payloadStr;
        for (unsigned int i = 0; i < length; i++) {
            payloadStr += (char)payload[i];
        }

        Serial.print(F("MQTT received: "));
        Serial.print(topicStr);
        Serial.print(F(" = "));
        Serial.println(payloadStr);

        // Handle floor target
        if (topicStr.endsWith("/floor/target/set")) {
            float temp = payloadStr.toFloat();
            temp = constrain(temp, MIN_FLOOR_TARGET, MAX_FLOOR_TARGET);
            _config->zones[ZONE_FLOOR].targetTemp = temp;
            publishState();
        }
        // Handle air target
        else if (topicStr.endsWith("/air/target/set")) {
            float temp = payloadStr.toFloat();
            temp = constrain(temp, MIN_AIR_TARGET, MAX_AIR_TARGET);
            _config->zones[ZONE_AIR].targetTemp = temp;
            publishState();
        }
        // Handle floor mode
        else if (topicStr.endsWith("/floor/mode/set")) {
            if (payloadStr == "heat") {
                _config->zones[ZONE_FLOOR].enabled = true;
                _controller->setOverride(ZONE_FLOOR, OVERRIDE_AUTO);
            } else if (payloadStr == "off") {
                _controller->setOverride(ZONE_FLOOR, OVERRIDE_OFF);
            }
            publishState();
        }
        // Handle air mode
        else if (topicStr.endsWith("/air/mode/set")) {
            if (payloadStr == "heat") {
                _config->zones[ZONE_AIR].enabled = true;
                _controller->setOverride(ZONE_AIR, OVERRIDE_AUTO);
            } else if (payloadStr == "off") {
                _controller->setOverride(ZONE_AIR, OVERRIDE_OFF);
            }
            publishState();
        }
        // Handle system commands
        else if (topicStr.endsWith("/command")) {
            if (payloadStr == "reboot") {
                Serial.println(F("Reboot command received"));
                ESP.restart();
            }
        }
    }

    void publishDiscovery() {
        if (!_connected) return;

        Serial.println(F("Publishing MQTT discovery..."));

        // Floor climate entity
        publishClimateDiscovery(ZONE_FLOOR);

        // Air climate entity
        publishClimateDiscovery(ZONE_AIR);

        // Temperature sensors
        publishSensorDiscovery("floor", "Floor Temperature", "temperature", "°C");
        publishSensorDiscovery("air", "Air Temperature", "temperature", "°C");
        publishSensorDiscovery("outdoor", "Outdoor Temperature", "temperature", "°C");
        publishSensorDiscovery("water_inlet", "Water Tank Inlet", "temperature", "°C");
        publishSensorDiscovery("water_outlet", "Water Tank Outlet", "temperature", "°C");
        publishSensorDiscovery("water_delta", "Water Tank Delta-T", "temperature", "°C");

        // Status sensors
        publishSensorDiscovery("wifi_rssi", "WiFi Signal", "signal_strength", "dBm");

        // Binary sensors for relays
        publishBinarySensorDiscovery("floor_relay", "Floor Pump", "running");
        publishBinarySensorDiscovery("air_relay", "Electric Heater", "running");

        Serial.println(F("MQTT discovery published"));
    }

    void publishClimateDiscovery(ZoneId zone) {
        StaticJsonDocument<1024> doc;

        String zoneName = (zone == ZONE_FLOOR) ? "floor" : "air";
        String friendlyName = (zone == ZONE_FLOOR) ? "Shop Floor Heating" : "Shop Air Heating";
        String uniqueId = "shop_thermo_" + zoneName + "_" + _deviceId;

        doc["name"] = friendlyName;
        doc["unique_id"] = uniqueId;
        doc["mode_cmd_t"] = _baseTopic + "/" + zoneName + "/mode/set";
        doc["mode_stat_t"] = _baseTopic + "/" + zoneName + "/mode";
        doc["temp_cmd_t"] = _baseTopic + "/" + zoneName + "/target/set";
        doc["temp_stat_t"] = _baseTopic + "/" + zoneName + "/target";
        doc["curr_temp_t"] = _baseTopic + "/" + zoneName + "/current";

        JsonArray modes = doc.createNestedArray("modes");
        modes.add("off");
        modes.add("heat");

        doc["min_temp"] = (zone == ZONE_FLOOR) ? MIN_FLOOR_TARGET : MIN_AIR_TARGET;
        doc["max_temp"] = (zone == ZONE_FLOOR) ? MAX_FLOOR_TARGET : MAX_AIR_TARGET;
        doc["temp_step"] = 0.5;
        doc["temperature_unit"] = "C";

        // Device info
        JsonObject device = doc.createNestedObject("device");
        JsonArray identifiers = device.createNestedArray("identifiers");
        identifiers.add("shop_thermostat_" + _deviceId);
        device["name"] = _config->system.deviceName;
        device["model"] = DEVICE_MODEL;
        device["manufacturer"] = DEVICE_MANUFACTURER;
        device["sw_version"] = FIRMWARE_VERSION;

        // Availability
        doc["availability_topic"] = _baseTopic + "/status";
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";

        String topic = "homeassistant/climate/shop_thermostat_" + zoneName + "/config";
        String payload;
        serializeJson(doc, payload);

        _mqtt.publish(topic.c_str(), payload.c_str(), true);
    }

    void publishSensorDiscovery(const char* sensor, const char* name, const char* deviceClass, const char* unit) {
        StaticJsonDocument<768> doc;

        String uniqueId = String("shop_thermo_") + sensor + "_" + _deviceId;
        String stateTopic;

        // Build topic based on sensor name
        if (strcmp(sensor, "water_inlet") == 0) {
            stateTopic = _baseTopic + "/water/inlet";
        } else if (strcmp(sensor, "water_outlet") == 0) {
            stateTopic = _baseTopic + "/water/outlet";
        } else if (strcmp(sensor, "water_delta") == 0) {
            stateTopic = _baseTopic + "/water/delta";
        } else if (strcmp(sensor, "wifi_rssi") == 0) {
            stateTopic = _baseTopic + "/wifi/rssi";
        } else {
            stateTopic = _baseTopic + "/" + String(sensor) + "/current";
        }

        doc["name"] = String("Shop ") + name;
        doc["unique_id"] = uniqueId;
        doc["state_topic"] = stateTopic;
        doc["device_class"] = deviceClass;
        doc["unit_of_measurement"] = unit;

        // Device info
        JsonObject device = doc.createNestedObject("device");
        JsonArray identifiers = device.createNestedArray("identifiers");
        identifiers.add("shop_thermostat_" + _deviceId);
        device["name"] = _config->system.deviceName;

        // Availability
        doc["availability_topic"] = _baseTopic + "/status";

        String topic = "homeassistant/sensor/shop_thermostat_" + String(sensor) + "/config";
        String payload;
        serializeJson(doc, payload);

        _mqtt.publish(topic.c_str(), payload.c_str(), true);
    }

    void publishBinarySensorDiscovery(const char* sensor, const char* name, const char* deviceClass) {
        StaticJsonDocument<512> doc;

        String uniqueId = String("shop_thermo_") + sensor + "_" + _deviceId;
        // Build topic: floor_relay -> floor/relay, air_relay -> air/relay
        String sensorStr = String(sensor);
        sensorStr.replace("_relay", "/relay");
        String stateTopic = _baseTopic + "/" + sensorStr;

        doc["name"] = String("Shop ") + name;
        doc["unique_id"] = uniqueId;
        doc["state_topic"] = stateTopic;
        doc["device_class"] = deviceClass;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";

        // Device info
        JsonObject device = doc.createNestedObject("device");
        JsonArray identifiers = device.createNestedArray("identifiers");
        identifiers.add("shop_thermostat_" + _deviceId);
        device["name"] = _config->system.deviceName;

        // Availability
        doc["availability_topic"] = _baseTopic + "/status";

        String topic = "homeassistant/binary_sensor/shop_thermostat_" + String(sensor) + "/config";
        String payload;
        serializeJson(doc, payload);

        _mqtt.publish(topic.c_str(), payload.c_str(), true);
    }

    void publishState() {
        if (!_connected) return;

        const TemperatureManager::Readings& readings = _temps->getReadings();

        // Publish temperatures
        if (readings.valid[SENSOR_FLOOR]) {
            publish("floor/current", String(readings.floor, 1).c_str());
        }
        if (readings.valid[SENSOR_AIR]) {
            publish("air/current", String(readings.air, 1).c_str());
        }
        if (readings.valid[SENSOR_OUTDOOR]) {
            publish("outdoor/current", String(readings.outdoor, 1).c_str());
        }
        if (readings.valid[SENSOR_WATER_IN]) {
            publish("water/inlet", String(readings.waterIn, 1).c_str());
        }
        if (readings.valid[SENSOR_WATER_OUT]) {
            publish("water/outlet", String(readings.waterOut, 1).c_str());
        }
        if (readings.valid[SENSOR_WATER_IN] && readings.valid[SENSOR_WATER_OUT]) {
            publish("water/delta", String(readings.waterDelta, 1).c_str());
            publish("water/flow_status", TemperatureManager::getFlowStatusString(_temps->getFlowStatus()));
        }

        // Publish targets
        publish("floor/target", String(_config->zones[ZONE_FLOOR].targetTemp, 1).c_str(), true);
        publish("air/target", String(_config->zones[ZONE_AIR].targetTemp, 1).c_str(), true);

        // Publish modes
        String floorMode = (_config->zones[ZONE_FLOOR].enabled &&
                           _config->zones[ZONE_FLOOR].override != OVERRIDE_OFF) ? "heat" : "off";
        String airMode = (_config->zones[ZONE_AIR].enabled &&
                         _config->zones[ZONE_AIR].override != OVERRIDE_OFF) ? "heat" : "off";
        publish("floor/mode", floorMode.c_str(), true);
        publish("air/mode", airMode.c_str(), true);

        // Publish relay states
        publish("floor/relay", _controller->isRelayOn(ZONE_FLOOR) ? "ON" : "OFF");
        publish("air/relay", _controller->isRelayOn(ZONE_AIR) ? "ON" : "OFF");

        // Publish WiFi RSSI
        publish("wifi/rssi", String(WiFi.RSSI()).c_str());

        _lastPublish = millis();
    }

    void publish(const char* subtopic, const char* payload, bool retained = false) {
        String topic = _baseTopic + "/" + subtopic;
        _mqtt.publish(topic.c_str(), payload, retained);
    }

    bool isConnected() const {
        return _connected;
    }

    bool isEnabled() const {
        return _config->mqtt.enabled;
    }

    unsigned long getLastPublishTime() const {
        return _lastPublish;
    }

    bool shouldPublish() const {
        return _connected && (millis() - _lastPublish > MQTT_PUBLISH_INTERVAL);
    }
};

MQTTHandler* MQTTHandler::_instance = nullptr;

#endif // MQTT_HANDLER_H
