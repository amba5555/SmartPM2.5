#include <WiFi.h>
#include <ArduinoJson.h>
#include "PM25Sensor.h"
#include "Display.h"
#include "AQICalculator.h"
#include "MQTTManager.h"

// WiFi Configuration
const char* ssid = "BIOLOGY_2.4G";
const char* password = "19012567";

// MQTT Topics
const char* MQTT_TOPIC_SENSOR = "smartpm25/sensor/data";
const char* MQTT_TOPIC_STATUS = "smartpm25/sensor/status";

// Component instances
PM25Sensor pmSensor(14, 16);  // RX, TX pins
Display display(128, 64);     // OLED display dimensions
MQTTManager mqtt;

// JSON document for MQTT messages
StaticJsonDocument<200> jsonDoc;

// System state
bool systemInitialized = false;
unsigned long lastReadingTime = 0;
const unsigned long READ_INTERVAL = 1000;  // 1 second interval

// Function declarations
void sendData(unsigned int pm1, unsigned int pm2_5, unsigned int pm10);

void setup() {
    Serial.begin(9600);
    
    // Initialize display
    display.begin();
    display.showStatus("Starting...");
    delay(1000);

    // Initialize PM2.5 sensor
    display.showStatus("Init Sensor...");
    pmSensor.begin();
    delay(1000);

    // Connect to WiFi
    display.showStatus("WiFi Setup...");
    WiFi.begin(ssid, password);
    
    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
        wifiAttempts++;
        String dots = String("");
        for(int i = 0; i < (wifiAttempts % 4); i++) dots += ".";
        display.showStatus("Connecting" + dots);
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        display.showStatus("WiFi Connected!");
        Serial.println("WiFi connected to: " + String(ssid));
        delay(1000);
    } else {
        display.showStatus("WiFi Failed!");
        Serial.println("WiFi connection failed");
        delay(2000);
    }

    // Initialize MQTT
    display.showStatus("Init MQTT...");
    mqtt.begin();
    
    // Connect to MQTT broker
    if (mqtt.connect()) {
        display.showStatus("MQTT Connected!");
        delay(1000);
    } else {
        display.showStatus("MQTT Failed!");
        delay(2000);
    }

    systemInitialized = true;
    display.showStatus("Ready!");
    delay(2000);
}

void loop() {
    static unsigned long lastSerialOutput = 0;
    static const unsigned long SERIAL_INTERVAL = 5000;  // Print to serial every 5 seconds
    
    if (!systemInitialized) {
        display.showError("Error!");
        delay(1000);
        return;
    }

    // Read sensor data at specified intervals
    if (millis() - lastReadingTime >= READ_INTERVAL) {
        lastReadingTime = millis();
        
        // Read PM2.5 sensor data
        PM25Sensor::PMData pmData = pmSensor.read();
        
        if (!pmData.isValid) {
            display.showError("Sensor Err");
            delay(2000);
            return;
        }

        // Calculate AQI
        AQICalculator::AQIResult aqiResult = AQICalculator::calculateAQI(pmData.pm2_5);

        // Update display with new readings and AQI information
        display.showReadings(pmData.pm2_5, aqiResult.value, aqiResult.healthMessage);

        // Log data to serial less frequently
        if (millis() - lastSerialOutput >= SERIAL_INTERVAL) {
            lastSerialOutput = millis();
            Serial.println("----------------------------------------");
            Serial.printf("PM2.5: %d μg/m³\n", pmData.pm2_5);
            Serial.printf("AQI: %d (%s)\n", aqiResult.value, aqiResult.category.c_str());
            Serial.printf("Health: %s\n", aqiResult.healthMessage.c_str());
            Serial.println("----------------------------------------");
        }

        // Send data to cloud
        sendData(pmData.pm1, pmData.pm2_5, pmData.pm10);
    }

    // Small delay to prevent CPU hogging
    delay(50);
}

void sendData(unsigned int pm1, unsigned int pm2_5, unsigned int pm10) {
    if (WiFi.status() != WL_CONNECTED) {
        display.showStatus("No WiFi!");
        delay(1000);
        return;
    }

    static unsigned long lastUploadTime = 0;
    if (millis() - lastUploadTime < 5000) {  // Only show upload status every 5 seconds
        return;
    }
    lastUploadTime = millis();

    // Create JSON document
    jsonDoc.clear();
    
    // Device Information
    jsonDoc["device_id"] = MQTTConfig::DEVICE_ID;
    
    // Sensor Readings
    JsonObject readings = jsonDoc.createNestedObject("readings");
    readings["pm1"] = pm1;
    readings["pm25"] = pm2_5;
    readings["pm10"] = pm10;
    
    // Metadata
    JsonObject metadata = jsonDoc.createNestedObject("metadata");
    metadata["timestamp"] = millis();
    metadata["wifi_rssi"] = WiFi.RSSI();
    metadata["ip"] = WiFi.localIP().toString();
    
    // Calculate AQI
    AQICalculator::AQIResult aqi = AQICalculator::calculateAQI(pm2_5);
    JsonObject aqiData = jsonDoc.createNestedObject("aqi");
    aqiData["value"] = aqi.value;
    aqiData["category"] = aqi.category;
    aqiData["health_message"] = aqi.healthMessage;

    // Serialize JSON to string
    String jsonString;
    serializeJson(jsonDoc, jsonString);

    // Publish to MQTT
    if (mqtt.publish(MQTTConfig::TOPIC_TELEMETRY, jsonString.c_str())) {
        Serial.println("Data published successfully");
        display.showStatus("Data Sent!");
    } else {
        Serial.println("Failed to publish data");
        display.showStatus("Send Failed!");
    }

    // Keep MQTT connection alive
    mqtt.loop();
}