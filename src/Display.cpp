#include "Display.h"

Display::Display(int width, int height) : _oled(width, height, &Wire, -1) {}

void Display::begin() {
    if (!_oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        return;
    }
    _oled.clearDisplay();
    _oled.display();
}

void Display::showReadings(const float pm2_5, const int aqi, const String& healthMsg) {
    clearAndSetBasicLayout();
    
    // Show PM2.5 Reading
    _oled.setTextSize(2);
    _oled.setCursor(0, 0);
    _oled.println("PM2.5");
    _oled.setTextSize(3);
    _oled.println(pm2_5);
    
    // Show AQI
    _oled.setTextSize(1);
    _oled.print("AQI: ");
    _oled.println(aqi);
    
    // Show health message (scrolling if needed)
    _oled.setTextSize(1);
    _oled.println(healthMsg);
    
    _oled.display();
}

void Display::showError(const String& error) {
    clearAndSetBasicLayout();
    _oled.setTextSize(1);
    _oled.println("ERROR:");
    _oled.println(error);
    _oled.display();
}

void Display::showStatus(const String& status) {
    clearAndSetBasicLayout();
    _oled.setTextSize(1);
    _oled.println(status);
    _oled.display();
}

void Display::clearAndSetBasicLayout() {
    _oled.clearDisplay();
    _oled.setTextColor(WHITE, BLACK);
    _oled.setCursor(0, 0);
}