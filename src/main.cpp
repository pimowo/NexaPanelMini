#include <Arduino.h>

#include "config.h"
#include "pins.h"
#include "version.h"
#include "core/AppState.h"
#include "core/Navigation.h"
#include "display/DisplayDriver.h"
#include "display/TouchDriver.h"
#include "ui/UiManager.h"
#include "services/WifiService.h"
#include "services/TimeService.h"
#include "services/WeatherService.h"
#include "services/RadioService.h"
#include "services/BoilerService.h"

AppState appState;
Navigation navigation;
DisplayDriver display;
TouchDriver touch;
UiManager ui;

WifiService wifiService;
TimeService timeService;
WeatherService weatherService;
RadioService radioService;
BoilerService boilerService;

namespace {

uint32_t lastDiagnosticMs = 0;

void printStartupDiagnostics() {
    Serial.println();
    Serial.println("=== NexaPanel Mini diagnostics ===");
    Serial.printf("Firmware: %s\n", FW_VERSION);
    Serial.printf("Chip: ESP8266, flash=%lu bytes, heap=%lu bytes\n",
                  static_cast<unsigned long>(ESP.getFlashChipRealSize()),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    Serial.printf("Reset: %s\n", ESP.getResetReason().c_str());
    Serial.printf("SPI: SCK=D5 MISO=D6 MOSI=D7 TFT_CS=D8 TOUCH_CS=D2\n");
    Serial.println("BOOT WARNING: D8/GPIO15 must stay LOW; D3/GPIO0 and D4/GPIO2 must stay HIGH during reset");
    Serial.println("BACKLIGHT: LCD BL is permanently powered; no ESP8266 control line");
}

void printHealthDiagnostics() {
    const uint32_t now = millis();
    if (now - lastDiagnosticMs < AppConfig::DIAGNOSTIC_INTERVAL_MS) return;
    lastDiagnosticMs = now;

    Serial.printf("HEALTH uptime=%lu heap=%lu max_block=%lu frag=%u%% wifi=%s rssi=%d ntp=%s\n",
                  static_cast<unsigned long>(now / 1000UL),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getMaxFreeBlockSize()),
                  ESP.getHeapFragmentation(),
                  appState.wifiConnected ? "online" : "offline",
                  appState.wifiRssi,
                  appState.timeValid ? "valid" : "waiting");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(50);
    printStartupDiagnostics();

    navigation.begin();
    display.begin();
    touch.begin();

    wifiService.begin();
    timeService.begin();
    weatherService.begin();
    radioService.begin(appState);
    boilerService.begin(appState);

    ui.begin(display, touch, navigation, radioService, boilerService);
}

void loop() {
    wifiService.update(appState);
    timeService.update(appState);
    weatherService.update(appState);
    radioService.update(appState);
    boilerService.update(appState);

    ui.update(appState);
    printHealthDiagnostics();

    delay(5);
}
