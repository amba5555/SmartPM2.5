#include "MQTTManager.h"
#include "MQTTConfig.h"
#include "MQTTSecurity.h"

MQTTManager::MQTTManager() : mqttClient(wifiClient) {
}

void MQTTManager::begin() {
    initSecureClient();
    mqttClient.setServer(MQTTConfig::MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(callback);
}

bool MQTTManager::connect() {
    if (mqttClient.connected()) {
        return true;
    }

    Serial.println("Connecting to MQTT broker...");
    Serial.print("Broker: "); Serial.println(MQTTConfig::MQTT_BROKER);
    Serial.print("Username: "); Serial.println(MQTTConfig::MQTT_USERNAME);
    Serial.print("Client ID: "); Serial.println(MQTTConfig::DEVICE_ID);
    
    // Set shorter keep alive interval
    mqttClient.setKeepAlive(60);
    mqttClient.setSocketTimeout(10); // 10 seconds timeout
    
    // First try connecting without Will Message
    if (mqttClient.connect(MQTTConfig::DEVICE_ID, MQTTConfig::MQTT_USERNAME, MQTTConfig::MQTT_PASSWORD)) {
        
        Serial.println("connected!");
        
        // Publish online status
        mqttClient.publish(MQTTConfig::TOPIC_STATUS, "{\"status\":\"online\"}", true);
        
        // Subscribe to command topic
        mqttClient.subscribe(MQTTConfig::TOPIC_COMMANDS, 1);
        
        return true;
    }

    // Print detailed error information
    int state = mqttClient.state();
    Serial.print("Connection failed! Error code = ");
    Serial.print(state);
    Serial.print(" : ");
    
    switch(state) {
        case -4: Serial.println("MQTT_CONNECTION_TIMEOUT"); break;
        case -3: Serial.println("MQTT_CONNECTION_LOST"); break;
        case -2: Serial.println("MQTT_CONNECT_FAILED"); break;
        case -1: Serial.println("MQTT_DISCONNECTED"); break;
        case 1: Serial.println("MQTT_CONNECT_BAD_PROTOCOL"); break;
        case 2: Serial.println("MQTT_CONNECT_BAD_CLIENT_ID"); break;
        case 3: Serial.println("MQTT_CONNECT_UNAVAILABLE"); break;
        case 4: Serial.println("MQTT_CONNECT_BAD_CREDENTIALS"); break;
        case 5: Serial.println("MQTT_CONNECT_UNAUTHORIZED"); break;
        default: Serial.println("MQTT_UNKNOWN_ERROR"); break;
    }
    
    return false;
}

bool MQTTManager::publish(const char* topic, const char* payload) {
    if (!mqttClient.connected()) {
        Serial.println("Failed to publish - not connected");
        return false;
    }
    
    Serial.print("Publishing to topic: ");
    Serial.println(topic);
    Serial.print("Payload: ");
    Serial.println(payload);
    
    bool success = mqttClient.publish(topic, payload);
    if (success) {
        Serial.println("Message published successfully");
    } else {
        Serial.println("Failed to publish message");
    }
    return success;
}

bool MQTTManager::isConnected() {
    return mqttClient.connected();
}

void MQTTManager::loop() {
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            if (connect()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        mqttClient.loop();
    }
}

void MQTTManager::callback(char* topic, byte* payload, unsigned int length) {
    // Handle incoming messages here
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.printf("Message arrived [%s]: %s\n", topic, message.c_str());
}

bool MQTTManager::initSecureClient() {
    // Configure secure client with proper certificates and settings
    MQTTSecurity::configureSecureClient(wifiClient);
    
    // Set MQTT buffer size for larger messages
    mqttClient.setBufferSize(2048);
    
    // Set MQTT timeouts
    mqttClient.setSocketTimeout(10);
    
    // Validate device ID
    if (!MQTTSecurity::validateDeviceId(MQTTConfig::DEVICE_ID)) {
        Serial.println("Warning: Device ID format is not secure!");
        return false;
    }
    
    return true;
}