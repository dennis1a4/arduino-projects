# Smart Dual-Zone Shop Thermostat

ESP8266-based dual-zone thermostat system for workshop environments with Home Assistant integration.

## Features

- **Dual-Zone Control**: Independent control of floor heating (pump) and electric heater
- **5 Temperature Sensors**: Floor, air, outdoor, water tank inlet/outlet (DS18B20)
- **Water System Monitoring**: Delta-T calculation, flow status, smart pump control
- **Local Interface**: 16x2 LCD display with rotary encoder navigation
- **Web Interface**: Responsive dashboard for configuration and monitoring
- **Home Assistant Integration**: MQTT with auto-discovery for climate entities
- **Scheduling**: 7 schedule slots with day/time/zone configuration
- **Safety Features**: Thermal runaway protection, sensor fault detection, runtime limits

## Hardware

| Component | Specification |
|-----------|--------------|
| Microcontroller | Wemos D1 Mini (ESP8266) |
| Temperature Sensors | DS18B20 waterproof (x5) |
| Relay Module | 2-channel 5V/10A |
| Display | 16x2 LCD with I2C backpack |
| Input | Rotary encoder with push button |

### Pin Assignments

| GPIO | Function |
|------|----------|
| D1 (GPIO5) | I2C SCL (LCD) |
| D2 (GPIO4) | I2C SDA (LCD) |
| D5 (GPIO14) | 1-Wire Bus (all sensors) |
| D0 (GPIO16) | Relay 1 - Floor Pump |
| D7 (GPIO13) | Relay 2 - Electric Heater |
| D4 (GPIO2) | Encoder A (interrupt, INPUT_PULLUP) |
| D6 (GPIO12) | Encoder B (INPUT_PULLUP) |
| A0 | Encoder Button (analogRead) |

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
# Compile
pio run

# Upload firmware
pio run -t upload

# Upload web interface to LittleFS
pio run -t uploadfs
```

## First-Time Setup

1. On first boot, the device creates an AP: `ShopThermostat-XXXX`
2. Connect and navigate to `192.168.4.1`
3. Configure WiFi credentials
4. DS18B20 sensors are auto-discovered and assigned on boot (first sensor → floor, second → air, etc.)
5. Configure MQTT broker if using Home Assistant

## LCD Display Modes

Press the encoder button to cycle through:

1. **Temperatures** - Floor/Air temps with relay status
2. **Targets** - Target temps and WiFi/MQTT status
3. **Water Temps** - Tank inlet/outlet temperatures
4. **Water Delta** - Delta-T and flow status
5. **Schedule** - Active schedule information
6. **System** - IP address and uptime

## Controls

| Action | Function |
|--------|----------|
| Rotate | Navigate/adjust values |
| Short Press | Cycle display / Select in menu |
| Long Press (3s) | Enter/exit menu |
| Very Long Press (10s) | Enter AP mode |

## Web Interface

- `/` - Dashboard with temperatures, zone control, water monitoring
- `/settings` - Zone targets, hysteresis, calibration
- `/schedule` - Schedule management
- `/mqtt` - MQTT broker configuration
- `/wifi` - WiFi settings and network scanner
- `/system` - Device settings, reboot

## MQTT Topics

Base topic: `homeassistant/climate/shop_thermostat`

| Topic | Description |
|-------|-------------|
| `~/floor/current` | Floor temperature |
| `~/air/current` | Air temperature |
| `~/outdoor/current` | Outdoor temperature |
| `~/water/inlet` | Tank inlet temperature |
| `~/water/outlet` | Tank outlet temperature |
| `~/water/delta` | Temperature difference |
| `~/floor/target/set` | Set floor target |
| `~/air/target/set` | Set air target |
| `~/floor/mode/set` | Set floor mode (heat/off) |
| `~/air/mode/set` | Set air mode (heat/off) |

## Safety Features

- **Thermal Runaway**: Auto-shutoff if floor >20°C or air >30°C
- **Sensor Fault**: Zone disabled after 5 minutes of invalid readings
- **Max Runtime**: 4 hours continuous operation limit
- **Min Cycle Time**: 5 minutes between relay state changes
- **Low Delta-T**: Smart pump control prevents pumping cold water

## Configuration

Settings stored in LittleFS as `/config.json`. Includes:
- WiFi credentials
- MQTT settings
- Zone targets and hysteresis
- Sensor addresses and calibration
- Schedules

## License

MIT
