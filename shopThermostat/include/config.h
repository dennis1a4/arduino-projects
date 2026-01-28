#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// PIN DEFINITIONS (Wemos D1 Mini)
// ============================================================================

// I2C for LCD Display
#define PIN_SDA         D2      // GPIO4
#define PIN_SCL         D1      // GPIO5

// Relay outputs
#define PIN_RELAY_PUMP  D6      // GPIO12 - Floor pump relay
#define PIN_RELAY_HEATER D7     // GPIO13 - Electric heater relay

// 1-Wire bus for DS18B20 sensors
#define PIN_ONEWIRE     D5      // GPIO14

// Rotary encoder (avoid boot-sensitive pins D3/GPIO0, D4/GPIO2, D8/GPIO15)
#define PIN_ENCODER_A   D0      // GPIO16 - Safe for boot
#define PIN_ENCODER_B   D4      // GPIO2 - Must be HIGH at boot (add pull-up)
#define PIN_ENCODER_BTN D8      // GPIO15 - Must be LOW at boot (add pull-down)

// Relay active state (most relay modules are active LOW)
#define RELAY_ON        LOW
#define RELAY_OFF       HIGH

// ============================================================================
// TEMPERATURE DEFAULTS
// ============================================================================

// Floor zone defaults (frost protection)
#define DEFAULT_FLOOR_TARGET        5.0f    // °C
#define DEFAULT_FLOOR_HYSTERESIS    2.0f    // °C
#define MIN_FLOOR_TARGET            2.0f    // °C
#define MAX_FLOOR_TARGET            15.0f   // °C
#define FLOOR_THERMAL_RUNAWAY       20.0f   // °C - Force OFF above this

// Air zone defaults (comfort heating)
#define DEFAULT_AIR_TARGET          18.0f   // °C (changed from 20 for safety)
#define DEFAULT_AIR_HYSTERESIS      1.0f    // °C
#define MIN_AIR_TARGET              10.0f   // °C
#define MAX_AIR_TARGET              25.0f   // °C
#define AIR_THERMAL_RUNAWAY         30.0f   // °C - Force OFF above this

// Water monitoring thresholds
#define DEFAULT_DELTA_T_WARNING_LOW  1.0f   // °C
#define DEFAULT_DELTA_T_WARNING_HIGH 15.0f  // °C
#define DELTA_T_CRITICAL            0.5f    // °C

// Sensor validation range
#define TEMP_MIN_VALID              -55.0f  // °C
#define TEMP_MAX_VALID              125.0f  // °C
#define TEMP_ERROR_VALUE            -127.0f // DS18B20 error return

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

// Sensor reading interval
#define TEMP_READ_INTERVAL          30000   // 30 seconds

// Control logic interval
#define CONTROL_INTERVAL            10000   // 10 seconds

// Schedule check interval
#define SCHEDULE_INTERVAL           60000   // 60 seconds

// NTP sync interval
#define NTP_SYNC_INTERVAL           3600000 // 1 hour

// MQTT publish interval for state
#define MQTT_PUBLISH_INTERVAL       30000   // 30 seconds

// WiFi reconnection interval
#define WIFI_RECONNECT_INTERVAL     30000   // 30 seconds

// Display update interval
#define DISPLAY_UPDATE_INTERVAL     1000    // 1 second

// Encoder debounce
#define ENCODER_DEBOUNCE_MS         5

// Button press thresholds
#define BUTTON_LONG_PRESS_MS        3000    // 3 seconds
#define BUTTON_VERY_LONG_PRESS_MS   10000   // 10 seconds

// Safety timers
#define MAX_RUNTIME_MS              14400000 // 4 hours in milliseconds
#define MIN_CYCLE_TIME_MS           300000  // 5 minutes
#define SENSOR_FAULT_TIMEOUT_MS     300000  // 5 minutes
#define MANUAL_OVERRIDE_TIMEOUT_MS  7200000 // 2 hours

// ============================================================================
// NETWORK DEFAULTS
// ============================================================================

#define DEFAULT_AP_PASSWORD         "thermostat123"
#define AP_TIMEOUT_MS               900000  // 15 minutes

#define DEFAULT_MQTT_PORT           1883
#define MQTT_KEEPALIVE              60
#define MQTT_RECONNECT_DELAY_MAX    300000  // 5 minutes max

#define DEFAULT_MQTT_BASE_TOPIC     "homeassistant/climate/shop_thermostat"

// ============================================================================
// LCD CONFIGURATION
// ============================================================================

#define LCD_ADDRESS                 0x27
#define LCD_COLS                    16
#define LCD_ROWS                    2

// Number of display modes
#define DISPLAY_MODE_COUNT          6

// Menu auto-exit timeout
#define MENU_TIMEOUT_MS             10000   // 10 seconds

// ============================================================================
// SCHEDULE CONFIGURATION
// ============================================================================

#define MAX_SCHEDULES               7

// ============================================================================
// SENSOR INDICES
// ============================================================================

enum SensorIndex {
    SENSOR_FLOOR = 0,
    SENSOR_AIR = 1,
    SENSOR_OUTDOOR = 2,
    SENSOR_WATER_IN = 3,
    SENSOR_WATER_OUT = 4,
    SENSOR_COUNT = 5
};

// ============================================================================
// ZONE IDENTIFIERS
// ============================================================================

enum ZoneId {
    ZONE_FLOOR = 0,
    ZONE_AIR = 1,
    ZONE_COUNT = 2
};

// ============================================================================
// SYSTEM STATES
// ============================================================================

enum SystemState {
    STATE_BOOT,
    STATE_WIFI_CONNECT,
    STATE_AP_MODE,
    STATE_NORMAL,
    STATE_OFFLINE
};

// ============================================================================
// OVERRIDE MODES
// ============================================================================

enum OverrideMode {
    OVERRIDE_AUTO = 0,
    OVERRIDE_ON = 1,
    OVERRIDE_OFF = 2
};

// ============================================================================
// FLOW STATUS
// ============================================================================

enum FlowStatus {
    FLOW_OK,
    FLOW_WARNING,
    FLOW_CRITICAL,
    FLOW_ERROR
};

// ============================================================================
// DEVICE INFO
// ============================================================================

#define FIRMWARE_VERSION            "1.0.0"
#define DEVICE_MODEL                "ESP8266 Dual Zone v1.1"
#define DEVICE_MANUFACTURER         "DIY"

#endif // CONFIG_H
