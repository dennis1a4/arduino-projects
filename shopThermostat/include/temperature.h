#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

// ============================================================================
// TEMPERATURE SENSOR MANAGER
// ============================================================================

class TemperatureManager {
public:
    struct Readings {
        float floor;
        float air;
        float outdoor;
        float waterIn;
        float waterOut;
        float waterDelta;
        bool valid[SENSOR_COUNT];
        unsigned long timestamp;
    };

    struct SensorAddresses {
        DeviceAddress floor;
        DeviceAddress air;
        DeviceAddress outdoor;
        DeviceAddress waterIn;
        DeviceAddress waterOut;
    };

private:
    OneWire* _oneWire;
    DallasTemperature* _sensors;
    SensorAddresses _addresses;
    float _calibration[SENSOR_COUNT];
    Readings _readings;
    bool _sensorsFound;
    int _deviceCount;
    unsigned long _lastReadTime;
    unsigned long _sensorFaultTime[SENSOR_COUNT];
    bool _sensorFault[SENSOR_COUNT];

    bool isValidReading(float temp) {
        return temp > TEMP_MIN_VALID && temp < TEMP_MAX_VALID && temp != TEMP_ERROR_VALUE;
    }

public:
    TemperatureManager() : _oneWire(nullptr), _sensors(nullptr) {
        _sensorsFound = false;
        _deviceCount = 0;
        _lastReadTime = 0;

        for (int i = 0; i < SENSOR_COUNT; i++) {
            _calibration[i] = 0.0f;
            _readings.valid[i] = false;
            _sensorFaultTime[i] = 0;
            _sensorFault[i] = false;
        }

        _readings.floor = 0;
        _readings.air = 0;
        _readings.outdoor = 0;
        _readings.waterIn = 0;
        _readings.waterOut = 0;
        _readings.waterDelta = 0;
        _readings.timestamp = 0;
    }

    void begin() {
        Serial.println(F("Creating OneWire..."));
        yield();

        // Create instances (deferred to avoid global construction issues)
        _oneWire = new OneWire(PIN_ONEWIRE);

        Serial.println(F("Creating DallasTemperature..."));
        yield();

        _sensors = new DallasTemperature(_oneWire);

        Serial.println(F("Calling sensors begin..."));
        yield();

        // Set non-blocking mode BEFORE begin to avoid hangs
        _sensors->setWaitForConversion(false);

        yield();
        _sensors->begin();
        yield();

        Serial.println(F("Getting device count..."));
        _deviceCount = _sensors->getDeviceCount();
        _sensorsFound = (_deviceCount > 0);

        yield();

        // Set resolution to 12 bits for all sensors (only if sensors found)
        if (_sensorsFound) {
            _sensors->setResolution(12);
            yield();
        }

        Serial.print(F("Found "));
        Serial.print(_deviceCount);
        Serial.println(F(" temperature sensors"));
    }

    void setSensorAddresses(const SensorAddresses& addresses) {
        memcpy(&_addresses, &addresses, sizeof(SensorAddresses));
    }

    void setCalibration(int sensorIndex, float offset) {
        if (sensorIndex >= 0 && sensorIndex < SENSOR_COUNT) {
            _calibration[sensorIndex] = offset;
        }
    }

    float getCalibration(int sensorIndex) {
        if (sensorIndex >= 0 && sensorIndex < SENSOR_COUNT) {
            return _calibration[sensorIndex];
        }
        return 0.0f;
    }

    void requestTemperatures() {
        _sensors->requestTemperatures();
    }

    void update() {
        _readings.timestamp = millis();

        // Read each sensor
        float rawFloor = _sensors->getTempC(_addresses.floor);
        float rawAir = _sensors->getTempC(_addresses.air);
        float rawOutdoor = _sensors->getTempC(_addresses.outdoor);
        float rawWaterIn = _sensors->getTempC(_addresses.waterIn);
        float rawWaterOut = _sensors->getTempC(_addresses.waterOut);

        // Validate and apply calibration
        _readings.valid[SENSOR_FLOOR] = isValidReading(rawFloor);
        _readings.valid[SENSOR_AIR] = isValidReading(rawAir);
        _readings.valid[SENSOR_OUTDOOR] = isValidReading(rawOutdoor);
        _readings.valid[SENSOR_WATER_IN] = isValidReading(rawWaterIn);
        _readings.valid[SENSOR_WATER_OUT] = isValidReading(rawWaterOut);

        if (_readings.valid[SENSOR_FLOOR]) {
            _readings.floor = rawFloor + _calibration[SENSOR_FLOOR];
            _sensorFaultTime[SENSOR_FLOOR] = 0;
            _sensorFault[SENSOR_FLOOR] = false;
        } else {
            handleSensorFault(SENSOR_FLOOR);
        }

        if (_readings.valid[SENSOR_AIR]) {
            _readings.air = rawAir + _calibration[SENSOR_AIR];
            _sensorFaultTime[SENSOR_AIR] = 0;
            _sensorFault[SENSOR_AIR] = false;
        } else {
            handleSensorFault(SENSOR_AIR);
        }

        if (_readings.valid[SENSOR_OUTDOOR]) {
            _readings.outdoor = rawOutdoor + _calibration[SENSOR_OUTDOOR];
            _sensorFaultTime[SENSOR_OUTDOOR] = 0;
            _sensorFault[SENSOR_OUTDOOR] = false;
        } else {
            handleSensorFault(SENSOR_OUTDOOR);
        }

        if (_readings.valid[SENSOR_WATER_IN]) {
            _readings.waterIn = rawWaterIn + _calibration[SENSOR_WATER_IN];
            _sensorFaultTime[SENSOR_WATER_IN] = 0;
            _sensorFault[SENSOR_WATER_IN] = false;
        } else {
            handleSensorFault(SENSOR_WATER_IN);
        }

        if (_readings.valid[SENSOR_WATER_OUT]) {
            _readings.waterOut = rawWaterOut + _calibration[SENSOR_WATER_OUT];
            _sensorFaultTime[SENSOR_WATER_OUT] = 0;
            _sensorFault[SENSOR_WATER_OUT] = false;
        } else {
            handleSensorFault(SENSOR_WATER_OUT);
        }

        // Calculate delta-T if both water sensors valid
        if (_readings.valid[SENSOR_WATER_IN] && _readings.valid[SENSOR_WATER_OUT]) {
            _readings.waterDelta = _readings.waterOut - _readings.waterIn;
        } else {
            _readings.waterDelta = 0;
        }

        _lastReadTime = millis();
    }

    void handleSensorFault(int sensorIndex) {
        if (_sensorFaultTime[sensorIndex] == 0) {
            _sensorFaultTime[sensorIndex] = millis();
        } else if (millis() - _sensorFaultTime[sensorIndex] > SENSOR_FAULT_TIMEOUT_MS) {
            _sensorFault[sensorIndex] = true;
        }
    }

    const Readings& getReadings() const {
        return _readings;
    }

    bool isSensorFault(int sensorIndex) const {
        if (sensorIndex >= 0 && sensorIndex < SENSOR_COUNT) {
            return _sensorFault[sensorIndex];
        }
        return true;
    }

    bool hasCriticalFault() const {
        // Floor and air sensors are critical
        return _sensorFault[SENSOR_FLOOR] || _sensorFault[SENSOR_AIR];
    }

    int getDeviceCount() const {
        return _deviceCount;
    }

    bool sensorsFound() const {
        return _sensorsFound;
    }

    // Discovery function to find and print all sensor addresses
    void discoverSensors() {
        Serial.println(F("=== DS18B20 Sensor Discovery ==="));
        Serial.print(F("Devices found: "));
        Serial.println(_deviceCount);
        Serial.println();

        DeviceAddress tempAddress;
        for (int i = 0; i < _deviceCount; i++) {
            if (_sensors->getAddress(tempAddress, i)) {
                Serial.print(F("Sensor "));
                Serial.print(i);
                Serial.print(F(" Address: "));
                printAddress(tempAddress);

                _sensors->requestTemperaturesByAddress(tempAddress);
                delay(750); // Wait for conversion
                float tempC = _sensors->getTempC(tempAddress);

                Serial.print(F(" | Temp: "));
                Serial.print(tempC);
                Serial.println(F("C"));
            }
        }
        Serial.println();
    }

    void printAddress(DeviceAddress deviceAddress) {
        Serial.print(F("{ "));
        for (uint8_t i = 0; i < 8; i++) {
            Serial.print(F("0x"));
            if (deviceAddress[i] < 16) Serial.print(F("0"));
            Serial.print(deviceAddress[i], HEX);
            if (i < 7) Serial.print(F(", "));
        }
        Serial.print(F(" }"));
    }

    // Get address string for configuration
    String addressToString(DeviceAddress deviceAddress) {
        String result = "";
        for (uint8_t i = 0; i < 8; i++) {
            if (deviceAddress[i] < 16) result += "0";
            result += String(deviceAddress[i], HEX);
        }
        result.toUpperCase();
        return result;
    }

    // Parse address from string
    bool stringToAddress(const String& str, DeviceAddress address) {
        if (str.length() != 16) return false;

        for (int i = 0; i < 8; i++) {
            String byteStr = str.substring(i * 2, i * 2 + 2);
            address[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
        }
        return true;
    }

    // Get sensor name by index
    static const char* getSensorName(int index) {
        switch (index) {
            case SENSOR_FLOOR: return "Floor";
            case SENSOR_AIR: return "Air";
            case SENSOR_OUTDOOR: return "Outdoor";
            case SENSOR_WATER_IN: return "Water In";
            case SENSOR_WATER_OUT: return "Water Out";
            default: return "Unknown";
        }
    }

    // Get water flow status
    FlowStatus getFlowStatus() const {
        if (!_readings.valid[SENSOR_WATER_IN] || !_readings.valid[SENSOR_WATER_OUT]) {
            return FLOW_ERROR;
        }

        float delta = _readings.waterDelta;

        if (delta < -0.5f) {
            return FLOW_ERROR;  // Possible flow reversal
        } else if (delta < DELTA_T_CRITICAL) {
            return FLOW_CRITICAL;
        } else if (delta < DEFAULT_DELTA_T_WARNING_LOW) {
            return FLOW_WARNING;
        } else if (delta > DEFAULT_DELTA_T_WARNING_HIGH) {
            return FLOW_ERROR;  // Likely sensor error
        }

        return FLOW_OK;
    }

    static const char* getFlowStatusString(FlowStatus status) {
        switch (status) {
            case FLOW_OK: return "OK";
            case FLOW_WARNING: return "WARNING";
            case FLOW_CRITICAL: return "CRITICAL";
            case FLOW_ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

#endif // TEMPERATURE_H
