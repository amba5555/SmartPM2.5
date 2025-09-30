import time
import json
import requests
import paho.mqtt.client as mqtt
from datetime import datetime

MQTT_BROKER = "e14ab717adac4332aa8a52bc69935082.s1.eu.hivemq.cloud"
MQTT_USERNAME = "SmartPMdv01"
MQTT_PASSWORD = "SmartPM25Pass2025"
TOPIC = "smartpm25.sensor.data"


def create_sample_data():
    now = int(datetime.now().timestamp() * 1000)
    return {
        "device_id": "INTEGRATION_TEST_001",
        "readings": {"pm1": 1, "pm25": 2, "pm10": 3},
        "aqi": {"value": 5},
        "metadata": {"timestamp": now, "wifi_rssi": -50, "ip": "127.0.0.1"}
    }


def test_publish_and_check_api():
    client = mqtt.Client()
    try:
        client.tls_set()
    except Exception:
        pass
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.connect(MQTT_BROKER, 8883, 60)
    client.loop_start()

    payload = create_sample_data()
    client.publish(TOPIC, json.dumps(payload))

    # Poll the API for up to 10 seconds
    found = False
    for _ in range(20):
        r = requests.get("http://127.0.0.1:8000/api/readings/latest", timeout=3)
        if r.status_code == 200 and "device_id" in r.json():
            if r.json().get("device_id") == "INTEGRATION_TEST_001":
                found = True
                break
        time.sleep(0.5)

    client.loop_stop()
    client.disconnect()

    assert found, "Published message was not observed by the backend API"
