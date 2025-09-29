# Smart PM2.5 Monitor

ESP32-based smart particulate matter (PM2.5) monitor with MQTT cloud connectivity.

## Setup Instructions

### 1. Hardware Requirements
- ESP32 Development Board
- PMS5003 PM2.5 Sensor
- SSD1306 OLED Display
- Connection wires

### 2. Software Requirements
- PlatformIO IDE
- HiveMQ Cloud Account
- Required Libraries (automatically installed via platformio.ini)

### 3. MQTT Configuration
1. Copy the example configuration:
   ```bash
   cp include/MQTTConfig.example.h include/MQTTConfig.h
   ```

2. Create MQTT configuration implementation:
   ```bash
   cp src/mqtt_config.example.cpp src/mqtt_config.cpp
   ```

3. Set up HiveMQ Cloud:
   - Create an account at [HiveMQ Cloud](https://console.hivemq.cloud)
   - Create a new cluster (free tier is sufficient)
   - Navigate to "Access Management"
   - Create new credentials
   - Add the following permissions:
     - Publish to: `smartpm25.sensor.data`, `smartpm25.device.status`
     - Subscribe to: `smartpm25.device.commands`

4. Update MQTT Configuration:
   Edit `src/mqtt_config.cpp` with your HiveMQ Cloud credentials:
   ```cpp
   const char* MQTT_BROKER = "your-cluster-url.hivemq.cloud";
   const char* MQTT_USERNAME = "your-username";
   const char* MQTT_PASSWORD = "your-password";
   const char* DEVICE_ID = "unique-device-id";  // Change for each device
   ```

### 4. Security Best Practices
1. Never commit MQTT credentials to git
2. Use a unique device ID for each deployment
3. Keep your HiveMQ Cloud credentials secure
4. Regularly rotate MQTT passwords
5. Monitor device access logs in HiveMQ Cloud
6. Use specific topic permissions instead of wildcards
7. Enable SSL/TLS verification in production

### 5. Data Format
The device publishes JSON data in the following format:
```json
{
  "device_id": "ESP32_PM25_001",
  "readings": {
    "pm1": 2,
    "pm25": 3,
    "pm10": 3
  },
  "metadata": {
    "timestamp": 216588,
    "wifi_rssi": -65,
    "ip": "192.168.1.100"
  },
  "aqi": {
    "value": 15,
    "category": "Good",
    "health_message": "Air quality is satisfactory"
  }
}
```

### 6. Topics
- `smartpm25.sensor.data`: Sensor readings
- `smartpm25.device.status`: Device online/offline status
- `smartpm25.device.commands`: Command channel (future use)

## Troubleshooting
1. Connection Issues:
   - Verify WiFi credentials
   - Check MQTT broker URL
   - Verify username/password
   - Check topic permissions

2. Data Issues:
   - Check sensor connections
   - Verify JSON format
   - Monitor serial output

## License
MIT License