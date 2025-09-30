# SmartPM2.5 Backend - MQTT Subscriber & API
# FastAPI app for receiving MQTT data and serving APIs

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
import paho.mqtt.client as mqtt
from supabase import create_client, Client
from datetime import datetime
import json
import config
import logging
import os
import time
from logging.handlers import RotatingFileHandler
from fastapi import Request, HTTPException

app = FastAPI(title="SmartPM2.5 Backend", version="1.0.0")

# CORS for frontend
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Update for production
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Global variables
supabase: Client = None
# Diagnostics
mqtt_connected = False
last_sub_result = None
last_received = None

# Logger
logger = logging.getLogger("smartpm")
logger.setLevel(logging.INFO)
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s: %(message)s",
)
# Add a rotating file handler
log_path = os.getenv("BACKEND_LOG_PATH", "./backend.log")
handler = RotatingFileHandler(log_path, maxBytes=1_000_000, backupCount=3)
handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(name)s: %(message)s"))
logger.addHandler(handler)
# Also log to stdout so PaaS (Render) captures logs reliably
stream_handler = logging.StreamHandler()
stream_handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(name)s: %(message)s"))
logger.addHandler(stream_handler)
import uuid

# Use an explicit client id to avoid collisions and make debugging easier
mqtt_client = mqtt.Client(client_id=f"smartpm-backend-{uuid.uuid4()}")

def on_connect(client, userdata, flags, rc):
    global mqtt_connected, last_sub_result
    mqtt_connected = (rc == 0)
    logger.info(f"Connected to MQTT with result code {rc}")
    # Subscribe and log the result tuple (result, mid)
    sub_result = client.subscribe("smartpm25.sensor.data")
    last_sub_result = sub_result
    logger.info(f"Subscribe result: {sub_result}")

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        global last_received
        last_received = data
        logger.info(f"Received: {data}")
        # Insert into Supabase with retries
        payload = {
            "device_id": data["device_id"],
            "pm1": data["readings"]["pm1"],
            "pm25": data["readings"]["pm25"],
            "pm10": data["readings"]["pm10"],
            "aqi": data["aqi"]["value"],
            "timestamp": data["metadata"]["timestamp"],
            "wifi_rssi": data["metadata"]["wifi_rssi"],
            "ip_address": data["metadata"]["ip"]
        }

        max_attempts = 3
        delay = 0.5
        for attempt in range(1, max_attempts + 1):
            try:
                resp = supabase.table("readings").insert(payload).execute()
                # Supabase client may include error info in resp.error or resp.get('error')
                # Log and break on success
                logger.info(f"Supabase insert response: {getattr(resp, 'data', resp)}")
                break
            except Exception as e:
                logger.error(f"Supabase insert attempt {attempt} failed: {e}")
                if attempt == max_attempts:
                    logger.error("Max Supabase insert attempts reached; dropping message")
                else:
                    time.sleep(delay)
                    delay *= 2

    except Exception as e:
        print(f"Error processing message: {e}")

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

@app.on_event("startup")
async def startup_event():
    global supabase
    # Initialize Supabase client
    supabase = create_client(config.SUPABASE_URL, config.SUPABASE_KEY)
    print(f"Connected to Supabase: {config.SUPABASE_URL}")
    
    # Connect to MQTT
    # Use TLS when connecting to HiveMQ Cloud (port 8883)
    try:
        mqtt_client.tls_set()  # use system CA certs
    except Exception:
        # If tls_set fails for any reason, continue and let the connection attempt run
        pass

    mqtt_client.username_pw_set(
        config.MQTT_USERNAME,
        config.MQTT_PASSWORD
    )
    mqtt_client.connect(config.MQTT_BROKER, 8883, 60)
    mqtt_client.loop_start()
    print(f"Connected to MQTT broker: {config.MQTT_BROKER}")

@app.on_event("shutdown")
async def shutdown_event():
    mqtt_client.loop_stop()


@app.get("/debug/mqtt")
async def debug_mqtt(request: Request):
    # Only enable debug endpoint if explicitly allowed via env var
    if os.getenv("ENABLE_MQTT_DEBUG", "false").lower() not in ("1", "true", "yes"):
        raise HTTPException(status_code=403, detail="debug endpoint disabled")

    token = os.getenv("DEBUG_TOKEN")
    if not token:
        # If token is not configured, disallow access for safety
        raise HTTPException(status_code=403, detail="debug endpoint not configured")

    header_token = request.headers.get("x-debug-token") or request.headers.get("X-Debug-Token")
    if header_token != token:
        raise HTTPException(status_code=401, detail="invalid debug token")

    try:
        return {
            "connected": mqtt_connected,
            "last_sub_result": last_sub_result,
            "last_received": last_received,
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/health")
async def health():
    return {"status": "ok", "mqtt_connected": mqtt_connected}

@app.get("/api/readings/latest")
async def get_latest_reading():
    try:
        response = supabase.table("readings").select("*").order("timestamp", desc=True).limit(1).execute()
        if response.data:
            return response.data[0]
        else:
            return {"message": "No readings available yet"}
    except Exception as e:
        return {"error": f"Database query failed: {str(e)}"}

@app.get("/api/readings/history")
async def get_history(period: str = "24h"):
    try:
        # Calculate time range
        now = int(datetime.now().timestamp() * 1000)  # Convert to int milliseconds
        if period == "7d":
            start_time = now - (7 * 24 * 60 * 60 * 1000)
        elif period == "30d":
            start_time = now - (30 * 24 * 60 * 60 * 1000)
        else:  # 24h
            start_time = now - (24 * 60 * 60 * 1000)

        response = supabase.table("readings").select("*").gte("timestamp", start_time).order("timestamp").execute()
        return {"data": response.data, "count": len(response.data)}
    except Exception as e:
        return {"error": f"Database query failed: {str(e)}"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)