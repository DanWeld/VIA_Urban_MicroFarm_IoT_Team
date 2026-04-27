# MQTT Telemetry Procedure (Arduino Mega + ESP8266 + Mosquitto)

## Purpose
Use this procedure to verify that firmware telemetry is published to Mosquitto on topic farm/sensor/telemetry.

## Prerequisites
- Docker Desktop is running.
- USB cable connected to Arduino Mega.
- Board is available on COM7.
- Project root is open in terminal:
  C:\Users\admin\Desktop\VIA_Urban_MicroFarm_IoT_Team

## 1) Build and Upload Firmware
Run:

    C:\Users\admin\.platformio\penv\Scripts\platformio.exe run -e megaatmega2560 --target upload

Expected result:
- Upload succeeds and flash verification is OK.

## 2) Start Mosquitto Service (if needed)
Run:

    docker compose up -d mosquitto

Expected result:
- Container urbanmicrofarm-mosquitto is running.

## 3) Clear Old Retained Telemetry
Run once:

    docker exec urbanmicrofarm-mosquitto mosquitto_pub -h localhost -p 1883 -t farm/sensor/telemetry -n -r

Expected result:
- Old retained values (test_message, healthcheck, null) are removed.

## 4) Subscribe in Docker Terminal
Open a dedicated terminal and run:

    docker exec -it urbanmicrofarm-mosquitto mosquitto_sub -h localhost -p 1883 -t farm/sensor/telemetry -v

Keep this terminal open.

## 5) Reset the Board
Press RESET on the board once.

Expected serial startup lines include:
- VIA UNIVERSITY COLLEGE SEP4 IoT Hardware DRIVERS DEMO
- Connecting to WiFi hotspot '3Bredband-CB45'.
- Connected to WiFi hotspot '3Bredband-CB45'.
- MQTT CONNECT sent to broker 192.168.1.61:1883 (client_id=arduino_mega_001).

## 6) Verify Live Telemetry
In the Docker subscriber terminal, you should see lines like:

    farm/sensor/telemetry {"setup_id":1,"sensor_id":null,"temperature":202,"humidity":580,"light":118,"soil_moisture":338}

New messages should arrive about every 2 seconds.

## Optional Quick Validation (Auto-stop After 3 Messages)
Run:

    docker exec urbanmicrofarm-mosquitto mosquitto_sub -h localhost -p 1883 -t farm/sensor/telemetry -C 3 -W 20 -v

Expected result:
- Command exits after receiving 3 telemetry messages.

## Troubleshooting

### A) PowerShell says mosquitto_sub is not recognized
Use Docker-based subscriber command from Step 4.

### B) Serial monitor Access denied on COM7
Close all serial apps (PlatformIO monitor, Arduino IDE, PuTTY, other terminals) and reopen monitor with explicit port:

    C:\Users\admin\.platformio\penv\Scripts\platformio.exe device monitor -p COM7 -b 115200

### C) No messages in Docker subscriber
- Confirm upload completed successfully.
- Confirm board reset after upload.
- Confirm WiFi and MQTT CONNECT lines appear in serial output.
- Re-run Step 3 to clear retained values and Step 4 to start a fresh subscriber.

### D) Board enters interactive menu instead of publishing loop
If menu appears, do not select drivers. Reset the board and verify startup logs again.

## Known Good Topic and Broker
- Topic: farm/sensor/telemetry
- Broker host (from firmware): 192.168.1.61
- Broker port: 1883
- Docker container: urbanmicrofarm-mosquitto
