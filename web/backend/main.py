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
        # Ignore integration test messages that may be published during CI or local tests.
        # This prevents test data (INTEGRATION_TEST_001) from showing up in the production API.
        device_id = data.get("device_id")
        if device_id == "INTEGRATION_TEST_001":
            logger.info("Ignoring integration test message from INTEGRATION_TEST_001")
            return
        # Normalize timestamp: some devices publish seconds instead of milliseconds
        raw_ts = None
        try:
            raw_ts = data.get("metadata", {}).get("timestamp")
        except Exception:
            raw_ts = None

        now_ms = int(time.time() * 1000)
        ts = None
        try:
            if raw_ts is None:
                ts = now_ms
            else:
                # Accept either int or string numeric
                ts_val = int(raw_ts)
                # If the value looks like seconds (10 digits or less), convert to ms
                if ts_val < 1_000_000_000_000:
                    ts = ts_val * 1000
                else:
                    ts = ts_val
        except Exception:
            ts = now_ms

        logger.info(f"Normalized timestamp for insert: {ts} (raw: {raw_ts})")

        # Insert into Supabase with retries
        payload = {
            "device_id": data["device_id"],
            "pm1": data["readings"]["pm1"],
            "pm25": data["readings"]["pm25"],
            "pm10": data["readings"]["pm10"],
            "aqi": data["aqi"]["value"],
            "timestamp": ts,
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
async def health_check():
    return {"status": "healthy", "timestamp": datetime.now(), "version": "2.1"}

@app.get("/api/readings/latest")
async def get_latest_reading():
    try:
        # Prefer real device data over known integration test rows.
        # First try to fetch the most recent row that is NOT the integration test.
        try:
            filtered = supabase.table("readings")
            # Exclude the known integration test device id. If you have more test ids,
            # expand this check or add a dedicated 'is_test' column.
            filtered_resp = filtered.select("*").neq("device_id", "INTEGRATION_TEST_001").order("created_at", desc=True).limit(1).execute()
            if getattr(filtered_resp, 'data', None):
                return filtered_resp.data[0]
        except Exception:
            # If the filtered query fails for any reason, fall back to the unfiltered query below.
            pass

        # Fallback: return the most recent row regardless (covers cases where only test data exists).
        response = supabase.table("readings").select("*").order("created_at", desc=True).limit(1).execute()
        if response.data:
            return response.data[0]
        else:
            return {"message": "No readings available yet"}
    except Exception as e:
        return {"error": f"Database query failed: {str(e)}"}

@app.get("/api/readings/latest-batch")
async def get_latest_batch(limit: int = 50):
    """Get the latest N readings for real-time rolling display"""
    try:
        # Limit to reasonable range
        limit = min(max(limit, 1), 200)
        
        response = supabase.table("readings").select("*").neq("device_id", "INTEGRATION_TEST_001").order("created_at", desc=True).limit(limit).execute()
        if response.data:
            # Reverse to get chronological order (oldest first)
            data = list(reversed(response.data))
            return {"data": data, "count": len(data)}
        else:
            return {"data": [], "count": 0}
    except Exception as e:
        return {"error": f"Database query failed: {str(e)}"}

@app.get("/api/v1/readings/aggregated")
async def get_aggregated_readings(timeframe: str = "1h"):
    """Get aggregated PM2.5 readings using database-side aggregation for better performance"""
    try:
        from datetime import datetime, timedelta
        
        # Define timeframe configurations with appropriate bucket intervals
        timeframe_configs = {
            "5m": {"hours": 0, "minutes": 5, "bucket_interval": "10 seconds"},
            "30m": {"hours": 0, "minutes": 30, "bucket_interval": "1 minute"},
            "1h": {"hours": 1, "minutes": 0, "bucket_interval": "2 minutes"},
            "4h": {"hours": 4, "minutes": 0, "bucket_interval": "10 minutes"},
            "24h": {"hours": 24, "minutes": 0, "bucket_interval": "30 minutes"}
        }
        
        config = timeframe_configs.get(timeframe)
        if not config:
            raise HTTPException(status_code=400, detail=f"Invalid timeframe: {timeframe}")
        
        # Calculate start and end times
        now = datetime.utcnow()
        start_time = now - timedelta(hours=config["hours"], minutes=config["minutes"])
        
        # Call the Supabase RPC function for aggregation
        # Note: We'll use a simple aggregation approach since time_bucket may not be available
        # This approach groups by time intervals and calculates averages
        response = supabase.rpc('get_aggregated_pm25', {
            'start_time': start_time.isoformat(),
            'end_time': now.isoformat(),
            'bucket_interval': config["bucket_interval"]
        }).execute()
        
        if hasattr(response, 'data') and response.data:
            return {
                "timeframe": timeframe,
                "start_time": start_time.isoformat(),
                "end_time": now.isoformat(),
                "bucket_interval": config["bucket_interval"],
                "data": response.data,
                "count": len(response.data)
            }
        else:
            # Fallback to simple aggregation if RPC function doesn't exist
            return await get_aggregated_readings_fallback(timeframe, start_time, now, config["bucket_interval"])
            
    except Exception as e:
        logger.error(f"Error in get_aggregated_readings: {str(e)}")
        # Fallback to existing logic if aggregation fails
        return await get_readings_history_fallback(timeframe)

async def get_aggregated_readings_fallback(timeframe: str, start_time: datetime, end_time: datetime, bucket_interval: str):
    """Fallback aggregation when database function is not available"""
    try:
        # Simple time-based aggregation using existing data
        response = supabase.table("readings").select("created_at, pm25, aqi").neq("device_id", "INTEGRATION_TEST_001").gte("created_at", start_time.isoformat() + "Z").lte("created_at", end_time.isoformat() + "Z").order("created_at", desc=False).execute()
        
        raw_data = response.data or []
        
        # Group data into time buckets for aggregation
        bucket_size_minutes = {
            "10 seconds": 1/6,
            "1 minute": 1,
            "2 minutes": 2,
            "10 minutes": 10,
            "30 minutes": 30
        }.get(bucket_interval, 5)
        
        buckets = {}
        for row in raw_data:
            try:
                created_at = datetime.fromisoformat(row['created_at'].replace('Z', '+00:00'))
                # Round to bucket boundary
                bucket_minutes = int(created_at.minute / bucket_size_minutes) * bucket_size_minutes
                bucket_time = created_at.replace(minute=int(bucket_minutes), second=0, microsecond=0)
                bucket_key = bucket_time.isoformat()
                
                if bucket_key not in buckets:
                    buckets[bucket_key] = {'pm25_values': [], 'bucket_time': bucket_time}
                buckets[bucket_key]['pm25_values'].append(float(row['pm25']))
            except Exception as e:
                continue
        
        # Calculate averages for each bucket
        aggregated_data = []
        for bucket_key in sorted(buckets.keys()):
            bucket = buckets[bucket_key]
            avg_pm25 = sum(bucket['pm25_values']) / len(bucket['pm25_values']) if bucket['pm25_values'] else 0
            aggregated_data.append({
                'bucket_time': bucket['bucket_time'].isoformat(),
                'average_pm25': round(avg_pm25, 2)
            })
        
        return {
            "timeframe": timeframe,
            "start_time": start_time.isoformat(),
            "end_time": end_time.isoformat(),
            "bucket_interval": bucket_interval,
            "data": aggregated_data,
            "count": len(aggregated_data)
        }
        
    except Exception as e:
        logger.error(f"Error in fallback aggregation: {str(e)}")
        raise HTTPException(status_code=500, detail="Aggregation failed")

async def get_readings_history_fallback(timeframe: str):
    """Fallback to existing history endpoint logic"""
    # Map new timeframe format to existing period format
    period_map = {"5m": "5min", "30m": "30min", "1h": "1h", "4h": "4h", "24h": "24h"}
    period = period_map.get(timeframe, "1h")
    return await get_readings_history(period)

@app.get("/api/readings/history")
async def get_readings_history(period: str = "1h", agg: str = "avg", buckets: int = None):
    """Get historical readings for different time periods with appropriate aggregation"""
    try:
        from datetime import datetime, timedelta
        
        # Use the database's latest created_at as the authoritative 'now' to avoid container clock skew.
        # Fallback to server UTC time if the DB query fails.
        now = None
        try:
            latest_resp = supabase.table("readings").select("created_at").order("created_at", desc=True).limit(1).execute()
            if getattr(latest_resp, 'data', None) and len(latest_resp.data) > 0:
                latest_created = latest_resp.data[0].get('created_at')
                if latest_created:
                    # Parse ISO timestamp returned by Supabase (includes timezone offset)
                    try:
                        # fromisoformat supports offsets like +00:00
                        now = datetime.fromisoformat(latest_created)
                    except Exception:
                        # As a last resort, fallback to utcnow
                        now = datetime.utcnow()
        except Exception as e:
            # If anything goes wrong with the DB lookup, fall back to utcnow
            now = None

        if now is None:
            now = datetime.utcnow()
        
        # Define time ranges and limits - calculate from current time going backwards
        time_configs = {
            "5min": {"duration": timedelta(minutes=5), "limit": 100},  # 5 minutes back, more samples
            "30min": {"duration": timedelta(minutes=30), "limit": 150},  # 30 minutes back  
            "1h": {"duration": timedelta(hours=1), "limit": 200},  # 1 hour back
            "4h": {"duration": timedelta(hours=4), "limit": 200},  # 4 hours back
            "24h": {"duration": timedelta(hours=24), "limit": 200},  # 24 hours back
        }
        
        config = time_configs.get(period, time_configs["1h"])
        start_time = now - config["duration"]
        
        # Query raw data from start_time to now, ordered by created_at DESC (newest first)
        # Using created_at timestamps for proper time filtering
        response = supabase.table("readings").select("*").neq("device_id", "INTEGRATION_TEST_001").gte("created_at", 
            start_time.isoformat() + "Z").lte("created_at", now.isoformat() + "Z").order("created_at", desc=True).limit(config["limit"]).execute()

        raw_rows = response.data or []
        # Reverse to chronological order (oldest -> newest)
        raw_rows = list(reversed(raw_rows))

        # Create regular time buckets spanning [start_time, now]. Use per-period bucket counts
        # so time bin sizes feel sensible for short vs long ranges.
        per_period_buckets = {
            "5min": 60,   # ~5s bins
            "30min": 60,  # ~30s bins
            "1h": 60,     # ~1min bins
            "4h": 120,    # ~2min bins
            "24h": 144    # ~10min bins
        }
        desired_buckets = buckets if (buckets and buckets > 0) else per_period_buckets.get(period, 60)

        # total_seconds is the requested duration length in seconds
        total_seconds = config["duration"].total_seconds()
        # bucket_size in seconds (minimum 1 second)
        bucket_size = max(1, total_seconds / desired_buckets)

        # Build bucket boundaries
        buckets = []
        from datetime import timedelta
        for i in range(desired_buckets):
            b_start = start_time + timedelta(seconds=bucket_size * i)
            b_end = start_time + timedelta(seconds=bucket_size * (i + 1))
            buckets.append({"start": b_start, "end": b_end, "rows": []})

        # Assign rows to buckets
        for r in raw_rows:
            try:
                created = r.get('created_at')
                if not created:
                    continue
                created_dt = datetime.fromisoformat(created)
            except Exception:
                # skip unparsable timestamps
                continue
            # If row is out of bounds, skip
            if created_dt < start_time or created_dt > now:
                continue
            # compute index
            idx = int((created_dt - start_time).total_seconds() // bucket_size)
            if idx < 0:
                idx = 0
            if idx >= len(buckets):
                idx = len(buckets) - 1
            buckets[idx]["rows"].append(r)

        # Aggregate each bucket using the requested aggregation method
        from statistics import median
        agg_buckets = []
        for b in buckets:
            rows = b["rows"]
            if rows:
                pm25_vals = [float(rr.get('pm25') or 0) for rr in rows]
                if agg == 'max':
                    agg_pm25 = max(pm25_vals)
                elif agg == 'median':
                    agg_pm25 = median(pm25_vals)
                else:
                    # default to average
                    agg_pm25 = sum(pm25_vals) / len(pm25_vals)

                # choose latest row's aqi (rows are chronological)
                latest_aqi = rows[-1].get('aqi')
                count = len(rows)
            else:
                agg_pm25 = None
                latest_aqi = None
                count = 0
            agg_buckets.append({
                "start": b["start"].isoformat(),
                "end": b["end"].isoformat(),
                "pm25": (round(agg_pm25, 2) if agg_pm25 is not None else None),
                "aqi": latest_aqi,
                "count": count
            })

        return {"data": raw_rows, "count": len(raw_rows), "period": period, "start_time": start_time.isoformat(), "end_time": now.isoformat(), "buckets": agg_buckets}
    except Exception as e:
        return {"error": f"Database query failed: {str(e)}"}

@app.get("/api/internal/wake")
async def wake_endpoint(request: Request):
    """
    Lightweight wake endpoint for MQTT pinger.
    Returns quickly without DB access to minimize response time.
    """
    # Optional: Check for wake token in headers
    wake_token = os.getenv("WAKE_TOKEN")
    if wake_token:
        provided_token = request.headers.get("X-Wake-Token")
        if provided_token != wake_token:
            raise HTTPException(status_code=401, detail="Invalid wake token")
    
    return {
        "status": "awake",
        "timestamp": int(time.time() * 1000),
        "mqtt_connected": mqtt_connected,
        "last_received": last_received
    }

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)