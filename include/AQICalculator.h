#ifndef AQICALCULATOR_H
#define AQICALCULATOR_H

#include <Arduino.h>

class AQICalculator {
public:
    struct AQIResult {
        int value;
        String category;
        String healthMessage;
        uint16_t color;
    };

    static AQIResult calculateAQI(float pm2_5);

private:
    static const int BREAKPOINTS[][2];
    static const int AQI_LEVELS[][2];
    static const char* CATEGORIES[];
    static const char* HEALTH_MESSAGES[];
    static const uint16_t COLORS[];
};

#endif // AQICALCULATOR_H