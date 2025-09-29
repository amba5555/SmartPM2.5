#include "PM25Sensor.h"

PM25Sensor::PM25Sensor(int rxPin, int txPin) : _serial(rxPin, txPin) {}

void PM25Sensor::begin() {
    _serial.begin(9600);
}

PM25Sensor::PMData PM25Sensor::read() {
    PMData data = {0, 0, 0, false};
    int idx = 0;
    
    // Wait for data header
    while (_serial.available() >= 2) {
        if (_serial.read() != 0x42) continue;
        if (_serial.read() != 0x4D) continue;
        
        // Found header, read remaining data
        _buffer[0] = 0x42;
        _buffer[1] = 0x4D;
        idx = 2;
        
        // Read remaining 30 bytes
        unsigned long timeout = millis() + 1000;  // 1 second timeout
        while (idx < 32 && millis() < timeout) {
            if (_serial.available()) {
                _buffer[idx++] = _serial.read();
            }
        }
        
        if (idx != 32) {
            return data;  // Timeout or incomplete data
        }
        
        // Validate checksum
        if (!validateChecksum(_buffer, 32)) {
            return data;
        }
        
        // Parse data
        data.pm1 = (_buffer[4] << 8) | _buffer[5];
        data.pm2_5 = (_buffer[6] << 8) | _buffer[7];
        data.pm10 = (_buffer[8] << 8) | _buffer[9];
        data.isValid = true;
        
        break;
    }
    
    // Clear buffer
    while (_serial.available()) {
        _serial.read();
    }
    
    return data;
}

bool PM25Sensor::validateChecksum(uint8_t* buffer, int length) {
    uint16_t sum = 0;
    for (int i = 0; i < length - 2; i++) {
        sum += buffer[i];
    }
    
    uint16_t received_checksum = (buffer[length-2] << 8) | buffer[length-1];
    return sum == received_checksum;
}