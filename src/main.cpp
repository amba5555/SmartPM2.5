#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "PM25Sensor.h"
#include "Display.h"
#include "AQICalculator.h"

// WiFi and Server Configuration
const char* ssid = "BIOLOGY_2.4G";
const char* password = "19012567";
const char* host = "script.google.com";
const int httpsPort = 443;
const String GAS_ID = "AKfycbz-Y-BkbIaTN43UiyIAukAkeaEeUT81PH32MRnP-Vna2OV-gotoJghNXDaojeWAaeY";

// Component instances
PM25Sensor pmSensor(14, 16);  // RX, TX pins
Display display(128, 64);     // OLED display dimensions
WiFiClientSecure client;

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

    // Configure secure client
    client.setInsecure();

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

    if (!client.connect(host, httpsPort)) {
        Serial.println("Connection failed");
        return;
    }

    String url = "/macros/s/" + GAS_ID + "/exec?pm1=" + String(pm1) + 
                "&pm25=" + String(pm2_5) + 
                "&pm10=" + String(pm10);

    Serial.print("Requesting URL: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "User-Agent: BuildFailureDetectorESP8266\r\n" +
                "Connection: close\r\n\r\n");

    Serial.println("Request sent");

    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
            Serial.println("Headers received");
            break;
        }
    }

    String line = client.readStringUntil('\n');
    if (line.startsWith("{\"state\":\"success\"")) {
        Serial.println("Data upload successful!");
    } else {
        Serial.println("Data upload failed");
    }

    Serial.print("Response: ");
    Serial.println(line);
    Serial.println("Closing connection");
    client.stop();
    Serial.println("==========\n");
}