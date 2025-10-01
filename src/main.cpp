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

// JSON document for MQTT messages - increased size to accommodate all data
StaticJsonDocument<512> jsonDoc;

// System State Enums
enum WiFiState {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_FAILED
};

enum SystemState {
    SYSTEM_INITIALIZING,
    SYSTEM_RUNNING,
    SYSTEM_ERROR
};

// System state variables
SystemState systemState = SYSTEM_INITIALIZING;
WiFiState wifiState = WIFI_DISCONNECTED;
bool sensorsInitialized = false;
bool displayInitialized = false;
bool mqttInitialized = false;

// Timing intervals (all in milliseconds)
const unsigned long SENSOR_READ_INTERVAL = 5000;        // Read sensor every 5 seconds
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;     // Update display every 1 second
const unsigned long WIFI_CHECK_INTERVAL = 10000;        // Check WiFi every 10 seconds
const unsigned long MQTT_PUBLISH_INTERVAL = 10000;      // Publish data every 10 seconds
const unsigned long SERIAL_OUTPUT_INTERVAL = 15000;     // Serial output every 15 seconds
const unsigned long STATUS_DISPLAY_DURATION = 3000;     // How long to show status messages

// Timing state variables
unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastMQTTPublish = 0;
unsigned long lastSerialOutput = 0;
unsigned long statusDisplayStart = 0;
unsigned long wifiConnectStart = 0;

// WiFi connection state
unsigned int wifiReconnectAttempts = 0;
const unsigned int MAX_WIFI_RECONNECT_ATTEMPTS = 3;
const unsigned long WIFI_CONNECT_TIMEOUT = 30000;       // 30 seconds timeout per attempt

// Current sensor data
PM25Sensor::PMData currentPMData = {0, 0, 0, false};
AQICalculator::AQIResult currentAQI = {0, "Unknown", "Initializing..."};

// Display state tracking
bool displayShowingStatus = false;
String currentStatus = "";
String lastPM25Display = "";
String lastAQIDisplay = "";
String lastHealthDisplay = "";
String lastProgressDisplay = "";
int progressCounter = 0;

// Function declarations
void manageWiFiConnection();
void readSensorData();
void updateDisplay();
void publishMQTTData();
void handleSerialOutput();
void updateDisplayPartial();
void showStatusTemporary(const String& status);
void initializeSystem();
bool initializeSensors();
bool initializeDisplay();
bool initializeMQTT();

void setup() {
    Serial.begin(9600);
    Serial.println("Smart PM2.5 Monitor Starting...");
    
    // Initialize system components
    initializeSystem();
}

void initializeSystem() {
    // Initialize display first so we can show status
    if (initializeDisplay()) {
        displayInitialized = true;
        showStatusTemporary("Display OK");
    } else {
        Serial.println("Display initialization failed!");
        systemState = SYSTEM_ERROR;
        return;
    }
    
    // Initialize sensors
    if (initializeSensors()) {
        sensorsInitialized = true;
        showStatusTemporary("Sensors OK");
    } else {
        Serial.println("Sensor initialization failed!");
        display.showError("Sensor Init Failed");
        systemState = SYSTEM_ERROR;
        return;
    }
    
    // Initialize MQTT (but don't require connection)
    if (initializeMQTT()) {
        mqttInitialized = true;
        showStatusTemporary("MQTT Init OK");
    } else {
        Serial.println("MQTT initialization failed!");
        showStatusTemporary("MQTT Init Failed");
        // Don't set error state - MQTT can be optional
    }
    
    // Set initial WiFi state to start connection process
    wifiState = WIFI_DISCONNECTED;
    systemState = SYSTEM_RUNNING;
    
    showStatusTemporary("System Ready!");
    Serial.println("System initialization complete. Starting main loop...");
}

void loop() {
    unsigned long currentTime = millis();
    
    // Handle system state
    switch (systemState) {
        case SYSTEM_INITIALIZING:
            // System is still initializing, wait
            return;
            
        case SYSTEM_ERROR:
            // System error state - try to recover every 30 seconds
            static unsigned long lastRecoveryAttempt = 0;
            if (currentTime - lastRecoveryAttempt > 30000) {
                lastRecoveryAttempt = currentTime;
                Serial.println("Attempting system recovery...");
                initializeSystem();
            }
            return;
            
        case SYSTEM_RUNNING:
            // Normal operation - continue with main tasks
            break;
    }
    
    // Task 1: Manage WiFi connection (non-blocking)
    manageWiFiConnection();
    
    // Task 2: Read sensor data periodically
    if (sensorsInitialized && (currentTime - lastSensorRead >= SENSOR_READ_INTERVAL)) {
        readSensorData();
        lastSensorRead = currentTime;
    }
    
    // Task 3: Update display (non-blocking, partial updates)
    if (displayInitialized && (currentTime - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)) {
        updateDisplay();
        lastDisplayUpdate = currentTime;
    }
    
    // Task 4: Publish MQTT data periodically
    if (mqttInitialized && (currentTime - lastMQTTPublish >= MQTT_PUBLISH_INTERVAL)) {
        publishMQTTData();
        lastMQTTPublish = currentTime;
    }
    
    // Task 5: Serial output for debugging
    if (currentTime - lastSerialOutput >= SERIAL_OUTPUT_INTERVAL) {
        handleSerialOutput();
        lastSerialOutput = currentTime;
    }
    
    // Keep MQTT connection alive if initialized
    if (mqttInitialized) {
        mqtt.loop();
    }
    
    // Small yield to prevent watchdog timeout
    yield();
}

// ========================================================================================
// TASK 1: NON-BLOCKING WIFI MANAGER
// ========================================================================================
void manageWiFiConnection() {
    unsigned long currentTime = millis();
    
    // Only check WiFi status at specified intervals
    if (currentTime - lastWiFiCheck < WIFI_CHECK_INTERVAL) {
        return;
    }
    lastWiFiCheck = currentTime;
    
    switch (wifiState) {
        case WIFI_DISCONNECTED:
            Serial.println("WiFi: Starting connection attempt...");
            WiFi.begin(ssid, password);
            wifiState = WIFI_CONNECTING;
            wifiConnectStart = currentTime;
            wifiReconnectAttempts++;
            showStatusTemporary("WiFi Connecting...");
            break;
            
        case WIFI_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                wifiState = WIFI_CONNECTED;
                wifiReconnectAttempts = 0;
                Serial.printf("WiFi: Connected to %s (IP: %s, RSSI: %d)\n", 
                             ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI());
                showStatusTemporary("WiFi Connected!");
            } else if (currentTime - wifiConnectStart > WIFI_CONNECT_TIMEOUT) {
                Serial.printf("WiFi: Connection timeout (attempt %d/%d)\n", 
                             wifiReconnectAttempts, MAX_WIFI_RECONNECT_ATTEMPTS);
                if (wifiReconnectAttempts >= MAX_WIFI_RECONNECT_ATTEMPTS) {
                    wifiState = WIFI_FAILED;
                    showStatusTemporary("WiFi Failed!");
                } else {
                    wifiState = WIFI_DISCONNECTED;
                    showStatusTemporary("WiFi Retry...");
                }
            }
            break;
            
        case WIFI_CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("WiFi: Connection lost, attempting reconnection...");
                wifiState = WIFI_DISCONNECTED;
                wifiReconnectAttempts = 0;
                showStatusTemporary("WiFi Lost!");
            }
            break;
            
        case WIFI_FAILED:
            // Reset to try again after extended delay
            if (currentTime - wifiConnectStart > 60000) { // Wait 1 minute before retry
                Serial.println("WiFi: Resetting connection attempts after extended delay");
                wifiReconnectAttempts = 0;
                wifiState = WIFI_DISCONNECTED;
            }
            break;
    }
}

// ========================================================================================
// TASK 2: NON-BLOCKING SENSOR READING
// ========================================================================================
void readSensorData() {
    PM25Sensor::PMData newData = pmSensor.read();
    
    if (!newData.isValid) {
        Serial.println("Sensor: Read failed, using previous data");
        showStatusTemporary("Sensor Error!");
        return;
    }
    
    // Debug: Print raw sensor data
    Serial.printf("Sensor: Raw data - PM1: %d, PM2.5: %d, PM10: %d\n", 
                 newData.pm1, newData.pm2_5, newData.pm10);
    
    // Only update if data actually changed
    if (newData.pm1 != currentPMData.pm1 || 
        newData.pm2_5 != currentPMData.pm2_5 || 
        newData.pm10 != currentPMData.pm10) {
        
        currentPMData = newData;
        currentAQI = AQICalculator::calculateAQI(currentPMData.pm2_5);
        
        Serial.printf("Sensor: New reading - PM2.5: %d μg/m³, AQI: %d (%s)\n", 
                     currentPMData.pm2_5, currentAQI.value, currentAQI.category.c_str());
        Serial.printf("Sensor: AQI details - Value: %d, Category: %s, Health: %s\n",
                     currentAQI.value, currentAQI.category.c_str(), currentAQI.healthMessage.c_str());
    }
}

// ========================================================================================
// TASK 3: NON-BLOCKING DISPLAY UPDATES (ANTI-FLICKER)
// ========================================================================================
void updateDisplay() {
    unsigned long currentTime = millis();
    
    // If showing temporary status, check if it's time to return to normal display
    if (displayShowingStatus && (currentTime - statusDisplayStart > STATUS_DISPLAY_DURATION)) {
        displayShowingStatus = false;
        // Force full display refresh after status
        lastPM25Display = "";
        lastAQIDisplay = "";
        lastHealthDisplay = "";
        lastProgressDisplay = "";
        progressCounter = 0;
    }
    
    // If showing status, don't update normal display
    if (displayShowingStatus) {
        return;
    }
    
    // Use partial update to prevent flickering
    updateDisplayPartial();
}

void updateDisplayPartial() {
    bool displayChanged = false;
    
    // Prepare display strings
    String pm25Str = String(currentPMData.pm2_5);
    String aqiStr = "AQI: " + String(currentAQI.value);
    String healthStr = currentAQI.healthMessage;
    
    // Create progress indicator
    progressCounter = (progressCounter + 1) % 4;
    String progressStr = "Status: ";
    switch (wifiState) {
        case WIFI_CONNECTED:
            progressStr += "Online ";
            break;
        case WIFI_CONNECTING:
            progressStr += "Connecting ";
            break;
        case WIFI_DISCONNECTED:
            progressStr += "Offline ";
            break;
        case WIFI_FAILED:
            progressStr += "Failed ";
            break;
    }
    
    // Add animated progress dots
    for (int i = 0; i < progressCounter; i++) {
        progressStr += ".";
    }
    for (int i = progressCounter; i < 3; i++) {
        progressStr += " ";
    }
    
    // Only update if something changed
    if (pm25Str != lastPM25Display || 
        aqiStr != lastAQIDisplay || 
        healthStr != lastHealthDisplay || 
        progressStr != lastProgressDisplay) {
        
        // Clear display once and redraw everything
        display._oled.clearDisplay();
        
        // Static labels and layout
        display._oled.setTextColor(WHITE, BLACK);
        
        // PM2.5 section (top)
        display._oled.setTextSize(1);
        display._oled.setCursor(0, 0);
        display._oled.println("PM2.5 (μg/m³):");
        
        display._oled.setTextSize(2);
        display._oled.setCursor(0, 12);
        display._oled.println(pm25Str);
        
        // AQI section
        display._oled.setTextSize(1);
        display._oled.setCursor(0, 32);
        display._oled.println(aqiStr);
        
        // Health message (truncate if too long)
        String healthDisplay = healthStr;
        if (healthDisplay.length() > 21) {
            healthDisplay = healthDisplay.substring(0, 18) + "...";
        }
        display._oled.setCursor(0, 42);
        display._oled.println(healthDisplay);
        
        // Progress status (bottom)
        display._oled.setCursor(0, 54);
        display._oled.println(progressStr);
        
        display._oled.display();
        
        // Update last displayed values
        lastPM25Display = pm25Str;
        lastAQIDisplay = aqiStr;
        lastHealthDisplay = healthStr;
        lastProgressDisplay = progressStr;
        
        displayChanged = true;
    }
}

void showStatusTemporary(const String& status) {
    if (!displayInitialized) return;
    
    display.showStatus(status);
    displayShowingStatus = true;
    statusDisplayStart = millis();
    currentStatus = status;
    Serial.println("Status: " + status);
}

// ========================================================================================
// MQTT AND COMMUNICATION FUNCTIONS
// ========================================================================================
void publishMQTTData() {
    if (wifiState != WIFI_CONNECTED || !currentPMData.isValid) {
        return;
    }
    
    // Create JSON document
    jsonDoc.clear();
    
    // Device Information
    jsonDoc["device_id"] = MQTTConfig::DEVICE_ID;
    
    // Sensor Readings
    JsonObject readings = jsonDoc.createNestedObject("readings");
    readings["pm1"] = currentPMData.pm1;
    readings["pm25"] = currentPMData.pm2_5;
    readings["pm10"] = currentPMData.pm10;
    
    // Metadata
    JsonObject metadata = jsonDoc.createNestedObject("metadata");
    metadata["timestamp"] = millis();
    metadata["wifi_rssi"] = WiFi.RSSI();
    metadata["ip"] = WiFi.localIP().toString();
    metadata["wifi_attempts"] = wifiReconnectAttempts;
    
    // Always calculate fresh AQI right before publishing
    AQICalculator::AQIResult freshAQI = AQICalculator::calculateAQI(currentPMData.pm2_5);
    
    // Debug AQI calculation with detailed info
    Serial.printf("AQI Debug: PM2.5=%d, AQI=%d, Category=%s\n", 
                  currentPMData.pm2_5, freshAQI.value, freshAQI.category.c_str());
    Serial.printf("AQI Raw Value: %d, Size: %d bytes\n", freshAQI.value, sizeof(freshAQI.value));
    
    // AQI Data - manually add each field with explicit value handling
    JsonObject aqiData = jsonDoc.createNestedObject("aqi");
    
    // Store the AQI value directly without casting
    int aqiValue = freshAQI.value;
    aqiData["value"] = aqiValue;
    
    // Create char buffers for string data to avoid String object issues
    char categoryBuffer[32];
    char healthBuffer[64];
    strncpy(categoryBuffer, freshAQI.category.c_str(), sizeof(categoryBuffer) - 1);
    strncpy(healthBuffer, freshAQI.healthMessage.c_str(), sizeof(healthBuffer) - 1);
    categoryBuffer[sizeof(categoryBuffer) - 1] = '\0';
    healthBuffer[sizeof(healthBuffer) - 1] = '\0';
    
    aqiData["category"] = categoryBuffer;
    aqiData["health_message"] = healthBuffer;
    
    Serial.printf("AQI Added to JSON: value=%d, category=%s, health=%s\n", 
                  aqiValue, categoryBuffer, healthBuffer);
    
    // Verify the JSON object has the value
    Serial.printf("JSON AQI value check: %d\n", (int)aqiData["value"]);
    
    // Update global AQI for display
    currentAQI = freshAQI;

    // Serialize JSON to string
    String jsonString;
    size_t jsonSize = serializeJson(jsonDoc, jsonString);
    
    // Debug: Print the JSON being sent
    Serial.println("JSON being sent:");
    Serial.println(jsonString);
    Serial.printf("JSON size: %d bytes\n", jsonSize);

    // Publish to MQTT
    if (mqtt.publish(MQTTConfig::TOPIC_TELEMETRY, jsonString.c_str())) {
        Serial.println("MQTT: Data published successfully");
        showStatusTemporary("Data Sent!");
    } else {
        Serial.println("MQTT: Failed to publish data");
        showStatusTemporary("MQTT Failed!");
    }
}

// ========================================================================================
// UTILITY AND INITIALIZATION FUNCTIONS
// ========================================================================================
void handleSerialOutput() {
    if (!currentPMData.isValid) return;
    
    Serial.println("========================================");
    Serial.printf("System Status: %s\n", 
                 systemState == SYSTEM_RUNNING ? "RUNNING" : 
                 systemState == SYSTEM_ERROR ? "ERROR" : "INITIALIZING");
    
    Serial.printf("WiFi Status: ");
    switch (wifiState) {
        case WIFI_CONNECTED:
            Serial.printf("CONNECTED (IP: %s, RSSI: %d dBm)\n", 
                         WiFi.localIP().toString().c_str(), WiFi.RSSI());
            break;
        case WIFI_CONNECTING:
            Serial.println("CONNECTING...");
            break;
        case WIFI_DISCONNECTED:
            Serial.println("DISCONNECTED");
            break;
        case WIFI_FAILED:
            Serial.println("FAILED");
            break;
    }
    
    Serial.printf("Sensor Data: PM1=%d, PM2.5=%d, PM10=%d μg/m³\n", 
                 currentPMData.pm1, currentPMData.pm2_5, currentPMData.pm10);
    Serial.printf("AQI: %d (%s)\n", currentAQI.value, currentAQI.category.c_str());
    Serial.printf("Health: %s\n", currentAQI.healthMessage.c_str());
    Serial.printf("MQTT Status: %s\n", mqtt.isConnected() ? "CONNECTED" : "DISCONNECTED");
    Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
    Serial.println("========================================");
}

bool initializeDisplay() {
    display.begin();
    return true; // Assume success - display.begin() doesn't return status
}

bool initializeSensors() {
    pmSensor.begin();
    return true; // Assume success - pmSensor.begin() doesn't return status
}

bool initializeMQTT() {
    mqtt.begin();
    return true; // MQTT connection will be handled separately
}