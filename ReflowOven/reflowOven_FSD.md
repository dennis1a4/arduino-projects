# Function Specification Document
## Toaster Oven Controller with Reflow and Filament Drying Modes

**Document Version:** 1.0  
**Date:** January 18, 2026  
**Author:** [Your Name]  
**Project:** Arduino-based Toaster Oven Controller

---

## 1. Project Overview

### 1.1 Purpose
This document specifies the functional requirements for an Arduino-based toaster oven controller using a Wemos D1 Mini microcontroller. The system provides precise temperature control for two distinct operating modes: reflow soldering and 3D printer filament drying. The controller features both local (physical) and remote (web/MQTT) interfaces with safety-focused access controls.

### 1.2 Scope
The system encompasses hardware interfacing, temperature control logic, user interface (physical and web-based), MQTT communication for Home Assistant integration, and safety mechanisms to prevent unauthorized remote activation.

---

## 2. Hardware Components

### 2.1 Core Components
- **Microcontroller:** Wemos D1 Mini (ESP8266-based)
- **Display:** 128x64 I2C SSD1306 OLED
- **Temperature Sensor:** AD8495 thermocouple amplifier (K-type thermocouple)
- **Rotary Encoder:** For menu navigation and value adjustment
- **Push Button:** For selection/confirmation
- **Solid State Relay (SSR):** For heater control
- **Power Supply:** Appropriate for Wemos D1 Mini and peripherals

### 2.2 Hardware Connections

#### Pin Assignments (Wemos D1 Mini)
| Component | Pin | GPIO | Notes |
|-----------|-----|------|-------|
| SSD1306 SDA | D2 | GPIO4 | I2C Data |
| SSD1306 SCL | D1 | GPIO5 | I2C Clock |
| Rotary Encoder CLK | D5 | GPIO14 | Clock signal |
| Rotary Encoder DT | D6 | GPIO12 | Data signal |
| Push Button | D7 | GPIO13 | Active LOW with pullup |
| AD8495 Output | A0 | ADC0 | Analog input (0-3.3V) |
| SSR Control | D8 | GPIO15 | PWM capable for future PID |

**Note:** All pin assignments are configurable in the firmware configuration section.

---

## 3. Serial Port Configuration

### 3.1 Serial Port Selection
The system shall support configuration of the serial port used for debugging and monitoring.

**Configuration Parameters:**
- **Baud Rate:** 115200 (default), configurable options: 9600, 57600, 115200
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1
- **Flow Control:** None

### 3.2 Serial Port Usage
- Debug output for development and troubleshooting
- Temperature readings output (optional, configurable)
- System status messages
- Error reporting
- Firmware update messages

### 3.3 Serial Configuration Location
Serial port settings shall be defined in a dedicated configuration header file (`config.h`) with the following parameters:

```cpp
#define SERIAL_BAUD_RATE 115200
#define SERIAL_ENABLE_DEBUG true
#define SERIAL_TEMP_LOGGING true
#define SERIAL_LOG_INTERVAL 1000  // milliseconds
```

---

## 4. Operating Modes

### 4.1 Mode 1: Reflow Soldering

#### 4.1.1 Temperature Profile
The reflow profile shall follow a standard lead-free solder profile:

| Phase | Target Temp (°C) | Duration | Ramp Rate |
|-------|-----------------|----------|-----------|
| Preheat | 150°C | 60-90s | 1-3°C/s |
| Soak | 150-180°C | 60-120s | Maintain |
| Reflow | 220-250°C | 30-60s | 2-3°C/s |
| Cooling | < 100°C | Natural | Passive |

#### 4.1.2 User Configurable Parameters
- Peak temperature (default: 240°C, range: 220-250°C)
- Soak temperature (default: 150°C, range: 140-180°C)
- Soak duration (default: 90s, range: 60-120s)

#### 4.1.3 Safety Limits
- Maximum temperature: 260°C (hardware cutoff)
- Maximum duration: 10 minutes
- Automatic shutdown after cooling phase

### 4.2 Mode 2: Filament Drying

#### 4.2.1 Temperature Profiles
Preset profiles for common filament types:

| Filament Type | Temperature | Duration |
|---------------|-------------|----------|
| PLA | 45°C | 4 hours |
| PETG | 65°C | 4 hours |
| ABS | 70°C | 4 hours |
| Nylon | 80°C | 6 hours |
| TPU | 50°C | 4 hours |
| Custom | User-defined | User-defined |

#### 4.2.2 User Configurable Parameters
- Target temperature (range: 40-90°C)
- Drying duration (range: 1-12 hours)
- Countdown timer display

#### 4.2.3 Safety Limits
- Maximum temperature: 95°C (hardware cutoff)
- Maximum duration: 12 hours
- Automatic shutdown after completion

---

## 5. Physical User Interface

### 5.1 Display Layout

#### 5.1.1 Main Screen (Idle/Running)
```
┌────────────────────────┐
│ MODE: Reflow/Filament  │
│ Current: 125°C         │
│ Target:  150°C         │
│ Status: HEATING        │
│ Time: 02:34 / 05:00    │
│ [MENU]  WiFi:●  MQTT:●│
└────────────────────────┘
```

#### 5.1.2 Menu Structure
```
Main Menu
├── Select Mode
│   ├── Reflow Solder
│   └── Filament Drying
├── Reflow Settings
│   ├── Peak Temp
│   ├── Soak Temp
│   └── Soak Time
├── Filament Settings
│   ├── Filament Type
│   ├── Temperature
│   └── Duration
├── Network Settings
│   ├── WiFi SSID
│   ├── WiFi Password
│   ├── MQTT Broker
│   └── MQTT Port
└── System Info
    ├── WiFi Status
    ├── MQTT Status
    └── Firmware Version
```

### 5.2 User Interactions

#### 5.2.1 Rotary Encoder
- **Rotate:** Navigate menu items or adjust values
- **Rotation Speed:** Accelerated adjustment for large value changes
- **Detents:** Physical feedback for each step

#### 5.2.2 Push Button
- **Short Press:** Select menu item or confirm value
- **Long Press (2s):** Return to previous menu level
- **Long Press (5s) on Main Screen:** START/STOP operation

#### 5.2.3 Operation Start Sequence
To start heating (Safety Requirement):
1. User must be at main screen
2. Mode must be selected
3. Long press button for 5 seconds
4. Display confirmation prompt
5. Short press to confirm START
6. System begins heating cycle

**This is the ONLY way to start the oven - remote interfaces cannot initiate heating.**

#### 5.2.4 Operation Stop Sequence
- Long press button (5s) during operation
- Confirm stop via web interface
- Automatic stop via MQTT command
- Emergency stop on error conditions

---

## 6. Web Interface

### 6.1 Access
- **URL:** http://[device-ip-address]
- **Port:** 80 (HTTP)
- **Authentication:** Basic authentication (optional, configurable)

### 6.2 Web Interface Features

#### 6.2.1 Monitoring Dashboard
- Real-time temperature display (updated every 2 seconds)
- Current operating mode
- Target temperature
- Remaining time (for timed operations)
- Temperature graph (last 30 minutes)
- System status indicators (WiFi, MQTT, Heater)

#### 6.2.2 Remote Control Capabilities
**ALLOWED:**
- Adjust target temperature (within safe limits)
- Stop/Turn OFF the oven
- View operation history
- Modify mode settings (when not running)

**NOT ALLOWED:**
- Start/Turn ON the oven
- Override safety limits
- Disable safety features

#### 6.2.3 Configuration Page
- WiFi network settings
- MQTT broker configuration
- Temperature calibration offset
- Serial port settings
- Safety limit adjustments (password protected)

### 6.3 Web Interface Technical Specifications
- **Framework:** ESP8266WebServer library
- **Response Format:** HTML5 with embedded CSS/JavaScript
- **Real-time Updates:** AJAX polling (2-second interval)
- **Mobile Responsive:** Yes
- **Browser Compatibility:** Modern browsers (Chrome, Firefox, Safari, Edge)

---

## 7. MQTT Integration

### 7.1 MQTT Broker Connection
- **Protocol:** MQTT v3.1.1
- **Port:** 1883 (default, configurable)
- **Client ID:** `toaster-oven-[MAC-address]`
- **Keep Alive:** 60 seconds
- **Clean Session:** True
- **QoS:** 1 (At least once delivery)
- **Reconnect:** Automatic with exponential backoff

### 7.2 MQTT Topics Structure

#### 7.2.1 Base Topic
`homeassistant/toaster_oven/[device-id]/`

#### 7.2.2 Published Topics (Status)
| Topic | Payload | Update Frequency | Retained |
|-------|---------|------------------|----------|
| `state` | ON/OFF | On change | Yes |
| `temperature/current` | Float (°C) | Every 5s | Yes |
| `temperature/target` | Float (°C) | On change | Yes |
| `mode` | reflow/filament/idle | On change | Yes |
| `status` | heating/cooling/maintaining/idle/error | On change | Yes |
| `time/remaining` | Integer (seconds) | Every 30s | No |
| `wifi/rssi` | Integer (dBm) | Every 60s | No |
| `availability` | online/offline | On connect/LWT | Yes |

#### 7.2.3 Subscribed Topics (Commands)
| Topic | Payload | Action |
|-------|---------|--------|
| `command/stop` | any | Stop operation (safe shutdown) |
| `command/set_temp` | Float (°C) | Set target temperature (if within limits) |
| `command/set_mode` | reflow/filament | Change mode (only when idle) |
| `config/update` | JSON object | Update configuration parameters |

**Security Note:** Start command is NOT implemented via MQTT.

### 7.3 Home Assistant Auto-Discovery

#### 7.3.1 Discovery Topic
`homeassistant/climate/toaster_oven_[device-id]/config`

#### 7.3.2 Discovery Payload (JSON)
```json
{
  "name": "Toaster Oven Controller",
  "unique_id": "toaster_oven_12345",
  "device": {
    "identifiers": ["toaster_oven_12345"],
    "name": "Reflow Oven",
    "model": "Custom Controller v1.0",
    "manufacturer": "DIY"
  },
  "mode_command_topic": "homeassistant/toaster_oven/12345/command/set_mode",
  "mode_state_topic": "homeassistant/toaster_oven/12345/mode",
  "temperature_command_topic": "homeassistant/toaster_oven/12345/command/set_temp",
  "temperature_state_topic": "homeassistant/toaster_oven/12345/temperature/target",
  "current_temperature_topic": "homeassistant/toaster_oven/12345/temperature/current",
  "modes": ["reflow", "filament", "off"],
  "min_temp": 40,
  "max_temp": 260,
  "temp_step": 5,
  "availability_topic": "homeassistant/toaster_oven/12345/availability"
}
```

### 7.4 MQTT Error Handling
- Connection failure: Retry with exponential backoff (5s, 10s, 30s, 60s)
- Publish failure: Queue messages (max 10) and retry
- Invalid command: Log and ignore, publish error status
- Network loss during operation: Continue operation safely, attempt reconnection

---

## 8. Temperature Control System

### 8.1 Temperature Sensing

#### 8.1.1 AD8495 Specifications
- **Input:** K-type thermocouple
- **Output:** 5mV/°C
- **Reference Junction:** Internally compensated
- **Range:** 0-500°C (with appropriate thermocouple)
- **Supply Voltage:** 3.3V or 5V

#### 8.1.2 ADC Conversion
- **ADC Resolution:** 10-bit (0-1023)
- **Reference Voltage:** 3.3V
- **Conversion Formula:**
  ```
  Temperature (°C) = (ADC_Value * 3.3 / 1023) / 0.005
  Temperature (°C) = ADC_Value * 0.6459
  ```

#### 8.1.3 Temperature Reading
- **Sample Rate:** 10 Hz (100ms interval)
- **Averaging:** Rolling average of last 10 samples (1 second window)
- **Filtering:** Outlier rejection (discard values >10°C from average)
- **Calibration Offset:** User-adjustable via web interface

### 8.2 Control Algorithm

#### 8.2.1 PID Controller
- **Algorithm:** Proportional-Integral-Derivative control
- **Update Rate:** 1 Hz (1 second)
- **Output:** PWM signal (0-100% duty cycle) to SSR

**PID Parameters (Tunable):**
```
Reflow Mode:
- Kp (Proportional): 2.0
- Ki (Integral): 0.5
- Kd (Derivative): 1.0

Filament Drying Mode:
- Kp: 1.5
- Ki: 0.3
- Kd: 0.8
```

#### 8.2.2 PWM Configuration
- **Frequency:** 1 Hz (1 second period) - suitable for SSR thermal cycling
- **Resolution:** 8-bit (0-255 levels)
- **Minimum ON Time:** 100ms (prevents SSR chattering)

#### 8.2.3 Control Logic
```
IF (Current Temp < Target - 10°C):
    Apply full power (100% PWM)
ELSE IF (Current Temp within 10°C of Target):
    Apply PID control
ELSE IF (Current Temp > Target):
    Turn heater OFF
```

### 8.3 Safety Features

#### 8.3.1 Over-Temperature Protection
- **Trigger:** Current temperature > Maximum Safe Limit
- **Reflow Mode Limit:** 260°C
- **Filament Mode Limit:** 95°C
- **Action:** Immediate heater shutoff, display error, MQTT alert

#### 8.3.2 Thermal Runaway Detection
- **Definition:** Temperature continues rising despite heater being off
- **Detection:** Temperature increase >5°C when heater OFF for >30s
- **Action:** Shutdown, display error, require manual reset

#### 8.3.3 Sensor Failure Detection
- **Open Thermocouple:** ADC reading > 1000 (approximately >650°C)
- **Shorted Thermocouple:** ADC reading < 10 (approximately <6°C) when heater has been on
- **Action:** Immediate shutdown, display error message

#### 8.3.4 Timeout Protection
- **Maximum Continuous Operation:**
  - Reflow: 10 minutes
  - Filament: 12 hours
- **Action:** Automatic safe shutdown with notification

---

## 9. Network Configuration

### 9.1 WiFi Configuration

#### 9.1.1 Connection Modes
1. **Station Mode (Primary):**
   - Connects to existing WiFi network
   - Credentials stored in EEPROM/Flash
   - Automatic reconnection on disconnect

2. **Access Point Mode (Fallback):**
   - Activates if connection fails after 30 seconds
   - SSID: `ToasterOven-Setup-[MAC]`
   - Password: `configure123` (or user-defined)
   - Web interface for WiFi configuration
   - Automatic switch to Station mode after configuration

#### 9.1.2 WiFi Settings Storage
- **Location:** EEPROM or SPIFFS filesystem
- **Parameters:**
  - SSID (max 32 characters)
  - Password (max 64 characters)
  - MQTT broker address
  - MQTT port
  - MQTT username/password

### 9.2 Network Status Indicators
- **WiFi Connected:** Solid WiFi icon on display
- **WiFi Disconnected:** Flashing WiFi icon
- **MQTT Connected:** Solid MQTT icon
- **MQTT Disconnected:** No MQTT icon or error symbol

---

## 10. Data Storage and Logging

### 10.1 Non-Volatile Storage (EEPROM)
**Stored Parameters:**
- WiFi credentials
- MQTT broker configuration
- Last used mode
- Temperature calibration offset
- PID tuning parameters
- User preferences (units, display timeout)

### 10.2 Session Logging
**Volatile Memory (RAM) Storage:**
- Temperature history (last 1800 samples = 30 minutes at 1Hz)
- Operation start time
- Mode-specific parameters
- Error events

### 10.3 Log Export
- Download via web interface (CSV format)
- MQTT publish on completion (summary statistics)

---

## 11. Firmware Architecture

### 11.1 Main Program Structure

```
Setup():
  - Initialize serial communication
  - Initialize I2C (display)
  - Initialize GPIO (encoder, button, SSR)
  - Initialize ADC (temperature sensor)
  - Load configuration from EEPROM
  - Initialize WiFi
  - Initialize MQTT client
  - Initialize web server
  - Display splash screen

Loop():
  - Read and process encoder input
  - Read and process button input
  - Read and filter temperature
  - Update PID controller
  - Control SSR output
  - Update display
  - Handle web server requests
  - Handle MQTT messages
  - Check safety conditions
  - Update statistics
```

### 11.2 Task Timing

| Task | Frequency | Priority |
|------|-----------|----------|
| Safety checks | 10 Hz | Critical |
| Temperature reading | 10 Hz | High |
| PID control | 1 Hz | High |
| Display update | 2 Hz | Medium |
| Encoder/Button poll | 50 Hz | Medium |
| Web server | Event-driven | Medium |
| MQTT publish | 0.2 Hz (5s) | Low |
| WiFi reconnect | As needed | Low |

### 11.3 State Machine

**System States:**
```
IDLE → PREHEATING → SOAKING → REFLOW → COOLING → COMPLETE
  ↓                     ↓          ↓        ↓
ERROR ← ─ ─ ─ ─ ─ ─ ─ ─┴─ ─ ─ ─ ─ ┴─ ─ ─ ─┘

IDLE → HEATING → MAINTAINING → COMPLETE
  ↓        ↓           ↓
ERROR ← ─ ┴─ ─ ─ ─ ─ ─┘
```

---

## 12. Error Handling and Recovery

### 12.1 Error Types and Responses

| Error Code | Description | Response | Recovery |
|------------|-------------|----------|----------|
| E01 | Over-temperature | Immediate shutdown | Manual reset required |
| E02 | Sensor failure | Immediate shutdown | Check connections |
| E03 | Thermal runaway | Immediate shutdown | Manual reset required |
| E04 | WiFi connection lost | Continue operation | Auto-reconnect |
| E05 | MQTT connection lost | Continue operation | Auto-reconnect |
| E06 | Invalid command | Ignore command | Log and continue |
| E07 | Timeout exceeded | Safe shutdown | Auto-reset on restart |

### 12.2 Error Display
- Error code and description on OLED
- Error published to MQTT
- Error logged to serial
- Red LED (if available) for critical errors

### 12.3 Watchdog Timer
- **Timeout:** 8 seconds
- **Purpose:** System hang detection and recovery
- **Action:** Automatic system reset

---

## 13. Configuration Files

### 13.1 config.h Structure
```cpp
// Hardware Pin Definitions
#define PIN_SDA 4
#define PIN_SCL 5
#define PIN_ENCODER_CLK 14
#define PIN_ENCODER_DT 12
#define PIN_BUTTON 13
#define PIN_TEMP_SENSOR A0
#define PIN_SSR 15

// Serial Configuration
#define SERIAL_BAUD_RATE 115200
#define SERIAL_ENABLE_DEBUG true

// Network Configuration
#define WIFI_SSID "YourNetworkSSID"
#define WIFI_PASSWORD "YourPassword"
#define MQTT_BROKER "192.168.1.100"
#define MQTT_PORT 1883
#define MQTT_USER "username"
#define MQTT_PASS "password"

// Temperature Limits
#define REFLOW_MAX_TEMP 260
#define FILAMENT_MAX_TEMP 95
#define TEMP_CALIB_OFFSET 0.0

// PID Parameters
#define REFLOW_KP 2.0
#define REFLOW_KI 0.5
#define REFLOW_KD 1.0
#define FILAMENT_KP 1.5
#define FILAMENT_KI 0.3
#define FILAMENT_KD 0.8

// Safety Timeouts (seconds)
#define REFLOW_TIMEOUT 600
#define FILAMENT_TIMEOUT 43200
```

---

## 14. Testing Requirements

### 14.1 Hardware Testing
- [ ] Verify all GPIO connections
- [ ] Test temperature sensor accuracy (calibrated thermometer)
- [ ] Test SSR switching (load test)
- [ ] Verify display functionality
- [ ] Test encoder rotation and button press
- [ ] Power supply stability test

### 14.2 Software Testing
- [ ] Temperature reading accuracy (±2°C)
- [ ] PID control stability
- [ ] Over-temperature protection activation
- [ ] Thermal runaway detection
- [ ] Sensor failure detection
- [ ] WiFi reconnection logic
- [ ] MQTT publish/subscribe functionality
- [ ] Web interface responsiveness
- [ ] Remote control limitations (cannot start)
- [ ] Profile execution (reflow and filament modes)

### 14.3 Integration Testing
- [ ] Home Assistant discovery and control
- [ ] Web interface + MQTT simultaneous use
- [ ] Network loss during operation
- [ ] Power loss recovery (safety state)
- [ ] Emergency stop from all interfaces

### 14.4 Safety Testing
- [ ] Maximum temperature cutoff
- [ ] Timeout protection
- [ ] Physical interface-only start requirement
- [ ] Remote stop functionality
- [ ] Error state recovery

---

## 15. User Documentation Requirements

### 15.1 Quick Start Guide
- Hardware assembly instructions
- Initial configuration steps
- WiFi setup procedure
- First operation walkthrough

### 15.2 Operation Manual
- Mode selection guide
- Temperature profile explanations
- Safety precautions
- Troubleshooting common issues

### 15.3 Technical Documentation
- Pin assignment reference
- MQTT topic documentation
- API reference for web interface
- Configuration parameter guide

---

## 16. Future Enhancements (Optional)

### 16.1 Potential Features
- SD card logging for long-term data storage
- Custom profile editor via web interface
- Multi-profile storage (save/load profiles)
- Temperature graph on OLED display
- Audio alerts (buzzer) for completion
- Multi-language support
- OTA (Over-The-Air) firmware updates
- Integration with other home automation platforms

### 16.2 Hardware Expansions
- Multiple temperature sensors
- Exhaust fan control
- Door interlock switch
- External emergency stop button
- Status LED indicators

---

## 17. Compliance and Safety Notes

### 17.1 Electrical Safety
- All high-voltage wiring must be properly insulated
- SSR must be rated for oven heater load
- Proper grounding required
- Thermal fuse recommended as backup

### 17.2 User Warnings
- Never leave operating oven unattended
- Ensure adequate ventilation
- Keep flammable materials away
- Regular inspection of thermocouple integrity
- Not for commercial use

---

## 18. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01-18 | [Your Name] | Initial specification document |

---

## 19. Approval Signatures

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Project Lead | | | |
| Hardware Engineer | | | |
| Software Engineer | | | |

---

**End of Document**
