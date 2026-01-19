# Reflow Oven Controller

ESP8266-based toaster oven controller for reflow soldering and filament drying.

## Features

- **Reflow soldering mode** - programmable temperature profile
- **Filament drying mode** - presets for PLA, PETG, ABS, Nylon, TPU
- **OLED display** - real-time status, temperature, timers
- **Rotary encoder + button** - menu navigation and control
- **Web interface** - dashboard, settings, live temperature
- **MQTT integration** - Home Assistant compatible
- **PID temperature control** - separate tuning for each mode
- **Safety features** - thermal runaway detection, hardware cutoffs, watchdog

## Hardware

| Component | Connection |
|-----------|------------|
| Wemos D1 Mini | ESP8266 |
| SSD1306 OLED 128x64 | I2C (D1/D2) |
| AD8495 + K-type thermocouple | A0 |
| Rotary encoder | D5/D6 |
| Push button | D7 |
| SSR | D8 |

## Specs

### Temperature Limits
- Reflow max: 260°C
- Filament max: 100°C

### Reflow Profile (Leaded Sn63/Pb37)
- Preheat: 130°C
- Soak: 130-160°C for 90s
- Peak: 210°C for 45s
- Timeout: 10 min

### Filament Presets
| Type | Temp | Duration |
|------|------|----------|
| PLA | 45°C | 4 hrs |
| PETG | 65°C | 4 hrs |
| ABS | 70°C | 4 hrs |
| Nylon | 80°C | 6 hrs |
| TPU | 50°C | 4 hrs |

### Network
- WiFi with AP fallback mode
- MQTT with auto-reconnect
- Web server on port 80

## Web Endpoints

- `/` - Dashboard
- `/filament` - Filament settings
- `/setup` - Network configuration
- `/api/status` - JSON status
- `/api/filament` - Filament settings API
- `/api/set_mode` - Set operating mode
- `/api/set_temp` - Adjust temperature
- `/api/stop` - Stop operation

## MQTT Topics

Base: `homeassistant/toaster_oven`

- `/status` - JSON status
- `/command/start` - Start operation
- `/command/stop` - Stop operation
- `/command/set_mode` - Set mode (reflow/filament)
- `/command/filament/temp` - Set filament temp
- `/command/filament/duration` - Set duration
- `/command/filament/type` - Set preset type

## Safety

- 5-second button hold required to start from panel
- Mode must be set before starting
- Remote start disabled (local confirmation required)
- Thermal runaway detection
- Hardware watchdog (8s)
- Automatic timeout shutdown
