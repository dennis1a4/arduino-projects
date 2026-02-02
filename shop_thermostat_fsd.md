# Functional Specification Document: Smart Dual-Zone Shop Thermostat

**Project**: ESP8266-Based Dual-Zone Thermostat with Home Assistant Integration  
**Version**: 1.2  
**Date**: January 25, 2026  
**Platform**: Wemos D1 Mini (ESP8266)

---

## 1. Executive Summary

### 1.1 Purpose
Develop a dual-zone thermostat system for a workshop environment that manages two independent heating systems while integrating with Home Assistant for remote monitoring and automation.

### 1.2 Key Features
- Dual-zone temperature control (floor heating + electric heater)
- Five-sensor temperature monitoring (floor, air, outdoor, water tank in/out)
- Web-based configuration interface
- Local LCD display with rotary encoder control
- WiFi connectivity with AP fallback mode
- MQTT integration with Home Assistant
- Scheduling system for both zones
- Frost protection for in-floor heating
- Water system health monitoring and diagnostics

---

## 2. Hardware Specifications

### 2.1 Component List

| Component | Specification | Quantity | Purpose |
|-----------|--------------|----------|---------|
| Microcontroller | Wemos D1 Mini (ESP8266) | 1 | Main controller |
| **Temperature Sensors** | **DS18B20 waterproof** | **5** | **All temp monitoring** |
| Relay Module | 2-channel 5V/10A relay | 1 | Pump and heater control |
| Display | 16x2 LCD with I2C backpack | 1 | Local status display |
| Input | Rotary encoder with push button | 1 | Local configuration |
| **4.7kΩ Resistor** | **1/4W tolerance** | **1** | **1-Wire pullup (all sensors)** |
| Power Supply | 5V/2A USB or barrel jack | 1 | System power |

### 2.2 Pin Assignments (Wemos D1 Mini)

| GPIO | D Pin | Function | Component |
|------|-------|----------|-----------|
| GPIO4 | D2 | I2C SDA | LCD Display |
| GPIO5 | D1 | I2C SCL | LCD Display |
| GPIO16 | D0 | Relay 1 | Floor Pump |
| GPIO13 | D7 | Relay 2 | Electric Heater |
| GPIO14 | D5 | 1-Wire Bus | Temperature Sensors (all 5) |
| GPIO2 | D4 | Encoder A | Rotary Encoder (interrupt, INPUT_PULLUP) |
| GPIO12 | D6 | Encoder B | Rotary Encoder (INPUT_PULLUP) |
| — | A0 | Encoder Button | Push Button (analogRead) |

### 2.3 Temperature Sensor Configuration

| Sensor ID | Location | Purpose | Mounting |
|-----------|----------|---------|----------|
| **Sensor 1** | Floor slab | Frost protection control | Embedded in concrete |
| **Sensor 2** | Shop air | Comfort heating control | Wall-mounted, breathing height |
| **Sensor 3** | Outdoor | Reference/display | North wall, shaded |
| **Sensor 4** | Water tank inlet | Monitor supply temp | Pipe clamp or well |
| **Sensor 5** | Water tank outlet | Monitor return temp | Pipe clamp or well |

### 2.4 Electrical Specifications
- **Floor Pump Relay**: Controls hydronic circulation pump (typical 1/20 HP, 120V)
- **Heater Relay**: Controls 6kW electric heater (240V circuit via contactor or 120V control)
- **Relay Ratings**: Minimum 10A @ 250VAC
- **Temperature Sensors**: DS18B20 (-55°C to +125°C range)
- **1-Wire Bus**: All 5 sensors on single GPIO with 4.7kΩ pull-up

**Note**: 6kW heater requires professional electrical installation with appropriate contactors/relays rated for the load.

### 2.5 1-Wire Bus Specifications

**DS18B20 on Single 1-Wire Bus:**
- **Current Configuration**: 5 sensors
- **Practical Maximum**: 20-30 devices
- **Recommended Pull-up**: 4.7kΩ (standard) or 2.2kΩ if cable runs exceed 10m
- **Addressing**: Each sensor has unique 64-bit ROM address
- **Bus Topology**: Star or linear acceptable for 5 sensors

---

## 3. System Architecture

### 3.1 Software Stack
- **Core**: Arduino Framework (ESP8266 core)
- **Web Server**: ESPAsyncWebServer
- **MQTT Client**: PubSubClient or AsyncMqttClient
- **WiFi Manager**: WiFiManager library
- **Display**: LiquidCrystal_I2C
- **Temperature**: OneWire + DallasTemperature (for DS18B20)
- **Storage**: LittleFS for configuration persistence

### 3.2 Data Flow
```
Sensors → ESP8266 → Control Logic → Relays
                  ↓
                  ├→ LCD Display
                  ├→ Web Interface
                  └→ MQTT → Home Assistant
```

---

## 4. Functional Requirements

## 4.1 Temperature Sensing

### 4.1.1 Sensor Configuration

**All Sensors on 1-Wire Bus (GPIO14):**
- Floor sensor - measures in-floor temperature
- Air sensor - measures ambient shop air temperature
- Outdoor sensor - measures outside temperature
- Water inlet sensor - measures tank supply temperature
- Water outlet sensor - measures tank return temperature

### 4.1.2 Reading Specifications
- **Sampling Rate**: Every 30 seconds
- **Accuracy**: ±0.5°C
- **Error Handling**: 
  - Detect sensor failures (CRC errors, out-of-range readings)
  - Display error on LCD
  - Publish error status via MQTT
  - Use safe fallback (disable heating if critical sensor fails)

### 4.1.3 Sensor Identification

Each DS18B20 has a unique 64-bit ROM address. Initial setup requires sensor discovery and address mapping.

**Auto-Discovery Process:**
1. Upload sketch with discovery function
2. Heat/cool each sensor individually to identify addresses
3. Note addresses in configuration
4. Save addresses to config file or hard-code

---

## 4.2 Heating Control

### 4.2.1 Zone 1: In-Floor Heating (Frost Protection)

**Primary Function**: Maintain minimum temperature above freezing

| Parameter | Default Value | Range | Description |
|-----------|--------------|-------|-------------|
| Target Floor Temp | 5°C (41°F) | 2-15°C | Prevents freezing |
| Hysteresis | 2°C | 1-5°C | Prevents relay cycling |
| Sensor | Floor probe | - | Primary control sensor |

**Basic Control Logic**:
```
IF floor_temp < (target_temp - hysteresis/2):
    pump_relay = ON
ELSE IF floor_temp > (target_temp + hysteresis/2):
    pump_relay = OFF
```

**Enhanced Control Logic (Optional with Water Monitoring)**:
```
IF floor_temp < (target_temp - hysteresis/2) AND tank_delta_t > 1.0:
    pump_relay = ON  // Only run if tank has heat to give
ELSE IF floor_temp < target AND tank_delta_t < 1.0:
    pump_relay = OFF  // Don't waste electricity pumping cold water
```

### 4.2.2 Zone 2: Electric Heater (Comfort Heating)

**Primary Function**: Provide comfortable working temperature

| Parameter | Default Value | Range | Description |
|-----------|--------------|-------|-------------|
| Target Air Temp | 18°C (64°F) | 10-25°C | Working comfort temp |
| Hysteresis | 1°C | 0.5-3°C | Prevents relay cycling |
| Sensor | Air probe | - | Primary control sensor |

**Control Logic**:
```
IF air_temp < (target_temp - hysteresis/2) AND schedule_active:
    heater_relay = ON
ELSE IF air_temp > (target_temp + hysteresis/2) OR NOT schedule_active:
    heater_relay = OFF
```

### 4.2.3 Water System Monitoring

**Delta-T Calculation**:
```
ΔT = water_outlet_temp - water_inlet_temp
```

**Diagnostic Indicators**:
- **Positive ΔT (2-5°C)**: Normal operation - tank heating water
- **Low ΔT (<1.0°C)**: Warning - poor heat transfer or pump issue
- **Very Low ΔT (<0.5°C)**: Critical - no flow or tank depleted
- **Negative ΔT**: Error - possible flow reversal or sensor error
- **High ΔT (>15°C)**: Warning - possible sensor error

**Applications**:
- System health monitoring
- Detect scaling or sediment buildup
- Verify pump operation
- Early warning of tank failure
- Optional smart pump control

### 4.2.4 Safety Features
- **Maximum Runtime**: 4 hours continuous per zone (configurable)
- **Sensor Failure Lockout**: Disable zone if sensor reading invalid for >5 minutes
- **Temperature Limits**: 
  - Floor: Maximum 15°C to prevent overheating
  - Air: Maximum 25°C for safety
  - Floor (thermal runaway): Force OFF if exceeds 20°C
  - Air (thermal runaway): Force OFF if exceeds 30°C
- **Minimum Cycle Time**: 5 minutes between relay state changes

---

## 4.3 Scheduling System

### 4.3.1 Schedule Structure
Each zone supports up to **7 schedule periods** (configurable)

**Schedule Entry Format**:
```json
{
  "enabled": true,
  "days": [1,2,3,4,5],  // Monday-Friday (0=Sunday, 6=Saturday)
  "start_time": "08:00",
  "end_time": "17:00",
  "target_temp": 20.0,
  "zone": "heater"  // or "floor"
}
```

### 4.3.2 Schedule Priority
1. Manual override (via web/MQTT) - highest priority, 2-hour timeout
2. Active schedule period
3. Default setpoint (frost protection for floor, OFF for heater)

### 4.3.3 Time Synchronization
- **NTP Client**: Sync time on WiFi connection
- **Timezone**: Configurable via web interface
- **Fallback**: Internal RTC (loses time on power loss, requires resync)

---

## 4.4 WiFi Connectivity

### 4.4.1 Normal Operation Mode
- **SSID/Password**: Stored in persistent memory
- **Connection**: Auto-connect on boot
- **Reconnection**: Automatic retry every 30 seconds if connection lost
- **Status Indicator**: Show WiFi status on LCD

### 4.4.2 Access Point (AP) Mode

**Trigger Conditions**:
1. No WiFi credentials configured
2. Stored credentials fail to connect after 60 seconds
3. Manual trigger via button hold (10 seconds)

**AP Configuration**:
- **SSID**: `ShopThermostat-[ChipID]`
- **Password**: `thermostat123` (configurable)
- **IP Address**: 192.168.4.1
- **Duration**: Remains in AP mode until WiFi configured or 15 minutes timeout

**Captive Portal**:
- Auto-redirects to configuration page
- Allows WiFi scanning and selection
- Password entry for selected network
- Tests connection before saving

### 4.4.3 WiFi Configuration Page
- Scan for available networks
- Manual SSID entry option
- Password field with show/hide toggle
- Test connection button
- Save and reboot option

---

## 4.5 Web Interface

### 4.5.1 Dashboard Page (`/`)

**Display Elements**:
- Current temperatures (floor, air, outdoor, water in/out) with icons
- Zone status indicators (ON/OFF/AUTO)
- Target temperatures for both zones
- Water system delta-T with status indicator
- Active schedule indication
- System uptime and WiFi signal strength
- Relay status indicators

**Quick Controls**:
- Temperature adjustment sliders for each zone
- Manual override buttons (ON/OFF/AUTO)
- Schedule enable/disable toggles

**Water System Status Card**:
```
┌─────────────────────────────────┐
│   Hot Water Tank Monitoring     │
├─────────────────────────────────┤
│ Inlet Temp:      45.2°C         │
│ Outlet Temp:     42.8°C         │
│ Delta-T:         2.4°C   ✓ OK   │
│                                 │
│ Flow Status:     Normal         │
│ Pump Runtime:    3h 24m today   │
└─────────────────────────────────┘
```

### 4.5.2 Settings Page (`/settings`)

**Temperature Settings**:
- Floor target temperature (°C or °F)
- Air target temperature
- Hysteresis values
- Temperature units selection
- Sensor calibration offsets (all 5 sensors)

**Water Monitoring Settings**:
- Enable/disable water monitoring
- Delta-T warning thresholds (low/high)
- Smart pump control toggle
- Flow status alert settings

**System Settings**:
- Device name
- Timezone selection
- NTP server
- Temperature sensor calibration offsets

**Safety Settings**:
- Maximum runtime limits
- Temperature limit thresholds
- Minimum cycle time

### 4.5.3 Schedule Page (`/schedule`)

**Features**:
- Visual weekly schedule grid
- Add/edit/delete schedule entries
- Day-of-week selection (checkboxes)
- Time pickers for start/end
- Target temperature per entry
- Zone selection per entry
- Enable/disable individual schedules
- Copy schedule to other days

### 4.5.4 MQTT Configuration Page (`/mqtt`)

**Settings**:
- MQTT broker address
- Port (default 1883)
- Username/password (optional)
- Base topic (default: `homeassistant/climate/shop_thermostat`)
- Enable/disable MQTT
- Connection status display
- Test connection button

### 4.5.5 WiFi Configuration Page (`/wifi`)

**Features**:
- Current connection status
- Signal strength indicator
- Change WiFi credentials
- Network scanner
- Reconnect button
- Reboot to AP mode option

### 4.5.6 Technical Requirements
- **Responsive Design**: Works on mobile and desktop
- **Update Frequency**: Real-time updates via WebSocket or AJAX (5-second refresh)
- **Authentication**: Optional password protection for web interface
- **Styling**: Clean, modern UI with Bootstrap or similar framework

---

## 4.6 MQTT Integration

### 4.6.1 Home Assistant Discovery

Publishes auto-discovery messages for Home Assistant MQTT discovery protocol.

**Climate Entity for Floor Heating**:
```
Topic: homeassistant/climate/shop_thermostat_floor/config
Payload:
{
  "name": "Shop Floor Heating",
  "unique_id": "shop_thermo_floor",
  "mode_cmd_t": "~/floor/mode/set",
  "mode_stat_t": "~/floor/mode",
  "temp_cmd_t": "~/floor/target/set",
  "temp_stat_t": "~/floor/target",
  "curr_temp_t": "~/floor/current",
  "modes": ["off", "heat"],
  "min_temp": 2,
  "max_temp": 15,
  "temp_step": 0.5,
  "device": {...}
}
```

**Climate Entity for Air Heating**:
```
Topic: homeassistant/climate/shop_thermostat_air/config
Payload: (similar structure for air zone)
```

### 4.6.2 State Topics (Published by Device)

| Topic | Payload | Frequency | Description |
|-------|---------|-----------|-------------|
| `~/floor/current` | `5.2` | 30s | Current floor temp (°C) |
| `~/air/current` | `18.5` | 30s | Current air temp (°C) |
| `~/outdoor/current` | `-2.3` | 30s | Current outdoor temp (°C) |
| `~/water/inlet` | `45.2` | 30s | Tank inlet temp (°C) |
| `~/water/outlet` | `42.8` | 30s | Tank outlet temp (°C) |
| `~/water/delta` | `2.4` | 30s | Temperature difference |
| `~/water/flow_status` | `OK` or `WARNING` | 60s | Flow health indicator |
| `~/floor/target` | `5.0` | On change | Floor target temp |
| `~/air/target` | `20.0` | On change | Air target temp |
| `~/floor/mode` | `heat` | On change | Floor mode (heat/off) |
| `~/air/mode` | `off` | On change | Air mode (heat/off) |
| `~/floor/relay` | `ON` | On change | Pump relay state |
| `~/air/relay` | `OFF` | On change | Heater relay state |
| `~/status` | `online` | On connect | Availability (LWT) |
| `~/wifi/rssi` | `-65` | 60s | WiFi signal strength |

### 4.6.3 Command Topics (Subscribed by Device)

| Topic | Payload | Action |
|-------|---------|--------|
| `~/floor/target/set` | `6.0` | Set floor target temp |
| `~/air/target/set` | `21.0` | Set air target temp |
| `~/floor/mode/set` | `heat` or `off` | Set floor mode |
| `~/air/mode/set` | `heat` or `off` | Set air mode |
| `~/command` | `reboot` | System commands |

### 4.6.4 Sensor Entities

Additional sensor entities for detailed monitoring:

**Temperature Sensors**:
- Floor temperature sensor
- Air temperature sensor
- Outdoor temperature sensor
- Water tank inlet sensor
- Water tank outlet sensor
- Water tank delta-T sensor

**Status Sensors**:
- WiFi signal strength sensor
- Uptime sensor
- Floor relay state binary sensor
- Heater relay state binary sensor
- Water flow status sensor

### 4.6.5 Home Assistant Sensor Discovery Examples

```json
{
  "name": "Shop Water Tank Inlet",
  "unique_id": "shop_thermo_water_in",
  "state_topic": "homeassistant/climate/shop_thermostat/water/inlet",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "device": {...}
}
```

```json
{
  "name": "Shop Water Tank Delta-T",
  "unique_id": "shop_thermo_water_delta",
  "state_topic": "homeassistant/climate/shop_thermostat/water/delta",
  "unit_of_measurement": "°C",
  "icon": "mdi:thermometer-lines",
  "device": {...}
}
```

### 4.6.6 MQTT Specifications
- **QoS**: 1 (at least once delivery)
- **Retained**: State messages retained
- **Last Will**: `~/status` = `offline`
- **Keep Alive**: 60 seconds
- **Reconnection**: Automatic with exponential backoff

---

## 4.7 Local Display & Control

### 4.7.1 LCD Display (16x2)

**Display Modes** (cycle with encoder button):

**Mode 1: Primary Temperatures**
```
F:5.2° A:18.5°
P:ON H:OFF
```

**Mode 2: Targets & Status**
```
T:5/20° Out:-2°
WiFi:OK MQTT:OK
```

**Mode 3: Water Temperatures**
```
Tank In:  45.2°C
Tank Out: 42.8°C
```

**Mode 4: Water Delta-T**
```
ΔT: 2.4°C Flow:OK
Pump Runtime: 3h
```

**Mode 5: Schedule Status**
```
Sched: ACTIVE
08:00-17:00 20°C
```

**Mode 6: System Info**
```
IP:192.168.1.50
Up: 5d 3h 22m
```

### 4.7.2 Rotary Encoder Control

**Navigation**:
- **Rotate**: Navigate menu items or adjust values
- **Short Press**: Select/confirm
- **Long Press (3s)**: Enter/exit menu mode
- **Very Long Press (10s)**: Enter AP mode

**Menu Structure**:
```
Main Menu
├── Floor Target [2-15°C]
├── Air Target [10-25°C]
├── Manual Override
│   ├── Floor: Auto/On/Off
│   └── Air: Auto/On/Off
├── WiFi Info
├── MQTT Status
├── Water System Info
└── Reboot System
```

### 4.7.3 User Interaction Flow

**Adjusting Floor Temperature**:
1. Long press encoder (enter menu)
2. Rotate to "Floor Target"
3. Press to select
4. Rotate to adjust temperature
5. Press to confirm
6. Auto-exit to main display after 10s

**Manual Override**:
1. Navigate to "Manual Override"
2. Select zone
3. Choose Auto/On/Off
4. Override expires after 2 hours or schedule change

---

## 5. Software Design

### 5.1 State Machine

**System States**:
1. **BOOT**: Initialize hardware, load configuration
2. **WIFI_CONNECT**: Attempt WiFi connection
3. **AP_MODE**: Access point for configuration
4. **NORMAL**: Normal operation with WiFi
5. **OFFLINE**: Normal operation without WiFi (degraded)

**Transitions**:
```
BOOT → WIFI_CONNECT (if credentials exist)
BOOT → AP_MODE (if no credentials)
WIFI_CONNECT → NORMAL (connection successful)
WIFI_CONNECT → AP_MODE (60s timeout)
NORMAL → OFFLINE (WiFi lost)
OFFLINE → WIFI_CONNECT (retry timer)
ANY → AP_MODE (button hold 10s)
```

### 5.2 Main Loop Structure

```cpp
void loop() {
    // Run every iteration
    handleEncoder();           // ~1ms
    handleWebServer();         // Non-blocking
    handleMQTT();             // Non-blocking
    
    // Timed tasks
    if (tempReadTimer.ready()) {
        readTemperatures();    // Every 30s
        calculateWaterDeltaT(); // Calculate ΔT
        updateDisplay();
    }
    
    if (controlTimer.ready()) {
        runControlLogic();     // Every 10s
        checkWaterFlowStatus(); // Monitor water system
        publishMQTTState();
    }
    
    if (scheduleTimer.ready()) {
        checkSchedules();      // Every 60s
    }
    
    if (ntpTimer.ready()) {
        syncNTP();            // Every 1 hour
    }
}
```

### 5.3 Temperature Reading Functions

```cpp
struct TemperatureReadings {
    float floor;
    float air;
    float outdoor;
    float water_in;
    float water_out;
    float water_delta;
    bool valid[5];  // Validity flags for each sensor
};

TemperatureReadings readAllTemperatures() {
    TemperatureReadings temps;
    
    // Request temperatures from all sensors
    sensors.requestTemperatures();
    
    // Read each sensor by address
    temps.floor = sensors.getTempC(sensorAddr.floor);
    temps.air = sensors.getTempC(sensorAddr.air);
    temps.outdoor = sensors.getTempC(sensorAddr.outdoor);
    temps.water_in = sensors.getTempC(sensorAddr.water_in);
    temps.water_out = sensors.getTempC(sensorAddr.water_out);
    
    // Validate readings
    for(int i = 0; i < 5; i++) {
        temps.valid[i] = validateTemperature(temps[i]);
    }
    
    // Calculate delta-T
    if(temps.valid[3] && temps.valid[4]) {
        temps.water_delta = temps.water_out - temps.water_in;
    }
    
    return temps;
}
```

### 5.4 Water System Monitoring

```cpp
struct WaterSystemStatus {
    float delta_t;
    String flow_status;  // "OK", "WARNING", "CRITICAL"
    bool pump_should_run;
};

WaterSystemStatus checkWaterSystem(float delta_t) {
    WaterSystemStatus status;
    status.delta_t = delta_t;
    
    if(delta_t < 0.5) {
        status.flow_status = "CRITICAL";
        status.pump_should_run = false;
    } else if(delta_t < 1.0) {
        status.flow_status = "WARNING";
        status.pump_should_run = false;
    } else if(delta_t > 15.0) {
        status.flow_status = "ERROR";
        status.pump_should_run = false;
    } else {
        status.flow_status = "OK";
        status.pump_should_run = true;
    }
    
    return status;
}
```

### 5.5 Configuration Storage

**Stored in LittleFS (JSON format)**:

```json
{
  "wifi": {
    "ssid": "MyNetwork",
    "password": "encrypted"
  },
  "mqtt": {
    "broker": "192.168.1.100",
    "port": 1883,
    "username": "homeassistant",
    "password": "encrypted",
    "enabled": true
  },
  "sensors": {
    "floor": "28FF1234567890AB",
    "air": "28FF2345678901BC",
    "outdoor": "28FF3456789012CD",
    "water_in": "28FF4567890123DE",
    "water_out": "28FF5678901234EF",
    "calibration": {
      "floor": 0.0,
      "air": 0.0,
      "outdoor": 0.0,
      "water_in": 0.0,
      "water_out": 0.0
    }
  },
  "zones": {
    "floor": {
      "target": 5.0,
      "hysteresis": 2.0,
      "enabled": true
    },
    "air": {
      "target": 20.0,
      "hysteresis": 1.0,
      "enabled": true
    }
  },
  "water_monitoring": {
    "enabled": true,
    "delta_t_warning_low": 1.0,
    "delta_t_warning_high": 15.0,
    "smart_pump_control": false
  },
  "schedules": [...],
  "system": {
    "device_name": "Shop Thermostat",
    "timezone": "America/Winnipeg",
    "temp_unit": "C"
  }
}
```

### 5.6 Key Libraries

| Library | Purpose | Version |
|---------|---------|---------|
| ESP8266WiFi | WiFi connectivity | Core |
| ESPAsyncWebServer | Web interface | Latest |
| PubSubClient | MQTT client | 2.8+ |
| LiquidCrystal_I2C | LCD display | 1.1+ |
| DallasTemperature | DS18B20 sensors | 3.9+ |
| OneWire | 1-Wire protocol | 2.3+ |
| WiFiManager | WiFi configuration | 2.0+ |
| ArduinoJson | JSON parsing | 6.x |
| NTPClient | Time synchronization | 3.2+ |
| LittleFS | File storage | Core |

---

## 6. Home Assistant Integration

### 6.1 Climate Card Configuration

```yaml
# Automatically discovered, or manual configuration:
climate:
  - platform: mqtt
    name: "Shop Floor Heating"
    mode_command_topic: "homeassistant/climate/shop_thermostat/floor/mode/set"
    mode_state_topic: "homeassistant/climate/shop_thermostat/floor/mode"
    temperature_command_topic: "homeassistant/climate/shop_thermostat/floor/target/set"
    temperature_state_topic: "homeassistant/climate/shop_thermostat/floor/target"
    current_temperature_topic: "homeassistant/climate/shop_thermostat/floor/current"
    modes:
      - "off"
      - "heat"
    min_temp: 2
    max_temp: 15
```

### 6.2 Automation Examples

**Example 1: Preheat for Morning Work**
```yaml
automation:
  - alias: "Preheat Shop for Morning"
    trigger:
      - platform: time
        at: "07:00:00"
    condition:
      - condition: state
        entity_id: binary_sensor.workday_sensor
        state: "on"
    action:
      - service: climate.set_temperature
        target:
          entity_id: climate.shop_air_heating
        data:
          temperature: 20
      - service: climate.set_hvac_mode
        target:
          entity_id: climate.shop_air_heating
        data:
          hvac_mode: heat
```

**Example 2: Cold Weather Floor Boost**
```yaml
automation:
  - alias: "Boost Floor Heating in Extreme Cold"
    trigger:
      - platform: numeric_state
        entity_id: sensor.shop_outdoor_temperature
        below: -15
    action:
      - service: climate.set_temperature
        target:
          entity_id: climate.shop_floor_heating
        data:
          temperature: 8
```

**Example 3: Water System Health Alert**
```yaml
automation:
  - alias: "Alert: Low Water Tank Delta-T"
    trigger:
      - platform: numeric_state
        entity_id: sensor.shop_water_tank_delta_t
        below: 1.0
        for:
          minutes: 10
    condition:
      - condition: state
        entity_id: binary_sensor.shop_floor_relay
        state: "on"
    action:
      - service: notify.mobile_app
        data:
          title: "Shop Heating Issue"
          message: "Water tank not heating effectively. Delta-T: {{ states('sensor.shop_water_tank_delta_t') }}°C"
```

### 6.3 Template Sensors

**Water Tank Efficiency**
```yaml
sensor:
  - platform: template
    sensors:
      water_tank_efficiency:
        friendly_name: "Water Tank Efficiency"
        unit_of_measurement: "%"
        value_template: >
          {% set delta = states('sensor.shop_water_tank_delta_t') | float %}
          {% set max_delta = 15.0 %}
          {{ ((delta / max_delta) * 100) | round(1) }}
```

### 6.4 Lovelace Dashboard Card

```yaml
type: vertical-stack
cards:
  - type: thermostat
    entity: climate.shop_air_heating
    name: Shop Air Heater
  - type: thermostat
    entity: climate.shop_floor_heating
    name: Floor Heating
  - type: entities
    title: Water System
    entities:
      - entity: sensor.shop_water_tank_inlet
        name: Inlet Temperature
      - entity: sensor.shop_water_tank_outlet
        name: Outlet Temperature
      - entity: sensor.shop_water_tank_delta_t
        name: Delta-T
  - type: entities
    title: System Status
    entities:
      - entity: sensor.shop_outdoor_temperature
        name: Outdoor Temperature
      - entity: binary_sensor.shop_floor_relay
        name: Floor Pump
      - entity: binary_sensor.shop_heater_relay
        name: Electric Heater
      - entity: sensor.shop_wifi_rssi
        name: WiFi Signal
```

**History Graph for Water System**
```yaml
type: history-graph
title: Water Tank Performance (24h)
entities:
  - entity: sensor.shop_water_tank_inlet
    name: Inlet
  - entity: sensor.shop_water_tank_outlet
    name: Outlet
  - entity: sensor.shop_water_tank_delta_t
    name: Delta-T
hours_to_show: 24
```

---

## 7. Safety & Error Handling

### 7.1 Sensor Fault Detection

**Fault Conditions**:
- Temperature reading out of range (-55°C to +125°C)
- No data from sensor for 5 minutes
- CRC error on DS18B20 reading
- Sensor returns -127°C (typical error value)

**Actions on Fault**:
1. Log error to serial and MQTT
2. Display error on LCD: `SENSOR ERR: FLOOR`
3. Disable affected zone immediately
4. Send alert via MQTT
5. Flash LED or relay indicator
6. For critical sensors (floor/air), enter safe mode

### 7.2 Relay Protection

- **Minimum Off Time**: 5 minutes between cycles
- **Maximum On Time**: 4 hours continuous (configurable)
- **Cycle Counter**: Track relay cycles for maintenance prediction
- **Contact Wear Monitoring**: Alert after X cycles

### 7.3 Thermal Runaway Protection

**Floor Zone**:
- If floor temp exceeds 20°C, force pump OFF and disable zone
- Require manual reset via web interface or encoder
- Log event with timestamp

**Air Zone**:
- If air temp exceeds 30°C, force heater OFF and disable zone
- Require manual reset
- Log event with timestamp

**Water System**:
- Monitor for sensor failures that could indicate thermal issues
- If water temp exceeds 85°C, force pump OFF (boiling protection)

### 7.4 Network Loss Behavior

**WiFi Lost**:
- Continue local temperature control
- Display "WiFi: Lost" on LCD
- Continue using last known schedule
- Attempt reconnection every 30s
- Queue MQTT messages for later transmission

**MQTT Lost**:
- Continue all functions locally
- Display "MQTT: Lost" on LCD
- Queue state changes for republish when reconnected
- Attempt reconnection with exponential backoff (30s, 60s, 120s, max 300s)

### 7.5 Power Loss Recovery

**On Boot**:
- Load last configuration from flash
- Resume schedules immediately
- Sync time via NTP (use system clock until sync completes)
- Publish "online" status via MQTT
- Check all sensor health before enabling zones
- Validate relay states match expected states
- Log boot event with reason (power cycle, watchdog reset, etc.)

### 7.6 Water System Specific Safety

**Low Delta-T Protection**:
- If delta-T < 0.5°C for >10 minutes with pump running
- Disable pump to prevent energy waste
- Alert via MQTT
- Display warning on LCD

**High Delta-T Protection**:
- If delta-T > 15°C, likely sensor error
- Continue operation but flag error
- Log for diagnostics

**Flow Reversal Detection**:
- If delta-T consistently negative, possible plumbing issue
- Alert user via MQTT
- Display warning on LCD

---

## 8. Development Phases

### Phase 1: Core Hardware & Sensing (Week 1)
- [ ] Hardware assembly and wiring
- [ ] Temperature sensor integration (all 5 sensors)
- [ ] Sensor address discovery and mapping
- [ ] Relay control implementation
- [ ] Basic control logic (without schedules)
- [ ] Water delta-T calculation

### Phase 2: Local Interface (Week 2)
- [ ] LCD display implementation (all 6 modes)
- [ ] Rotary encoder menu system
- [ ] Local temperature adjustment
- [ ] Manual override controls
- [ ] Water system display modes

### Phase 3: WiFi & Web Interface (Week 3)
- [ ] WiFi connection management
- [ ] AP mode and captive portal
- [ ] Basic web dashboard
- [ ] Settings configuration page
- [ ] Water monitoring display on web

### Phase 4: Scheduling & Advanced Features (Week 4)
- [ ] NTP time synchronization
- [ ] Schedule system implementation
- [ ] Web schedule editor
- [ ] Schedule conflict resolution
- [ ] Smart pump control with water monitoring

### Phase 5: MQTT & Home Assistant (Week 5)
- [ ] MQTT client implementation
- [ ] Home Assistant discovery (climate + sensors)
- [ ] State publishing (including water sensors)
- [ ] Command handling
- [ ] Water system entity configuration

### Phase 6: Testing & Refinement (Week 6)
- [ ] Safety feature testing
- [ ] Error handling validation
- [ ] Water monitoring accuracy testing
- [ ] Performance optimization
- [ ] Documentation completion

---

## 9. Testing Requirements

### 9.1 Unit Testing

**Temperature Sensing**:
- Verify all five sensors readable
- Test sensor failure detection
- Validate calibration offsets
- Test sensor address mapping
- Verify delta-T calculation accuracy

**Control Logic**:
- Test hysteresis behavior
- Verify relay switching thresholds
- Validate safety limits
- Test smart pump control with various delta-T values

**Web Interface**:
- Test all pages load correctly
- Verify form submissions
- Test WebSocket updates
- Verify water monitoring display

### 9.2 Integration Testing

**WiFi Scenarios**:
- Fresh device (no config)
- Valid credentials
- Invalid credentials
- Network disappears during operation
- Network returns after loss

**MQTT Scenarios**:
- Broker offline at boot
- Broker disconnects during operation
- Command processing
- Discovery message validation
- Water sensor data publishing

**Schedule Testing**:
- Overlapping schedules
- Day transitions (midnight)
- DST transitions
- Manual override during schedule

**Water Monitoring Testing**:
- Various delta-T scenarios
- Sensor failure handling
- Smart pump control logic
- Alert threshold verification

### 9.3 Safety Testing

- Sensor disconnect during heating (all 5 sensors)
- Relay stuck closed detection
- Thermal runaway scenarios
- Power cycle during active heating
- Network loss during operation
- Water sensor failure scenarios
- Extreme delta-T values

### 9.4 Water System Testing

**Sensor Placement Validation**:
- Compare clamp-mounted vs. immersion readings
- Verify thermal paste application
- Test insulation effectiveness

**Delta-T Accuracy**:
- Test under various flow rates
- Verify against reference thermometer
- Test calibration adjustments

**Smart Control Logic**:
- Test pump control with low delta-T
- Verify energy savings
- Ensure no false triggers

---

## 10. Water Sensor Installation Guide

### 10.1 Mounting Options

**Option 1: Pipe Clamp Sensors (Easiest)**
- Use DS18B20 with stainless steel probe
- Attach to copper pipe with pipe clamp or hose clamp
- Apply thermal paste between sensor and pipe
- Insulate sensor with foam pipe insulation
- ⚠️ May read 2-3°C lower than actual water temp
- Best for: Monitoring trends, relative measurements

**Option 2: Immersion Well (Most Accurate)**
- Install thermowells in pipe using T-fittings
- Insert DS18B20 probe into thermowell
- Use thermal grease for thermal coupling
- Provides true water temperature
- ⚠️ Requires plumbing work and system drain
- Best for: Precise measurements, calibration reference

**Option 3: Compression Fitting Sensor (Professional)**
- Use threaded temperature sensor ports
- Requires 1/2" NPT or compression fittings
- Most accurate reading
- ⚠️ Requires draining system to install
- Best for: Permanent installation, highest accuracy

### 10.2 Recommended Sensor Locations

**Inlet Sensor (Water Supply to Tank)**:
- Location: On supply line to hot water tank, before any valves
- Purpose: Measure temperature of water entering heating system
- Mounting: 6-12 inches before tank inlet
- Orientation: Middle or top of pipe (avoid bottom where sediment settles)

**Outlet Sensor (Water Return from Floor)**:
- Location: On return line from floor heating, after circulation pump
- Purpose: Measure temperature of water returning from floor loops
- Mounting: 6-12 inches after pump discharge
- Orientation: Middle or top of pipe

**Installation Tips**:
- Keep sensors away from sharp bends or valves (turbulence affects reading)
- If using clamp sensors, clean pipe surface thoroughly
- Apply thermal paste generously
- Wrap sensor and pipe section with foam insulation tape
- Secure wiring to prevent sensor movement
- Label each sensor clearly

### 10.3 Wiring for Water Sensors

**1-Wire Bus Extension**:
```
ESP8266 GPIO14 ─┬─ 4.7kΩ ─ 3.3V
                │
                ├─ Floor Sensor
                ├─ Air Sensor
                ├─ Outdoor Sensor
                ├─ Water Inlet Sensor (can be 10m+ away)
                └─ Water Outlet Sensor (can be 10m+ away)
                
All sensors share common:
- Data line (GPIO14)
- GND
- VCC (3.3V or parasitic power)
```

**Cable Recommendations**:
- Use shielded Cat5e/Cat6 cable for long runs
- Maximum recommended distance: 30m per sensor
- For >30m runs, use 2.2kΩ pull-up resistor instead of 4.7kΩ
- Consider powered mode (3-wire) instead of parasitic mode for reliability

---

## 11. Documentation Deliverables

### 11.1 User Documentation
- Installation guide (wiring diagrams including water sensors)
- Water sensor mounting guide
- Quick start guide
- Web interface manual
- Home Assistant integration guide
- Water monitoring interpretation guide
- Troubleshooting guide

### 11.2 Technical Documentation
- Complete source code with comments
- API documentation (web endpoints, MQTT topics)
- Wiring schematics (including 5-sensor 1-Wire bus)
- Configuration file format specification
- Water system monitoring algorithm documentation

### 11.3 Maintenance Documentation
- Sensor calibration procedure (all 5 sensors)
- Water sensor cleaning and maintenance
- Relay replacement guide
- Firmware update procedure
- Backup/restore configuration
- Factory reset procedure

---

## 12. Future Enhancements (Out of Scope for v1.0)

### 12.1 Data Logging & Analysis
- Store temperature history to SD card
- Track water delta-T trends over time
- Generate performance reports
- Export data to CSV

### 12.2 Advanced Features
- **Energy Monitoring**: Track heater runtime and estimate costs
- **Weather Integration**: Use forecast for predictive heating
- **Voice Control**: Alexa/Google Home integration
- **Multiple Zones**: Support for additional heating zones
- **PID Control**: Advanced temperature regulation
- **Mobile App**: Dedicated smartphone application
- **Geofencing**: Auto-adjust based on phone location
- **Machine Learning**: Learn usage patterns for optimization

### 12.3 Water System Enhancements
- **Flow Rate Calculation**: Estimate GPM based on delta-T and power
- **Sediment Detection**: Analyze delta-T degradation over time
- **Predictive Maintenance**: Alert before tank failure
- **Pump Efficiency Monitoring**: Track pump performance
- **Automatic Calibration**: Self-calibrate sensors using known conditions

---

## 13. Bill of Materials (BOM)

| Item | Quantity | Est. Cost (USD) | Notes |
|------|----------|-----------------|-------|
| Wemos D1 Mini | 1 | $4 | ESP8266 module |
| **DS18B20 Temperature Sensors** | **5** | **$10** | **Waterproof probes** |
| 2-Channel Relay Module | 1 | $3 | 5V trigger, 10A contacts |
| 16x2 LCD I2C Module | 1 | $4 | HD44780 with PCF8574 |
| Rotary Encoder with Button | 1 | $2 | KY-040 or similar |
| 4.7kΩ Resistor | 1 | $0.10 | 1-Wire pullup |
| 5V Power Supply | 1 | $5 | 2A minimum |
| Enclosure | 1 | $8 | ABS project box |
| Thermal Paste | 1 | $3 | For water sensors |
| Pipe Clamps | 2 | $2 | For water sensors |
| Foam Pipe Insulation | 1m | $2 | For water sensors |
| Misc (wire, terminals, etc.) | - | $5 | Connectors, heat shrink |
| **Total** | | **~$48** | |

**Optional Upgrades**:
- Thermowells for water sensors: +$20-40
- Larger enclosure with DIN rail: +$15
- External antenna for ESP8266: +$5
- UPS/battery backup: +$20-50

**Note**: Does not include electrical installation costs for heater/pump circuits or plumbing work.

---

## 14. Compliance & Safety Notes

### 14.1 Electrical Safety
⚠️ **WARNING**: This device controls high-voltage heating equipment. Installation must comply with local electrical codes and should be performed by a licensed electrician.

- Use appropriately rated contactors for 6kW heater (minimum 30A @ 240V)
- Ensure proper grounding of all metal enclosures
- Use strain relief for all wire connections
- Install GFCI protection where required by code
- Follow NEC Article 424 for electric space heating equipment

### 14.2 Plumbing Safety
⚠️ **WARNING**: Improper installation of water sensors can cause leaks or system damage.

- Use thread sealant appropriate for potable water if using threaded sensors
- Do not over-tighten clamp-mounted sensors (can crack pipes)
- Ensure all connections are pressure-tested
- Install sensors downstream of pressure relief valve
- Follow local plumbing codes

### 14.3 Temperature Sensor Placement
- **Floor Sensor**: Embed in thermal mass, not in conduit, minimum 6" from heat source
- **Air Sensor**: Mount away from heater discharge, at breathing height (4-5 feet)
- **Outdoor Sensor**: North-facing wall, shaded location, protected from rain
- **Water Inlet**: Before tank, in straight pipe section, insulated
- **Water Outlet**: After pump, in straight pipe section, insulated

### 14.4 FCC/CE Considerations
- ESP8266 module must be FCC/CE certified
- Enclosure should not interfere with WiFi antenna
- Consider EMI from relay switching (use RC snubbers if needed)
- Maintain proper spacing between low-voltage and line-voltage wiring

### 14.5 Water System Considerations
- Ensure water sensors are rated for maximum system temperature (typically 180°F / 82°C)
- Use waterproof DS18B20 sensors rated for immersion
- Verify sensor cable insulation is rated for ambient temperature
- Consider thermal expansion when mounting sensors

---

## 15. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01-25 | Initial | Complete FSD created |
| 1.1 | 2026-01-25 | Updated | Added water tank monitoring (2 additional sensors) |
| 1.2 | 2026-02-01 | Updated | Pin reassignment: encoder to D4/D6, button to A0, pump relay to D0 |

---

## 16. Appendices

### Appendix A: Example MQTT Messages

**Temperature Update (Floor)**:
```
Topic: homeassistant/climate/shop_thermostat/floor/current
Payload: 5.2
```

**Temperature Update (Water Inlet)**:
```
Topic: homeassistant/climate/shop_thermostat/water/inlet
Payload: 45.2
```

**Water Delta-T Update**:
```
Topic: homeassistant/climate/shop_thermostat/water/delta
Payload: 2.4
```

**Water Flow Status**:
```
Topic: homeassistant/climate/shop_thermostat/water/flow_status
Payload: OK
```

**Mode Change Command**:
```
Topic: homeassistant/climate/shop_thermostat/air/mode/set
Payload: heat
```

**Discovery Message (Water Inlet Sensor)**:
```json
{
  "name": "Shop Water Tank Inlet",
  "unique_id": "shop_thermo_water_in_abc123",
  "state_topic": "homeassistant/climate/shop_thermostat/water/inlet",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "device": {
    "identifiers": ["shop_thermostat_abc123"],
    "name": "Shop Thermostat",
    "model": "ESP8266 Dual Zone v1.1",
    "manufacturer": "DIY"
  },
  "availability_topic": "homeassistant/climate/shop_thermostat/status",
  "payload_available": "online",
  "payload_not_available": "offline"
}
```

**Discovery Message (Floor Climate)**:
```json
{
  "name": "Shop Floor Heating",
  "unique_id": "shop_thermo_floor_abc123",
  "device": {
    "identifiers": ["shop_thermostat_abc123"],
    "name": "Shop Thermostat",
    "model": "ESP8266 Dual Zone v1.1",
    "manufacturer": "DIY"
  },
  "availability_topic": "homeassistant/climate/shop_thermostat/status",
  "payload_available": "online",
  "payload_not_available": "offline",
  "mode_command_topic": "~/floor/mode/set",
  "mode_state_topic": "~/floor/mode",
  "temperature_command_topic": "~/floor/target/set",
  "temperature_state_topic": "~/floor/target",
  "current_temperature_topic": "~/floor/current",
  "modes": ["off", "heat"],
  "min_temp": 2,
  "max_temp": 15,
  "temp_step": 0.5,
  "temperature_unit": "C"
}
```

### Appendix B: Web API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Dashboard page |
| `/api/status` | GET | JSON status of all systems |
| `/api/temps` | GET | Current temperature readings (all 5 sensors) |
| `/api/water` | GET | Water system status (inlet, outlet, delta-T, flow status) |
| `/api/config` | GET/POST | Get/set configuration |
| `/api/schedule` | GET/POST | Get/set schedules |
| `/api/override` | POST | Set manual override |
| `/settings` | GET/POST | Settings page |
| `/schedule` | GET/POST | Schedule editor page |
| `/mqtt` | GET/POST | MQTT configuration page |
| `/wifi` | GET/POST | WiFi configuration page |
| `/reboot` | POST | Reboot device |

**Example API Response (/api/temps)**:
```json
{
  "floor": 5.2,
  "air": 18.5,
  "outdoor": -2.3,
  "water_inlet": 45.2,
  "water_outlet": 42.8,
  "water_delta": 2.4,
  "timestamp": "2026-01-25T14:30:00Z"
}
```

**Example API Response (/api/water)**:
```json
{
  "inlet_temp": 45.2,
  "outlet_temp": 42.8,
  "delta_t": 2.4,
  "flow_status": "OK",
  "pump_runtime_today": 12600,
  "warnings": [],
  "timestamp": "2026-01-25T14:30:00Z"
}
```

### Appendix C: Sensor Discovery Code Example

```cpp
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 14  // GPIO14 (D5)

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void discoverSensors() {
    Serial.println("=== DS18B20 Sensor Discovery ===");
    
    sensors.begin();
    int deviceCount = sensors.getDeviceCount();
    
    Serial.print("Found ");
    Serial.print(deviceCount);
    Serial.println(" sensors on the bus.");
    Serial.println();
    
    DeviceAddress tempAddress;
    for(int i = 0; i < deviceCount; i++) {
        if(sensors.getAddress(tempAddress, i)) {
            Serial.print("Sensor ");
            Serial.print(i);
            Serial.print(" Address: ");
            printAddress(tempAddress);
            
            // Request temperature
            sensors.requestTemperatures();
            float tempC = sensors.getTempC(tempAddress);
            
            Serial.print(" | Current Temp: ");
            Serial.print(tempC);
            Serial.println("°C");
            
            Serial.println("  To identify: heat/cool this sensor and watch temp change");
            Serial.println();
        }
    }
}

void printAddress(DeviceAddress deviceAddress) {
    for (uint8_t i = 0; i < 8; i++) {
        if (deviceAddress[i] < 16) Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    discoverSensors();
}

void loop() {
    // Continuous monitoring for identification
    sensors.requestTemperatures();
    
    DeviceAddress tempAddress;
    int deviceCount = sensors.getDeviceCount();
    
    Serial.println("--- Current Readings ---");
    for(int i = 0; i < deviceCount; i++) {
        if(sensors.getAddress(tempAddress, i)) {
            float tempC = sensors.getTempC(tempAddress);
            Serial.print("Sensor ");
            Serial.print(i);
            Serial.print(": ");
            Serial.print(tempC);
            Serial.println("°C");
        }
    }
    Serial.println();
    
    delay(2000);  // Update every 2 seconds
}
```

**Sensor Identification Process**:
1. Upload discovery sketch
2. Open Serial Monitor (115200 baud)
3. Note all sensor addresses
4. Heat sensor #1 with your hand - watch which temperature rises
5. Label that sensor and record its address
6. Repeat for all 5 sensors
7. Update configuration with correct addresses

### Appendix D: Wiring Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                     Wemos D1 Mini                           │
│                                                             │
│  3.3V ─────┬──────────────────┬─────────────────────────── │
│           │                  │                             │
│           │              4.7kΩ Pull-up                     │
│           │                  │                             │
│  D5 ──────┼──────────────────┴──┬──┬──┬──┬──┬─── 1-Wire   │
│  (GPIO14) │                     │  │  │  │  │     Bus      │
│           │                     │  │  │  │  │              │
│  GND ─────┴─────────────────────┴──┴──┴──┴──┴───────────── │
│                                 │  │  │  │  │              │
│                                 │  │  │  │  │              │
│  D2 (GPIO4) ─────────────── SDA (LCD I2C)                  │
│  D1 (GPIO5) ─────────────── SCL (LCD I2C)                  │
│                                                             │
│  D0 (GPIO16) ────────────── Relay 1 (Floor Pump)           │
│  D7 (GPIO13) ────────────── Relay 2 (Electric Heater)      │
│                                                             │
│  D4 (GPIO2) ─────────────── Encoder A (interrupt)          │
│  D6 (GPIO12) ────────────── Encoder B                      │
│  A0 ─────────────────────── Encoder Button (analogRead)    │
│                                                             │
│  5V ────────────────────────── Power Supply +5V            │
│  GND ───────────────────────── Power Supply GND            │
└─────────────────────────────────────────────────────────────┘
         │  │  │  │  │
         │  │  │  │  └── DS18B20 #5 (Water Out)
         │  │  │  └───── DS18B20 #4 (Water In)
         │  │  └──────── DS18B20 #3 (Outdoor)
         │  └─────────── DS18B20 #2 (Air)
         └────────────── DS18B20 #1 (Floor)

Each DS18B20 has 3 wires:
- Red/Brown: VCC (3.3V)
- Black: GND
- Yellow/White: Data (GPIO14 with 4.7kΩ pullup to 3.3V)
```

---

## 17. Quick Reference Guide

### 17.1 Default Settings

| Parameter | Default Value |
|-----------|--------------|
| Floor Target | 5°C |
| Air Target | 20°C |
| Floor Hysteresis | 2°C |
| Air Hysteresis | 1°C |
| Water Delta-T Warning (Low) | 1.0°C |
| Water Delta-T Warning (High) | 15.0°C |
| Max Runtime | 4 hours |
| Min Cycle Time | 5 minutes |
| AP Mode Password | thermostat123 |
| MQTT Port | 1883 |

### 17.2 LCD Button Functions

| Action | Function |
|--------|----------|
| Single Press | Cycle display modes |
| Long Press (3s) | Enter menu |
| Very Long Press (10s) | Enter AP mode |
| Rotate Left/Right | Navigate/adjust |
| Press in Menu | Select/confirm |

### 17.3 Common MQTT Topics

**Subscribe to for monitoring**:
- `homeassistant/climate/shop_thermostat/+/current`
- `homeassistant/climate/shop_thermostat/water/#`
- `homeassistant/climate/shop_thermostat/status`

**Publish to for control**:
- `homeassistant/climate/shop_thermostat/floor/target/set`
- `homeassistant/climate/shop_thermostat/air/target/set`
- `homeassistant/climate/shop_thermostat/floor/mode/set`
- `homeassistant/climate/shop_thermostat/air/mode/set`

### 17.4 Troubleshooting Quick Checks

**No temperature readings**:
1. Check 4.7kΩ pull-up resistor
2. Verify sensor wiring (VCC, GND, Data)
3. Run sensor discovery code
4. Check for loose connections

**WiFi won't connect**:
1. Hold button 10 seconds for AP mode
2. Connect to ShopThermostat-XXXX
3. Navigate to 192.168.4.1
4. Re-enter WiFi credentials

**MQTT not working**:
1. Verify broker IP/port
2. Check username/password
3. Ensure firewall allows port 1883
4. Test connection from web interface

**Water monitoring shows WARNING**:
1. Check both sensors are reading correctly
2. Verify pump is running
3. Inspect pipe insulation on sensors
4. Check for air in system
5. Verify correct sensor placement

**Relay not switching**:
1. Check relay module power (5V)
2. Verify GPIO connections
3. Test relay manually via web interface
4. Check for loose wiring
5. Replace relay module if faulty

---

**End of Functional Specification Document**

This comprehensive FSD provides complete specifications for developing your five-sensor dual-zone shop thermostat with water system monitoring. The addition of water inlet/outlet sensors enables sophisticated diagnostics and efficiency monitoring while requiring no additional GPIO pins on the ESP8266. The system maintains backward compatibility with the original design while adding valuable water system health monitoring capabilities.
