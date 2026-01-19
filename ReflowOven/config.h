/**
 * Reflow Oven Controller Configuration
 *
 * Hardware: Wemos D1 Mini (ESP8266)
 * Version: 1.0
 */

#ifndef CONFIG_H
#define CONFIG_H

// ===========================================
// Hardware Pin Definitions
// ===========================================

// I2C Display (SSD1306)
#define PIN_SDA 4         // D2 - GPIO4
#define PIN_SCL 5         // D1 - GPIO5

// Rotary Encoder
#define PIN_ENCODER_CLK 14  // D5 - GPIO14
#define PIN_ENCODER_DT 12   // D6 - GPIO12

// Push Button
#define PIN_BUTTON 13       // D7 - GPIO13 (Active LOW with pullup)

// Temperature Sensor (AD8495)
#define PIN_TEMP_SENSOR A0  // ADC0

// SSR Control
#define PIN_SSR 15          // D8 - GPIO15

// ===========================================
// Serial Configuration
// ===========================================
#define SERIAL_BAUD_RATE 115200
#define SERIAL_ENABLE_DEBUG true
#define SERIAL_TEMP_LOGGING true
#define SERIAL_LOG_INTERVAL 1000  // milliseconds

// ===========================================
// Network Configuration (Defaults)
// ===========================================
#define DEFAULT_WIFI_SSID "YourNetworkSSID"
#define DEFAULT_WIFI_PASSWORD "YourPassword"
#define DEFAULT_MQTT_BROKER "192.168.1.100"
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_USER ""
#define DEFAULT_MQTT_PASS ""

// WiFi AP Mode (Fallback)
#define AP_SSID_PREFIX "ToasterOven-Setup-"
#define AP_PASSWORD "configure123"
#define WIFI_CONNECT_TIMEOUT 30000  // 30 seconds

// ===========================================
// Temperature Limits
// ===========================================
#define REFLOW_MAX_TEMP 260       // Hardware cutoff for reflow mode
#define FILAMENT_MAX_TEMP 100     // Hardware cutoff for filament mode
#define TEMP_CALIB_OFFSET -30.0   // Calibration offset in degrees C

// AD8495 Temperature Conversion (Adafruit board)
// Output: 5mV/°C with 1.25V reference at 0°C
// Wemos D1 Mini ADC: 0-3.2V input range, 10-bit (0-1023)
// Formula: Temp = (Vout - 1.25) / 0.005
//          Vout = ADC * 3.2 / 1023
//          Temp = (ADC * 3.2 / 1023 - 1.25) / 0.005
#define ADC_REFERENCE_VOLTAGE 3.2
#define AD8495_OFFSET_VOLTAGE 1.25
#define AD8495_MV_PER_C 0.005

// Temperature Filtering
#define TEMP_SAMPLE_COUNT 10      // Rolling average samples
#define TEMP_OUTLIER_THRESHOLD 10 // Reject readings > 10°C from average

// Sensor Failure Detection
#define SENSOR_OPEN_THRESHOLD 1000    // ADC > 1000 = open thermocouple
#define SENSOR_SHORT_THRESHOLD 10     // ADC < 10 = shorted thermocouple

// ===========================================
// PID Parameters - Reflow Mode
// ===========================================
#define REFLOW_KP 2.0
#define REFLOW_KI 0.5
#define REFLOW_KD 1.0

// ===========================================
// PID Parameters - Filament Drying Mode
// ===========================================
#define FILAMENT_KP 1.5
#define FILAMENT_KI 0.3
#define FILAMENT_KD 0.8

// ===========================================
// PWM Configuration
// ===========================================
#define PWM_FREQUENCY 1       // 1 Hz for SSR
#define PWM_MIN_ON_TIME 100   // Minimum 100ms ON time

// ===========================================
// Safety Timeouts
// ===========================================
#define REFLOW_TIMEOUT 600        // 10 minutes in seconds
#define FILAMENT_TIMEOUT 86400    // 24 hours in seconds

// Thermal Runaway Detection
#define THERMAL_RUNAWAY_TEMP_RISE 5   // Max °C rise when heater off
#define THERMAL_RUNAWAY_TIME 30000    // Detection window in ms

// ===========================================
// Reflow Profile Defaults (Leaded Solder - Sn63/Pb37)
// Melting point: 183°C
// ===========================================
#define REFLOW_PREHEAT_TEMP 130
#define REFLOW_SOAK_TEMP_MIN 130
#define REFLOW_SOAK_TEMP_MAX 160
#define REFLOW_SOAK_DURATION 90       // seconds
#define REFLOW_PEAK_TEMP 210
#define REFLOW_PEAK_DURATION 45       // seconds
#define REFLOW_COOLING_TEMP 80

// Reflow configurable ranges (leaded solder)
#define REFLOW_PEAK_TEMP_MIN 195
#define REFLOW_PEAK_TEMP_MAX 230
#define REFLOW_SOAK_TEMP_CONFIG_MIN 120
#define REFLOW_SOAK_TEMP_CONFIG_MAX 170
#define REFLOW_SOAK_DURATION_MIN 60
#define REFLOW_SOAK_DURATION_MAX 120

// ===========================================
// Filament Drying Presets
// ===========================================
// PLA
#define FILAMENT_PLA_TEMP 45
#define FILAMENT_PLA_TIME 240     // 4 hours in minutes

// PETG
#define FILAMENT_PETG_TEMP 65
#define FILAMENT_PETG_TIME 240

// ABS
#define FILAMENT_ABS_TEMP 70
#define FILAMENT_ABS_TIME 240

// Nylon
#define FILAMENT_NYLON_TEMP 80
#define FILAMENT_NYLON_TIME 360   // 6 hours

// TPU
#define FILAMENT_TPU_TEMP 50
#define FILAMENT_TPU_TIME 240

// Filament configurable ranges
#define FILAMENT_TEMP_MIN 40
#define FILAMENT_TEMP_MAX 100
#define FILAMENT_TIME_MIN 60      // 1 hour in minutes
#define FILAMENT_TIME_MAX 1440    // 24 hours in minutes

// ===========================================
// Display Configuration
// ===========================================
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DISPLAY_ADDRESS 0x3C
#define DISPLAY_UPDATE_INTERVAL 500   // 2 Hz update

// ===========================================
// User Interface Timing
// ===========================================
#define BUTTON_DEBOUNCE_TIME 150      // ms
#define BUTTON_LONG_PRESS_TIME 2000   // 2 seconds for menu back
#define BUTTON_START_PRESS_TIME 5000  // 5 seconds for start/stop
#define ENCODER_ACCEL_THRESHOLD 50    // ms between clicks for acceleration

// ===========================================
// MQTT Configuration
// ===========================================
// Note: MQTT_KEEPALIVE is defined in PubSubClient.h (default 15)
// To change it, modify before including PubSubClient.h
#define MQTT_QOS 1
#define MQTT_RECONNECT_DELAY_BASE 5000    // Initial 5 seconds
#define MQTT_RECONNECT_DELAY_MAX 60000    // Max 60 seconds
#define MQTT_PUBLISH_INTERVAL 5000        // Publish status every 5 seconds

// MQTT Base Topic
#define MQTT_BASE_TOPIC "homeassistant/toaster_oven"

// ===========================================
// Web Server Configuration
// ===========================================
#define WEB_SERVER_PORT 80
#define WEB_UPDATE_INTERVAL 2000      // AJAX polling interval

// ===========================================
// Watchdog Configuration
// ===========================================
#define WATCHDOG_TIMEOUT 8000         // 8 seconds

// ===========================================
// EEPROM Configuration
// ===========================================
#define EEPROM_SIZE 512
#define EEPROM_SIGNATURE 0xAB         // To verify valid data

// EEPROM Addresses
#define EEPROM_ADDR_SIGNATURE 0
#define EEPROM_ADDR_WIFI_SSID 1       // 32 bytes
#define EEPROM_ADDR_WIFI_PASS 33      // 64 bytes
#define EEPROM_ADDR_MQTT_BROKER 97    // 64 bytes
#define EEPROM_ADDR_MQTT_PORT 161     // 2 bytes
#define EEPROM_ADDR_MQTT_USER 163     // 32 bytes
#define EEPROM_ADDR_MQTT_PASS 195     // 32 bytes
#define EEPROM_ADDR_TEMP_OFFSET 227   // 4 bytes (float)
#define EEPROM_ADDR_LAST_MODE 231     // 1 byte
#define EEPROM_ADDR_REFLOW_PEAK 232   // 2 bytes
#define EEPROM_ADDR_REFLOW_SOAK 234   // 2 bytes
#define EEPROM_ADDR_REFLOW_SOAK_TIME 236  // 2 bytes
#define EEPROM_ADDR_PID_PARAMS 238    // 24 bytes (6 floats)

// ===========================================
// Temperature History
// ===========================================
#define TEMP_HISTORY_SIZE 1800        // 30 minutes at 1 Hz

// ===========================================
// Firmware Version
// ===========================================
#define FIRMWARE_VERSION "1.0.0"
#define DEVICE_MODEL "Reflow Oven Controller"
#define DEVICE_MANUFACTURER "DIY"

#endif // CONFIG_H
