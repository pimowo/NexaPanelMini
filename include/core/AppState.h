#pragma once
#include <Arduino.h>

struct WeatherDay {
    String dateText;
    int weatherCode = -1;
    float tempMin = NAN;
    float tempMax = NAN;
};

enum class BoilerPreset : uint8_t {
    MANUAL,
    COMFORT,
    SLEEP
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
    int currentWeatherCode = -1;
    float outsideTemp = NAN;
    float outsidePressure = NAN;
    float todayMin = NAN;
    float todayMax = NAN;
    WeatherDay forecast[4];

    // Temperatura zewnętrzna z HA (sensor.temperatura_zewnetrzna)
    bool haOutsideTempValid = false;
    float haOutsideTemp = NAN;
    uint32_t haOutsideTempLastUpdateMs = 0;
    bool haOutsidePressureValid = false;
    float haOutsidePressure = NAN;
    uint32_t haOutsidePressureLastUpdateMs = 0;

    // yoRadio
    bool radioOnline = false;
    bool radioOfflineError = false;
    bool radioPlaying = false;
    int radioVolume = 0;
    int radioBitrate = 0;
    String radioStation = "---";
    String radioArtist = "---";
    String radioTitle = "---";

    // Kocioł
    bool boilerOnline = false;
    bool boilerEnabled = false;
    bool boilerPowerOn = false;
    bool boilerHvacHeat = false;
    BoilerPreset boilerPreset = BoilerPreset::MANUAL;
    float boilerCurrentTemp = NAN;
    float boilerTargetTemp = NAN;

    // System
    bool wifiConnected = false;
    int wifiRssi = -127;
    String ipAddress = "0.0.0.0";
};
