#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include "config.h"
#include "storage.h"

// ============================================================================
// WIFI CONNECTION MANAGER
// ============================================================================

class WiFiConnectionManager {
public:
    enum ConnectionState {
        WIFI_STATE_DISCONNECTED,
        WIFI_STATE_CONNECTING,
        WIFI_STATE_CONNECTED,
        WIFI_STATE_AP_MODE
    };

private:
    ConfigManager* _config;
    ConnectionState _state;
    DNSServer _dnsServer;
    bool _apMode;
    unsigned long _apStartTime;
    unsigned long _lastConnectAttempt;
    unsigned long _connectTimeout;
    String _ipAddress;
    int _rssi;
    String _apSSID;

public:
    WiFiConnectionManager(ConfigManager* config)
        : _config(config),
          _state(WIFI_STATE_DISCONNECTED),
          _apMode(false),
          _apStartTime(0),
          _lastConnectAttempt(0),
          _connectTimeout(60000),
          _rssi(0) {
        // Generate AP SSID with chip ID
        _apSSID = "ShopThermostat-" + String(ESP.getChipId(), HEX);
        _apSSID.toUpperCase();
    }

    void begin() {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);

        if (_config->hasWifiCredentials()) {
            _state = WIFI_STATE_CONNECTING;
            _lastConnectAttempt = millis();
            WiFi.begin(_config->wifi.ssid, _config->wifi.password);
            Serial.print(F("Connecting to WiFi: "));
            Serial.println(_config->wifi.ssid);
        } else {
            Serial.println(F("No WiFi credentials, starting AP mode"));
            startAPMode();
        }
    }

    void update() {
        if (_apMode) {
            _dnsServer.processNextRequest();

            // Check AP timeout
            if (millis() - _apStartTime > AP_TIMEOUT_MS) {
                Serial.println(F("AP mode timeout"));
                // Try to reconnect if we have credentials
                if (_config->hasWifiCredentials()) {
                    stopAPMode();
                    begin();
                }
            }
        } else {
            switch (_state) {
                case WIFI_STATE_CONNECTING:
                    if (WiFi.status() == WL_CONNECTED) {
                        _state = WIFI_STATE_CONNECTED;
                        _ipAddress = WiFi.localIP().toString();
                        _rssi = WiFi.RSSI();
                        Serial.print(F("WiFi connected! IP: "));
                        Serial.println(_ipAddress);
                    } else if (millis() - _lastConnectAttempt > _connectTimeout) {
                        Serial.println(F("WiFi connection timeout, starting AP mode"));
                        startAPMode();
                    }
                    break;

                case WIFI_STATE_CONNECTED:
                    if (WiFi.status() != WL_CONNECTED) {
                        _state = WIFI_STATE_DISCONNECTED;
                        _ipAddress = "";
                        Serial.println(F("WiFi disconnected"));
                    } else {
                        _rssi = WiFi.RSSI();
                    }
                    break;

                case WIFI_STATE_DISCONNECTED:
                    // Try to reconnect periodically
                    if (millis() - _lastConnectAttempt > WIFI_RECONNECT_INTERVAL) {
                        _lastConnectAttempt = millis();
                        if (_config->hasWifiCredentials()) {
                            _state = WIFI_STATE_CONNECTING;
                            WiFi.begin(_config->wifi.ssid, _config->wifi.password);
                            Serial.println(F("Attempting WiFi reconnection..."));
                        }
                    }
                    break;

                default:
                    break;
            }
        }
    }

    void startAPMode() {
        Serial.println(F("Starting AP mode..."));

        WiFi.disconnect();
        delay(100);
        WiFi.mode(WIFI_AP);

        // Start access point
        WiFi.softAP(_apSSID.c_str(), DEFAULT_AP_PASSWORD);

        // Start DNS server for captive portal
        _dnsServer.start(53, "*", WiFi.softAPIP());

        _apMode = true;
        _state = WIFI_STATE_AP_MODE;
        _apStartTime = millis();
        _ipAddress = WiFi.softAPIP().toString();

        Serial.print(F("AP started: "));
        Serial.println(_apSSID);
        Serial.print(F("IP: "));
        Serial.println(_ipAddress);
    }

    void stopAPMode() {
        Serial.println(F("Stopping AP mode..."));

        _dnsServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);

        _apMode = false;
        _state = WIFI_STATE_DISCONNECTED;
        _apStartTime = 0;
        _ipAddress = "";
    }

    bool connectToNetwork(const char* ssid, const char* password) {
        Serial.print(F("Connecting to: "));
        Serial.println(ssid);

        // Save credentials
        strlcpy(_config->wifi.ssid, ssid, sizeof(_config->wifi.ssid));
        strlcpy(_config->wifi.password, password, sizeof(_config->wifi.password));
        _config->save();

        // Stop AP mode if active
        if (_apMode) {
            stopAPMode();
        }

        // Start connection
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);

        _state = WIFI_STATE_CONNECTING;
        _lastConnectAttempt = millis();

        // Wait for connection with timeout
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
            delay(100);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            _state = WIFI_STATE_CONNECTED;
            _ipAddress = WiFi.localIP().toString();
            _rssi = WiFi.RSSI();
            Serial.print(F("Connected! IP: "));
            Serial.println(_ipAddress);
            return true;
        }

        Serial.println(F("Connection failed"));
        return false;
    }

    // Scan for available networks
    int scanNetworks(String* ssids, int* rssis, int maxNetworks) {
        int n = WiFi.scanNetworks();
        int count = min(n, maxNetworks);

        for (int i = 0; i < count; i++) {
            ssids[i] = WiFi.SSID(i);
            rssis[i] = WiFi.RSSI(i);
        }

        WiFi.scanDelete();
        return count;
    }

    // Getters
    bool isConnected() const {
        return _state == WIFI_STATE_CONNECTED;
    }

    bool isAPMode() const {
        return _apMode;
    }

    ConnectionState getState() const {
        return _state;
    }

    const String& getIPAddress() const {
        return _ipAddress;
    }

    int getRSSI() const {
        return _rssi;
    }

    const String& getAPSSID() const {
        return _apSSID;
    }

    String getSSID() const {
        if (_apMode) {
            return _apSSID;
        }
        return WiFi.SSID();
    }

    String getStateString() const {
        switch (_state) {
            case WIFI_STATE_DISCONNECTED: return "Disconnected";
            case WIFI_STATE_CONNECTING: return "Connecting";
            case WIFI_STATE_CONNECTED: return "Connected";
            case WIFI_STATE_AP_MODE: return "AP Mode";
            default: return "Unknown";
        }
    }

    String getMACAddress() const {
        return WiFi.macAddress();
    }

    // Force reconnect
    void reconnect() {
        if (_apMode) return;

        WiFi.disconnect();
        _state = WIFI_STATE_DISCONNECTED;
        _lastConnectAttempt = 0;  // Trigger immediate reconnect
    }

    // Force AP mode (for button press)
    void forceAPMode() {
        if (!_apMode) {
            startAPMode();
        }
    }
};

#endif // WIFI_MANAGER_H
