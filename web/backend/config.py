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

# Validate required env vars
required_vars = [SUPABASE_URL, SUPABASE_KEY, MQTT_BROKER, MQTT_USERNAME, MQTT_PASSWORD]
if not all(required_vars):
    raise ValueError("Missing required environment variables. Check .env file.")