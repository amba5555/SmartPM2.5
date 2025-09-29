#ifndef PM25SENSOR_H
#define PM25SENSOR_H

#include <Arduino.h>
#include <SoftwareSerial.h>

class PM25Sensor {
public:
    struct PMData {
        unsigned int pm1;
        unsigned int pm2_5;
        unsigned int pm10;
        bool isValid;
    };

    PM25Sensor(int rxPin, int txPin);
    void begin();
    PMData read();
    bool validateChecksum(uint8_t* buffer, int length);

private:
    SoftwareSerial _serial;
    static const int BUFFER_SIZE = 32;
    uint8_t _buffer[BUFFER_SIZE];
};

#endif // PM25SENSOR_H