#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <Arduino.h>

// Icon dimensions
#define WEATHER_ICON_WIDTH 64
#define WEATHER_ICON_HEIGHT 64

class WeatherIcons {
public:
    // Get icon bitmap based on condition and time of day
    static const unsigned char* getIcon(String condition, bool isDaytime);
    
    // Individual icon getters
    static const unsigned char* getClearNight();
    static const unsigned char* getClearDay();
    static const unsigned char* getSnowing();
    static const unsigned char* getFoggy();
    static const unsigned char* getThunderstorm();
    static const unsigned char* getCloudy();
    static const unsigned char* getPartlyCloudyNight();
    static const unsigned char* getPartlyCloudyDay();
    
    // Utility function to determine if it's daytime
    static bool isDaytime(int hour);
};

#endif