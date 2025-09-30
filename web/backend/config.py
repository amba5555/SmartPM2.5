# Configuration for SmartPM2.5 Backend
# Environment variables for Supabase and MQTT

import os
from dotenv import load_dotenv

load_dotenv()

# Supabase
SUPABASE_URL = os.getenv("SUPABASE_URL")
SUPABASE_KEY = os.getenv("SUPABASE_KEY")

# MQTT
MQTT_BROKER = os.getenv("MQTT_BROKER")
MQTT_USERNAME = os.getenv("MQTT_USERNAME")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD")

# Validate required env vars and show which ones are missing
required = {
    "SUPABASE_URL": SUPABASE_URL,
    "SUPABASE_KEY": SUPABASE_KEY,
    "MQTT_BROKER": MQTT_BROKER,
    "MQTT_USERNAME": MQTT_USERNAME,
    "MQTT_PASSWORD": MQTT_PASSWORD,
}
missing = [name for name, val in required.items() if not val]
if missing:
    raise ValueError(f"Missing required environment variables: {', '.join(missing)}. Check web/backend/.env or environment.")