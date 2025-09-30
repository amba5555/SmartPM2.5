#!/usr/bin/env python3
"""
Test script to publish sample MQTT data to verify the backend is working
"""

import json
import time
import paho.mqtt.client as mqtt
from datetime import datetime

# MQTT Configuration (same as your ESP32)
MQTT_BROKER = "e14ab717adac4332aa8a52bc69935082.s1.eu.hivemq.cloud"
MQTT_USERNAME = "SmartPMdv01"
MQTT_PASSWORD = "SmartPM25Pass2025"
# Use the dotted topic to match the ESP32 firmware
TOPIC = "smartpm25.sensor.data"

def create_sample_data():
    """Create sample sensor data in the same format as ESP32"""
    now = int(datetime.now().timestamp() * 1000)  # milliseconds
    
    return {
        "device_id": "ESP32_PM25_001",
        "readings": {
            "pm1": 12,
            "pm25": 35,
            "pm10": 50
        },
        "aqi": {
            "value": 85,
            "category": "Moderate"
        },
        "metadata": {
            "timestamp": now,
            "wifi_rssi": -45,
            "ip": "192.168.1.100"
        }
    }

def on_connect(client, userdata, flags, rc):
    print(f"Connected to MQTT with result code {rc}")

def on_publish(client, userdata, mid):
    print(f"Message published with mid: {mid}")

def main():
    # Create MQTT client
    client = mqtt.Client()
    # Use TLS similar to the ESP32 firmware for HiveMQ Cloud
    try:
        client.tls_set()
    except Exception:
        pass
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.on_connect = on_connect
    client.on_publish = on_publish
    
    # Connect to broker
    print(f"Connecting to {MQTT_BROKER}...")
    client.connect(MQTT_BROKER, 8883, 60)
    client.loop_start()
    
    # Wait for connection
    time.sleep(2)
    
    # Create and publish sample data
    sample_data = create_sample_data()
    message = json.dumps(sample_data)
    
    print(f"Publishing to topic: {TOPIC}")
    print(f"Sample data: {message}")
    
    result = client.publish(TOPIC, message)
    
    if result.rc == mqtt.MQTT_ERR_SUCCESS:
        print("✅ Message published successfully!")
    else:
        print(f"❌ Failed to publish message: {result.rc}")
    
    # Wait a bit for processing
    time.sleep(2)
    client.loop_stop()
    client.disconnect()
    
    print("\n🔍 Now check the API endpoints:")
    print("curl http://localhost:8000/api/readings/latest")
    print("curl http://localhost:8000/api/readings/history")

if __name__ == "__main__":
    main()