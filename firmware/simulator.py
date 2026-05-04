import os
import time
import paho.mqtt.client as mqtt
import random

broker = os.environ.get("MQTT_BROKER", "mosquitto")
port = int(os.environ.get("MQTT_PORT", 1883))
client_id = os.environ.get("MQTT_CLIENT_ID", "simulator_device_001")
topic = "urbanmicrofarm/sensor/temperature"
interval = int(os.environ.get("SENSOR_INTERVAL", 5))

client = mqtt.Client(client_id)
client.connect(broker, port)

print(f"[simulator] Publishing dummy data to {broker}:{port} every {interval}s...")

try:
    while True:
        temp = round(random.uniform(18.0, 30.0), 2)
        payload = f"{{\"temperature\": {temp}}}"
        client.publish(topic, payload)
        print(f"Published: {payload}")
        time.sleep(interval)
except KeyboardInterrupt:
    print("Simulator stopped.")
