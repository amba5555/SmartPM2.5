#!/usr/bin/env python3
"""
Test script to directly insert sample data into Supabase
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from supabase import create_client
from datetime import datetime
import config

def main():
    try:
        # Create Supabase client
        supabase = create_client(config.SUPABASE_URL, config.SUPABASE_KEY)
        print(f"Connected to Supabase: {config.SUPABASE_URL}")
        
        # Create sample data
        now = int(datetime.now().timestamp() * 1000)
        sample_data = {
            "device_id": "ESP32_PM25_001",
            "pm1": 12,
            "pm25": 35,
            "pm10": 50,
            "aqi": 85,
            "timestamp": now,
            "wifi_rssi": -45,
            "ip_address": "192.168.1.100"
        }
        
        print(f"Inserting sample data: {sample_data}")
        
        # Insert data
        response = supabase.table("readings").insert(sample_data).execute()
        
        if response.data:
            print("✅ Sample data inserted successfully!")
            print(f"Inserted record: {response.data[0]}")
        else:
            print("❌ Failed to insert data")
            
    except Exception as e:
        print(f"❌ Error: {e}")
    
    print("\n🔍 Now test the API endpoints:")
    print("curl http://localhost:8000/api/readings/latest")
    print("curl http://localhost:8000/api/readings/history")

if __name__ == "__main__":
    main()