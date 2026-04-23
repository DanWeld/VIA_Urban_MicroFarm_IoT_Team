# Urban Micro Farming - IoT Firmware

Embedded C firmware for the Urban Micro Farming IoT devices. Communicates with Mosquitto MQTT broker to integrate with the Spring Boot backend.

## Tech Stack

- **C Language** (embedded firmware)
- **PlatformIO** (IDE + build system)
- **MQTT Protocol** (communication with broker)
- **Mosquitto Broker** (MQTT message broker)
- **Arduino Mega2560** (primary microcontroller)
- **ESP8266** (WiFi module)
- **Native Testing** (C unit tests)

## Hardware Setup

- Arduino Mega2560 board
- Arduino Uno Multifunctional Expansion Shield
- VIA University expansion board
- ESP8266 WiFi module (WiFi + MQTT connectivity)
- HC-SR501 PIR sensor (motion detection)
- HC-SR04 Proximity sensor (ultrasonic distance)
- KY-018 LDR module (light sensor)
- DHT11 temperature & humidity sensor
- Capacitive soil moisture sensor
- SG90 Servo (motor control)

See detailed hardware documentation in the `doc/` folder.

---

## Prerequisites

### Local Development
- **PlatformIO CLI** installed (`pip install platformio`)
- **Python 3.9+** (for PlatformIO)
- **C compiler** (GCC for native testing)
- **Git** for version control
- **Serial monitor tool** (for USB debugging)

### Docker (Mosquitto Broker + Testing)
- **Docker Engine** installed
- **Docker Compose** plugin installed

---

## Build, Test, Run (Local)

### 1. Clone & Setup
```bash
cd /path/to/UrbanMicroFarm_IoT
git clone <repo-url>
```

### 2. Build Firmware
```bash
# Compile for Arduino Mega2560
pio run

# Compile for specific environment
pio run -e mega2560
```

### 3. Run Unit Tests
```bash
# Run native C tests
pio test -e native
```

### 4. Upload to Hardware
```bash
# Build and upload to connected device
pio run -t upload -e mega2560
```

### 5. Monitor Serial Output
```bash
# Open serial monitor (115200 baud)
pio device monitor --baud 115200
```

---

## Docker - Mosquitto Broker + Device Simulation

### Setup Files Required
Place these files in the project root (same level as `platformio.ini`):

- `Dockerfile` (optional: for device simulation)
- `.dockerignore`
- `docker-compose.yml` (Mosquitto broker)
- `.env` (copy from `.env.example`)

### 1. Prepare Environment File
```bash
cd /path/to/UrbanMicroFarm_IoT
cp .env.example .env
# Edit .env with your MQTT broker settings
```

### 2. Start Mosquitto Broker
```bash
docker compose build
docker compose up -d
```

### 3. Check Status & Logs
```bash
# Show running containers
docker compose ps

# View broker logs
docker compose logs -f mosquitto

# View device simulation logs (if running)
docker compose logs -f iot-device
```

### 4. Publish Test Message to Broker
```bash
# From host machine (requires mosquitto-clients installed)
mosquitto_pub -h localhost -t "farm/sensor/temperature" -m "25.5"

# Subscribe to all topics
mosquitto_sub -h localhost -t "farm/#"
```

### 5. Stop Containers
```bash
docker compose down
```

### 6. Full Reset (Delete Volumes)
```bash
docker compose down -v
```

---

## What Gets Created

### Docker Artifacts
- **Image**: `urbanmicrofarm-iot:dev` (device simulator)
- **Container**: `urbanmicrofarm-mosquitto` (MQTT broker)
- **Container**: `urbanmicrofarm-iot` (firmware simulation/testing)
- **Network**: `urbanmicrofarm-net` (internal communication)

### Firmware Artifacts
- `.pio/build/mega2560/firmware.hex` (compiled firmware)
- `.pio/build/native/program` (native test executable)
- `lib/drivers/` (compiled driver libraries)

---

## MQTT Topics & Message Format

### Published by IoT Devices (to backend)

| Topic | Payload | Interval |
|-------|---------|----------|
| `farm/sensor/temperature` | `{"value": 25.5, "unit": "C"}` | 60s |
| `farm/sensor/humidity` | `{"value": 65.2, "unit": "%"}` | 60s |
| `farm/sensor/soil_moisture` | `{"value": 450, "unit": "raw"}` | 120s |
| `farm/sensor/light` | `{"value": 500, "unit": "lux"}` | 120s |
| `farm/sensor/motion` | `{"detected": true, "timestamp": "2026-04-16T10:30:00Z"}` | Event |
| `farm/device/status` | `{"device_id": "ESP01-001", "uptime": 3600}` | 300s |

### Subscribed by IoT Devices (from backend)

| Topic | Payload | Action |
|-------|---------|--------|
| `farm/actuator/servo` | `{"angle": 90, "duration": 2000}` | Rotate servo |
| `farm/actuator/buzzer` | `{"frequency": 1000, "duration": 500}` | Sound buzzer |
| `farm/config/update` | `{"param": "sensor_interval", "value": 30}` | Update firmware config |

---

## MQTT Connection Configuration

Reference the `.env` file for broker settings:

```
MQTT_BROKER_HOST=mosquitto
MQTT_BROKER_PORT=1883
MQTT_CLIENT_ID=esp8266-farm-001
MQTT_KEEPALIVE=60
MQTT_QoS=1
```

These are used by the WiFi/MQTT driver (`lib/drivers/wifi.c`).

---

## Documentation

- `doc/HARDWARE.md` - Hardware pin mappings and schematics
- `doc/MQTT_PROTOCOL.md` - MQTT protocol specification
- `test/` - Unit tests and test fixtures
- `lib/drivers/` - Driver implementations with comments

---

## Team Roles

| Role | Responsibility |
|------|-----------------|
| **Lead** | Merge PRs, manage releases, MQTT protocol specs |
| **Developer** | Create features, implement drivers, submit PRs |
| **Reviewer** | Code review, hardware testing |
| **Hardware Tester** | Validate on physical devices before main merge |

---

## Quick Start Checklist

- [ ] Clone repository
- [ ] Install PlatformIO: `pip install platformio`
- [ ] Build firmware: `pio run`
- [ ] Run tests: `pio test -e native`
- [ ] Start Mosquitto: `docker compose up -d mosquitto`
- [ ] Test MQTT connection
- [ ] Upload to hardware: `pio run -t upload`

Ready to code? Create a feature branch and start developing!

```bash
git checkout -b feature/mqtt-temperature-sensor
```
