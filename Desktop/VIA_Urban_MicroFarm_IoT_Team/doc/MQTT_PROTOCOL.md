# MQTT Protocol Specification

## Overview
The Urban Micro Farming IoT system uses MQTT (Message Queuing Telemetry Transport) to communicate between ESP8266/Arduino devices and the Spring Boot backend via Mosquitto broker.

**Protocol Version:** MQTT 3.1.1  
**Broker:** Mosquitto (Docker container)  
**QoS Level:** 1 (At least once delivery)

---

## Topic Structure

### Topic Naming Convention
```
farm/{category}/{device_type}/{sensor_name}
```

**Categories:**
- `sensor` - Sensor readings from devices
- `actuator` - Actuator commands (LED, servo, buzzer)
- `device` - Device status and lifecycle
- `config` - Configuration updates
- `diagnostic` - Debug and diagnostic data

---

## Published Topics (Device → Broker → Backend)

### Temperature Sensor
**Topic:** `farm/sensor/temperature`  
**Interval:** 60 seconds  
**Payload:**
```json
{
  "device_id": "esp8266_001",
  "sensor_type": "DHT11",
  "value": 25.5,
  "unit": "C",
  "timestamp": "2026-04-16T10:30:00Z",
  "quality": 0.95
}
```

### Humidity Sensor
**Topic:** `farm/sensor/humidity`  
**Interval:** 60 seconds  
**Payload:**
```json
{
  "device_id": "esp8266_001",
  "sensor_type": "DHT11",
  "value": 65.2,
  "unit": "%",
  "timestamp": "2026-04-16T10:30:00Z"
}
```

### Soil Moisture Sensor
**Topic:** `farm/sensor/soil_moisture`  
**Interval:** 120 seconds  
**Payload:**
```json
{
  "device_id": "arduino_mega_001",
  "sensor_type": "Capacitive Moisture",
  "value": 450,
  "unit": "raw",
  "min": 200,
  "max": 800,
  "timestamp": "2026-04-16T10:30:00Z"
}
```

### Light Sensor
**Topic:** `farm/sensor/light`  
**Interval:** 120 seconds  
**Payload:**
```json
{
  "device_id": "esp8266_001",
  "sensor_type": "LDR",
  "value": 500,
  "unit": "lux",
  "timestamp": "2026-04-16T10:30:00Z"
}
```

### PIR Motion Sensor
**Topic:** `farm/sensor/motion`  
**Interval:** On event  
**Payload:**
```json
{
  "device_id": "esp8266_001",
  "sensor_type": "PIR",
  "detected": true,
  "timestamp": "2026-04-16T10:30:15Z"
}
```

### Proximity/Distance Sensor
**Topic:** `farm/sensor/proximity`  
**Interval:** On event (when distance changes)  
**Payload:**
```json
{
  "device_id": "arduino_mega_001",
  "sensor_type": "HC-SR04",
  "distance": 45.2,
  "unit": "cm",
  "obstacle_detected": false,
  "timestamp": "2026-04-16T10:30:00Z"
}
```

### Device Status
**Topic:** `farm/device/status`  
**Interval:** 5 minutes (300 seconds)  
**Payload:**
```json
{
  "device_id": "esp8266_001",
  "uptime": 3600,
  "status": "online",
  "rssi": -65,
  "ip_address": "192.168.1.100",
  "firmware_version": "1.0.0",
  "free_memory": 45000,
  "timestamp": "2026-04-16T10:30:00Z"
}
```

---

## Subscribed Topics (Backend → Broker → Device)

### Servo Actuator Command
**Topic:** `farm/actuator/servo`  
**Payload:**
```json
{
  "device_id": "arduino_mega_001",
  "angle": 90,
  "speed": 100,
  "duration": 2000,
  "timestamp": "2026-04-16T10:30:00Z"
}
```
**Response:** Device acknowledges on `farm/actuator/servo/ack`

### LED Control
**Topic:** `farm/actuator/led`  
**Payload:**
```json
{
  "device_id": "esp8266_001",
  "state": "on",
  "brightness": 255,
  "color": "#FF0000",
  "duration": 5000
}
```

### Buzzer/Tone
**Topic:** `farm/actuator/buzzer`  
**Payload:**
```json
{
  "device_id": "arduino_mega_001",
  "frequency": 1000,
  "duration": 500,
  "intensity": 100
}
```

### Configuration Update
**Topic:** `farm/config/update`  
**Payload:**
```json
{
  "device_id": "esp8266_001",
  "config": {
    "sensor_interval": 30,
    "mqtt_keepalive": 60,
    "log_level": "INFO"
  },
  "timestamp": "2026-04-16T10:30:00Z"
}
```

---

## QoS (Quality of Service) Levels

- **QoS 0:** At most once (no guarantee) - Used for non-critical sensor readings
- **QoS 1:** At least once (guaranteed) - Used for configuration updates and critical commands
- **QoS 2:** Exactly once (highest guarantee) - Currently not used to reduce latency

**Default:** QoS 1 for all Urban Micro Farming messages

---

## Payload Format Standards

### All Payloads MUST Include:
1. `device_id` - Unique identifier (e.g., `esp8266_001`)
2. `timestamp` - ISO 8601 format (UTC): `2026-04-16T10:30:00Z`
3. Actual data (sensor value, status, etc.)
4. `unit` - Unit of measurement (if applicable)

### Optional Fields:
- `sensor_type` - Hardware sensor model
- `quality` - Data quality metric (0-1)
- `error` - Error message if applicable

---

## Connection Settings

```
Broker Address: mosquitto (Docker) or MQTT_BROKER env var
Port: 1883 (MQTT)
Port: 8883 (MQTT over TLS) - Optional
Port: 9001 (WebSocket) - Optional
Client ID: {device_type}-{device_number} (e.g., esp8266-001)
Keep Alive: 60 seconds
Clean Session: True
```

---

## Error Handling

### Connection Failures
- Automatic reconnection with exponential backoff (max 30 seconds)
- Store messages locally and retry when connection restored
- Log connection failures to serial output

### Message Failures
- Missing required fields: Reject silently
- Invalid JSON: Log and skip
- Timeout on subscription: Retry up to 3 times

---

## Testing with Mosquitto CLI

### Publish Test Message
```bash
mosquitto_pub -h localhost -t "farm/sensor/temperature" -m '{"device_id":"test-001","value":25.5,"unit":"C","timestamp":"2026-04-16T10:30:00Z"}'
```

### Subscribe to Topic
```bash
mosquitto_sub -h localhost -t "farm/sensor/#" -v
```

### Subscribe to All Topics
```bash
mosquitto_sub -h localhost -t "farm/#" -v
```

---

## Performance Considerations

- **Message Size:** Keep < 1KB per message (embedded devices memory constraint)
- **Publish Frequency:** Sensor readings every 60-300 seconds (configurable)
- **Broker Capacity:** Mosquitto can handle 1000+ concurrent connections

---

## Security (Future Enhancements)

- [ ] TLS/SSL encryption for production
- [ ] Username/password authentication
- [ ] Topic ACL (Access Control List)
- [ ] Device certificate-based authentication
