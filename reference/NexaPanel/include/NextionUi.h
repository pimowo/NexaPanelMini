#pragma once

#include <Arduino.h>

#include "config.h"

struct AppState;
class ClockService;
class HomeAssistantMqtt;
class NextionDriver;
class RuntimeConfig;
class YoRadioClient;
struct NextionTouchEvent;
enum class UiPage : uint8_t;

enum class UpdateType : uint8_t {
  NEXTION,
  ESP_FIRMWARE,
};

class NextionUi {
 public:
  NextionUi(AppState& state, NextionDriver& driver,
            YoRadioClient& yoRadio, ClockService& clock,
            HomeAssistantMqtt& mqtt, const RuntimeConfig& config);
  void begin(uint32_t firmwareBootStartedMs);
  void loop();
  bool showUpdateScreen(UpdateType type);
  bool showUpdateProgress(uint8_t percent);
  bool showUpdateSuccess();
  bool showUpdateError(const char* message);
  bool updateScreenPrepared() const;

 private:
  enum class BootStatus : uint8_t {
    UNKNOWN,
    STARTING,
    ERROR_WIFI,
    ERROR_NTP,
  };

  enum class UpdateScreenPhase : uint8_t {
    IDLE,
    WAIT_PAGE_TX,
    WAIT_PAGE_SETTLE,
    WAIT_TEXT_TX,
  };

  enum class CalendarMarqueePhase : uint8_t {
    IDLE,
    START_PAUSE,
    SCROLLING,
    END_PAUSE,
  };

  struct CalendarMarqueeState {
    char fullText[Config::HomeAssistant::CALENDAR_ENTRY_MAX_BYTES + 1]{};
    char visibleText[Config::HomeAssistant::CALENDAR_ENTRY_MAX_BYTES + 1]{};
    uint16_t offsetCharacters{0};
    uint32_t nextStepMs{0};
    CalendarMarqueePhase phase{CalendarMarqueePhase::IDLE};
    bool scrollNeeded{false};
  };

  struct RadioCache {
    String station;
    String artist;
    String title;
    String bitrate;
    String volume;
    uint8_t playback{0};
    bool stationValid{false};
    bool artistValid{false};
    bool titleValid{false};
    bool bitrateValid{false};
    bool volumeValid{false};
    bool playbackValid{false};
  };

  struct StartCache {
    struct WasteEntry {
      char text[Config::HomeAssistant::WASTE_TEXT_MAX_BYTES + 1]{};
      uint16_t color{0};
      uint16_t picture{0};
      bool textValid{false};
      bool colorValid{false};
      bool pictureValid{false};
    };

    char clock[6]{};
    char date[11]{};
    char weatherTemperature[24]{};
    char calendarToday[4]
                      [Config::HomeAssistant::CALENDAR_ENTRY_MAX_BYTES + 1]{};
    char calendarTomorrow[4]
                         [Config::HomeAssistant::CALENDAR_ENTRY_MAX_BYTES + 1]{};
    uint16_t calendarTodayColor[4]{};
    uint16_t calendarTomorrowColor[4]{};
    bool clockValid{false};
    bool dateValid{false};
    bool weatherTemperatureValid{false};
    bool calendarTodayValid[4]{};
    bool calendarTomorrowValid[4]{};
    bool calendarTodayColorValid[4]{};
    bool calendarTomorrowColorValid[4]{};
    WasteEntry waste[4]{};
  };

  struct BoilerCache {
    char currentTemperature[16]{};
    char targetTemperature[16]{};
    char weatherTemperature[24]{};
    uint16_t heatPicture{0};
    uint16_t modePicture{0};
    uint16_t firePicture{0};
    uint16_t powerPicture{0};
    uint16_t windowsPicture{0};
    bool currentTemperatureValid{false};
    bool targetTemperatureValid{false};
    bool weatherTemperatureValid{false};
    bool heatPictureValid{false};
    bool modePictureValid{false};
    bool firePictureValid{false};
    bool powerPictureValid{false};
    bool windowsPictureValid{false};
  };

  static void onTouch(void* context, const NextionTouchEvent& event);
  static void onPage(void* context, uint8_t pageId);
  static void onReady(void* context);
  static void onError(void* context, uint8_t errorCode);

  void handleTouch(const NextionTouchEvent& event);
  void handlePage(uint8_t pageId);
  void handleReady();
  void handleError(uint8_t errorCode);
  void processStartupSync();
  bool requestPhysicalBootPage();
  void startBootSplash();
  void processBootSplash();
  bool finishBootSplash();
  bool setBootStatus(BootStatus status, const char* text);
  bool setBootDots(uint8_t count);
  bool queueUpdateScreen(const char* status, const char* progress,
                         const char* warning);
  void dispatchNavigation(const NextionTouchEvent& event);
  void handleRadioTouch(const NextionTouchEvent& event);
  void handleBoilerTouch(const NextionTouchEvent& event);
  void processOptimisticTarget();
  void setCurrentPage(UiPage page);
  void processPageTimeout();
  void processVolumeHold();
  void startVolumeHold(bool volumeUp);
  void finishVolumeHold(bool volumeUp);
  bool sendVolumeCommand(bool volumeUp, bool logCommand);
  void processTemperatureHold();
  void startTemperatureHold(bool temperatureUp);
  void finishTemperatureHold(bool temperatureUp);
  bool sendTemperatureCommand(bool temperatureUp, bool logCommand);
  void renderStart();
  void syncCalendarMarquee(CalendarMarqueeState& marquee,
                           const char* fullText, uint32_t now);
  bool advanceCalendarMarquee(CalendarMarqueeState& marquee, uint32_t now);
  void resetCalendarMarquees();
  bool renderStartText(const char* component, const char* value,
                       char* cachedValue, size_t cachedSize,
                       bool& cacheValid, const char* logName, bool utf8,
                       bool logUpdate);
  bool renderStartColor(const char* component, uint16_t color,
                        uint16_t& cachedColor, bool& cacheValid,
                        const char* logName);
  bool renderStartWasteEntry(const char* textComponent,
                             const char* pictureComponent, const char* text,
                             uint16_t color, uint16_t picture,
                             StartCache::WasteEntry& cache,
                             const char* logName);
  void invalidateStartCache();
  void renderBoiler();
  bool renderBoilerText(const char* component, const char* value,
                        char* cachedValue, size_t cachedSize,
                        bool& cacheValid, const char* logName, bool utf8);
  bool renderBoilerPicture(const char* component, uint16_t pictureId,
                           uint16_t& cachedPicture, bool& cacheValid,
                           const char* logName);
  void invalidateBoilerCache();
  void renderRadio();
  void invalidateRadioCache();
  bool renderRadioText(const char* component, const String& value,
                       String& cachedValue, bool& cacheValid,
                       const char* logName, bool utf8);

  AppState& state_;
  NextionDriver& driver_;
  YoRadioClient& yoRadio_;
  ClockService& clock_;
  HomeAssistantMqtt& mqtt_;
  const RuntimeConfig& config_;
  StartCache startCache_;
  CalendarMarqueeState calendarTodayMarquee_[4];
  CalendarMarqueeState calendarTomorrowMarquee_[4];
  BoilerCache boilerCache_;
  RadioCache radioCache_;
  uint32_t nextStartupSyncMs_{0};
  uint32_t bootStartedMs_{0};
  uint32_t bootPageRequestedMs_{0};
  uint32_t nextBootDotMs_{0};
  uint32_t volumeHoldStartedMs_{0};
  uint32_t nextVolumeRepeatMs_{0};
  uint32_t temperatureHoldStartedMs_{0};
  uint32_t nextTemperatureRepeatMs_{0};
  uint32_t optimisticTargetSourceUpdateMs_{0};
  uint32_t updatePageSettledFromMs_{0};
  bool startupSyncActive_{false};
  bool startupSyncAttempted_{false};
  bool bootSplashStarted_{false};
  bool bootSplashCompleted_{false};
  bool bootPageRequestPending_{false};
  bool bootPageConfirmationPending_{false};
  bool bootPageVerificationSent_{false};
  bool bootStartPagePending_{false};
  bool bootClockReadyLogged_{false};
  bool bootRecoveredLogged_{false};
  bool updateScreenActive_{false};
  UpdateScreenPhase updateScreenPhase_{UpdateScreenPhase::IDLE};
  BootStatus bootStatus_{BootStatus::UNKNOWN};
  uint8_t nextBootDotCount_{1};
  uint8_t renderedBootDotCount_{0xFF};
  uint8_t nextCalendarMarqueeIndex_{0};
  bool volumeHoldActive_{false};
  bool volumeHoldUp_{false};
  bool volumeHoldCommandStarted_{false};
  bool temperatureHoldActive_{false};
  bool temperatureHoldUp_{false};
  bool temperatureHoldCommandStarted_{false};
};
