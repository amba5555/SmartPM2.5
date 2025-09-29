#include "AQICalculator.h"

const int AQICalculator::BREAKPOINTS[][2] = {
    {0, 12},      // Good
    {12, 35},     // Moderate
    {35, 55},     // Unhealthy for Sensitive Groups
    {55, 150},    // Unhealthy
    {150, 250},   // Very Unhealthy
    {250, 500}    // Hazardous
};

const int AQICalculator::AQI_LEVELS[][2] = {
    {0, 50},      // Good
    {51, 100},    // Moderate
    {101, 150},   // Unhealthy for Sensitive Groups
    {151, 200},   // Unhealthy
    {201, 300},   // Very Unhealthy
    {301, 500}    // Hazardous
};

const char* AQICalculator::CATEGORIES[] = {
    "Good",
    "Moderate",
    "Sensitive",
    "Unhealthy",
    "Very Unhealthy",
    "Hazardous"
};

const char* AQICalculator::HEALTH_MESSAGES[] = {
    "Air quality is good",
    "Moderate health concern",
    "Sensitive groups at risk",
    "Everyone may experience effects",
    "Health warnings, avoid activity",
    "Health alert: everyone at risk"
};

const uint16_t AQICalculator::COLORS[] = {
    static_cast<uint16_t>(0x07E0),  // Green
    static_cast<uint16_t>(0xFFE0),  // Yellow
    static_cast<uint16_t>(0xFD20),  // Orange
    static_cast<uint16_t>(0xF800),  // Red
    static_cast<uint16_t>(0x780F),  // Purple
    static_cast<uint16_t>(0x7800)   // Maroon
};

AQICalculator::AQIResult AQICalculator::calculateAQI(float pm2_5) {
    AQIResult result;
    
    // Find the appropriate breakpoint category
    int category = 0;
    for (int i = 0; i < 6; i++) {
        if (pm2_5 <= BREAKPOINTS[i][1]) {
            category = i;
            break;
        }
    }
    
    // Calculate AQI using linear interpolation
    float aqi = (AQI_LEVELS[category][1] - AQI_LEVELS[category][0]) /
                (BREAKPOINTS[category][1] - BREAKPOINTS[category][0]) *
                (pm2_5 - BREAKPOINTS[category][0]) + AQI_LEVELS[category][0];
    
    result.value = (int)aqi;
    result.category = CATEGORIES[category];
    result.healthMessage = HEALTH_MESSAGES[category];
    result.color = COLORS[category];
    
    return result;
}