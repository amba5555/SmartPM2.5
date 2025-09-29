#ifndef MQTTCONFIG_H
#define MQTTCONFIG_H

namespace MQTTConfig {
    // HiveMQ Cloud Configuration
    extern const char* MQTT_BROKER;       // Your HiveMQ Cloud broker URL
    extern const char* MQTT_USERNAME;     // Your HiveMQ Cloud username
    extern const char* MQTT_PASSWORD;     // Your HiveMQ Cloud password

    // Device Configuration
    extern const char* DEVICE_ID;         // Unique device identifier

    // MQTT Topics (using dots for HiveMQ Cloud compatibility)
    extern const char* TOPIC_TELEMETRY;   // For publishing sensor data (smartpm25.sensor.data)
    extern const char* TOPIC_STATUS;      // For publishing device status (smartpm25.device.status)
    extern const char* TOPIC_COMMANDS;    // For subscribing to commands (smartpm25.device.commands)
}

#endif // MQTTCONFIG_H