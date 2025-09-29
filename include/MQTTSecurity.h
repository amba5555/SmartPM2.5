#ifndef MQTT_SECURITY_H
#define MQTT_SECURITY_H

#include <Arduino.h>
#include <WiFiClientSecure.h>

// HiveMQ Cloud Root CA Certificate
extern const char* ROOT_CA_CERT;

class MQTTSecurity {
public:
    static void configureSecureClient(WiFiClientSecure& client) {
        // Set Root CA certificate
        client.setCACert(ROOT_CA_CERT);
        
        // Set secure client options
        client.setTimeout(15000); // 15 seconds timeout (in milliseconds)
        client.setHandshakeTimeout(30000); // 30 seconds for handshake (in milliseconds)
        
        // Note: Session resumption is handled automatically by WiFiClientSecure
    }
    
    static bool validateDeviceId(const char* deviceId) {
        // Check if device ID follows our format
        if (strlen(deviceId) < 8) return false;
        if (strncmp(deviceId, "ESP32_PM25_", 11) != 0) return false;
        return true;
    }
    
    static void secureDisconnect(WiFiClientSecure& client) {
        // Properly close SSL connection
        client.stop();
        delay(100); // Small delay to ensure proper cleanup
    }
};

#endif // MQTT_SECURITY_H