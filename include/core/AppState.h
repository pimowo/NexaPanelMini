#pragma once
#include <Arduino.h>

struct WeatherDay {
    String dayName;
    String condition;
    float tempMin = NAN;
    float tempMax = NAN;
};

struct AppState {
    uint32_t timeRevision = 0;
    uint32_t weatherRevision = 0;
    uint32_t radioRevision = 0;
    uint32_t boilerRevision = 0;

    // Czas / NTP
    bool timeValid = false;
    String timeText = "--:--";
    String dateText = "--.--.----";
    String weekdayText = "---";

    // Pogoda
    bool weatherValid = false;
    float outsideTemp = NAN;
    String weatherText = "---";
    float todayMin = NAN;
    float todayMax = NAN;
    WeatherDay forecast[5];

    // yoRadio
    bool radioOnline = false;
    bool radioPlaying = false;
    int radioVolume = 0;
    String radioStation = "---";
    String radioArtist = "---";
    String radioTitle = "---";

    // Kocioł
    bool boilerOnline = false;
    bool boilerEnabled = false;
    float boilerCurrentTemp = NAN;
    float boilerTargetTemp = NAN;

    // System
    bool wifiConnected = false;
    int wifiRssi = -127;
    String ipAddress = "0.0.0.0";
};
