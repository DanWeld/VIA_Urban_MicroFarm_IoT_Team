# Commands

## 1. Build Firmware
```powershell
C:\Users\admin\.platformio\penv\Scripts\platformio.exe run -e megaatmega2560
```

## 2. Upload Firmware
```powershell
C:\Users\admin\.platformio\penv\Scripts\platformio.exe run -e megaatmega2560 --target upload
```

## 3. Open Serial Monitor (COM7)
```powershell
C:\Users\admin\.platformio\penv\Scripts\platformio.exe device monitor -p COM7 -b 115200
```

## 4. Start Mosquitto Broker
```powershell
docker compose up -d mosquitto
```

## 5. Clear Retained Telemetry Message
```powershell
docker exec urbanmicrofarm-mosquitto mosquitto_pub -h localhost -p 1883 -t farm/sensor/telemetry -n -r
```

## 6. Subscribe Live Telemetry in Docker
```powershell
docker exec -it urbanmicrofarm-mosquitto mosquitto_sub -h localhost -p 1883 -t farm/sensor/telemetry -v
```

## 7. One-Shot Validation (Receive 3 Messages)
```powershell
docker exec urbanmicrofarm-mosquitto mosquitto_sub -h localhost -p 1883 -t farm/sensor/telemetry -C 3 -W 20 -v
```
