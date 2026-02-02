# Shop Thermostat Quick Reference

## Pin Assignments

```
WEMOS D1 MINI
+--[USB]--+
|         |
| D0  RLY |-----> Floor Pump Relay
| D1  SCL |-----> LCD Clock
| D2  SDA |-----> LCD Data
| D4  ENC |-----> Encoder A (interrupt, INPUT_PULLUP)
| D5  1W  |-----> All Temp Sensors (+ 4.7k pullup to 3.3V)
| D6  ENC |-----> Encoder B (INPUT_PULLUP)
| D7  RLY |-----> Heater Relay
| A0  BTN |-----> Encoder Button (analogRead, needs pull-up to 3.3V)
|         |
| 5V  GND |-----> Power & Ground
+---------+
```

## Encoder Controls

| Action | How | Function |
|--------|-----|----------|
| **Cycle Display** | Short press | Switch between 6 display modes |
| **Enter Menu** | Hold 3 sec | Access settings menu |
| **Exit Menu** | Hold 3 sec | Return to display, save changes |
| **Navigate** | Rotate | Move through menu items |
| **Adjust Value** | Rotate (in edit) | Change temperature/setting |
| **Select/Confirm** | Short press | Enter edit mode or confirm |
| **AP Mode** | Hold 10 sec | Start WiFi configuration AP |

## Display Modes

```
1. TEMPS        2. TARGETS      3. WATER TEMPS
F:5.2° A:18.5°  T:5/20° O:-2°   Tank In:  45.2°C
P:ON H:OFF      WiFi:OK MQTT:OK Tank Out: 42.8°C

4. WATER DELTA  5. SCHEDULE     6. SYSTEM
dT:2.4° Flow:OK Sched: ACTIVE   IP:192.168.1.50
Pump: 3h 24m    08:00-17:00 20° Up: 5d 3h 22m
```

## Menu Structure

```
> Floor Target     [2-15°C]
> Air Target       [10-25°C]
> Floor Override   [Auto/On/Off]
> Air Override     [Auto/On/Off]
> WiFi Info
> MQTT Status
> Reboot System
```

## Status Indicators

| Display | Meaning |
|---------|---------|
| `P:ON` | Floor pump running |
| `H:ON` | Electric heater running |
| `Flow:OK` | Water circulation normal |
| `Flow:WARNING` | Low delta-T (<1°C) |
| `Flow:CRITICAL` | Very low delta-T (<0.5°C) |

## Default Settings

| Parameter | Value |
|-----------|-------|
| Floor Target | 5°C |
| Air Target | 18°C |
| Floor Hysteresis | 2°C |
| Air Hysteresis | 1°C |
| Max Runtime | 4 hours |
| AP Password | `thermostat123` |

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No WiFi | Hold encoder 10 sec for AP mode |
| Sensor ERR | Check wiring, verify 4.7kΩ pullup on D5. Sensors auto-discover on boot with retries |
| Relay stuck | Check web interface, try reboot |
| RUNAWAY | Reset via web interface after cooling |

## Web Access

- **Normal Mode**: `http://<device-ip>/`
- **AP Mode**: `http://192.168.4.1/`
- **AP SSID**: `ShopThermostat-XXXX`
