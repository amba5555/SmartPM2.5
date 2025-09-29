#ifndef MQTTMANAGER_H
#define MQTTMANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "MQTTConfig.h"
#include "MQTTSecurity.h"

class MQTTManager {
public:
    MQTTManager();
    void begin();
    bool connect();
    bool publish(const char* topic, const char* payload);
    bool isConnected();
    void loop();

private:
    static void callback(char* topic, byte* payload, unsigned int length);
    
    WiFiClientSecure wifiClient;
    PubSubClient mqttClient;
    
    // MQTT Configuration
    static const int MQTT_PORT = 8883;
    unsigned long lastReconnectAttempt = 0;
    static const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds
    
    bool initSecureClient();
};

#endif // MQTTMANAGER_H