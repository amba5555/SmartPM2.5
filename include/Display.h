#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AQICalculator.h"

class Display {
public:
    Display(int width = 128, int height = 64);
    void begin();
    void showReadings(const float pm2_5, const int aqi, const String& healthMsg);
    void showError(const String& error);
    void showStatus(const String& status);
    
    // Public access to OLED for partial updates (anti-flicker)
    Adafruit_SSD1306 _oled;

private:
    void clearAndSetBasicLayout();
};

#endif // DISPLAY_H