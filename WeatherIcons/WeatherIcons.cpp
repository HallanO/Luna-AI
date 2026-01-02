#include "WeatherIcons.h"
#include "bitmaps/clear_night.h"
#include "bitmaps/clear_day.h"
#include "bitmaps/snowing.h"
#include "bitmaps/foggy.h"
#include "bitmaps/thunderstorm.h"
#include "bitmaps/cloudy.h"
#include "bitmaps/partly_cloudy_night.h"
#include "bitmaps/partly_cloudy_day.h"

const unsigned char* WeatherIcons::getIcon(String condition, bool isDaytime) {
    condition.toLowerCase();
    
    // Clear/Sunny
    if (condition.indexOf("sunny") >= 0 || condition.indexOf("clear") >= 0) {
        return isDaytime ? getClearDay() : getClearNight();
    }
    // Partly Cloudy
    else if (condition.indexOf("partly cloudy") >= 0) {
        return isDaytime ? getPartlyCloudyDay() : getPartlyCloudyNight();
    }
    // Cloudy/Overcast
    else if (condition.indexOf("cloudy") >= 0 || condition.indexOf("overcast") >= 0) {
        return getCloudy();
    }
    // Rain
    else if (condition.indexOf("rain") >= 0 || condition.indexOf("drizzle") >= 0) {
        return getThunderstorm(); // Using thunderstorm icon for rain
    }
    // Thunderstorm
    else if (condition.indexOf("thunder") >= 0 || condition.indexOf("storm") >= 0) {
        return getThunderstorm();
    }
    // Snow
    else if (condition.indexOf("snow") >= 0) {
        return getSnowing();
    }
    // Fog/Mist
    else if (condition.indexOf("mist") >= 0 || condition.indexOf("fog") >= 0) {
        return getFoggy();
    }
    
    // Default to cloudy
    return getCloudy();
}

const unsigned char* WeatherIcons::getClearNight() {
    return CLEAR_NIGHT_BITMAP;
}

const unsigned char* WeatherIcons::getClearDay() {
    return CLEAR_DAY_BITMAP;
}

const unsigned char* WeatherIcons::getSnowing() {
    return SNOWING_BITMAP;
}

const unsigned char* WeatherIcons::getFoggy() {
    return FOGGY_BITMAP;
}

const unsigned char* WeatherIcons::getThunderstorm() {
    return THUNDERSTORM_BITMAP;
}

const unsigned char* WeatherIcons::getCloudy() {
    return CLOUDY_BITMAP;
}

const unsigned char* WeatherIcons::getPartlyCloudyNight() {
    return PARTLY_CLOUDY_NIGHT_BITMAP;
}

const unsigned char* WeatherIcons::getPartlyCloudyDay() {
    return PARTLY_CLOUDY_DAY_BITMAP;
}

bool WeatherIcons::isDaytime(int hour) {
    return (hour >= 6 && hour < 18);
}