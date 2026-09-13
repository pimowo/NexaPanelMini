#include "NextionUi.h"

#include <math.h>
#include <string.h>

#include "AppState.h"
#include "ClockService.h"
#include "HomeAssistantMqtt.h"
#include "NextionDriver.h"
#include "NextionIds.h"
#include "RuntimeConfig.h"
#include "YoRadioClient.h"
#include "config.h"

namespace {

enum class CalendarEntryType : uint8_t {
  NONE,
  MAGDA,
  PIOTREK,
};

constexpr const char* CALENDAR_TODAY_LOG_NAMES[4] = {
    "calendar today[0]", "calendar today[1]", "calendar today[2]",
    "calendar today[3]"};
constexpr const char* CALENDAR_TOMORROW_LOG_NAMES[4] = {
    "calendar tomorrow[0]", "calendar tomorrow[1]",
    "calendar tomorrow[2]", "calendar tomorrow[3]"};
constexpr const char* WASTE_LOG_NAMES[4] = {
    "mixed", "plastic", "paper", "glass"};

struct WasteUiValue {
  char text[Config::HomeAssistant::WASTE_TEXT_MAX_BYTES + 1]{};
  uint16_t color{NextionIds::Color::TEXT_DEFAULT};
  uint16_t picture{0};
};

bool isCalendarWhitespace(char character) {
  return character == ' ' || character == '\t' || character == '\r' ||
         character == '\n' || character == '\f' || character == '\v';
}

bool equalsAsciiIgnoreCase(const char* text, size_t length,
                           const char* expected) {
  const size_t expectedLength = strlen(expected);
  if (length != expectedLength) {
    return false;
  }
  for (size_t index = 0; index < length; ++index) {
    char actual = text[index];
    char wanted = expected[index];
    if (actual >= 'A' && actual <= 'Z') {
      actual = static_cast<char>(actual - 'A' + 'a');
    }
    if (wanted >= 'A' && wanted <= 'Z') {
      wanted = static_cast<char>(wanted - 'A' + 'a');
    }
    if (actual != wanted) {
      return false;
    }
  }
  return true;
}

bool isWasteToday(const char* text, size_t length) {
  // Home Assistant może przekazać nazwę dnia złożoną, rozłożoną albo bez
  // polskiego znaku. Wszystkie te warianty mają tę samą semantykę UI.
  if (equalsAsciiIgnoreCase(text, length, "Dzis") ||
      equalsAsciiIgnoreCase(text, length, "Dzisiaj")) {
    return true;
  }

  const bool composedPolishS =
      length == 5 &&
      static_cast<uint8_t>(text[3]) == 0xC5U &&
      (static_cast<uint8_t>(text[4]) == 0x9BU ||
       static_cast<uint8_t>(text[4]) == 0x9AU);
  const bool decomposedPolishS =
      length == 6 &&
      (text[3] == 's' || text[3] == 'S') &&
      static_cast<uint8_t>(text[4]) == 0xCCU &&
      static_cast<uint8_t>(text[5]) == 0x81U;
  const bool prefixMatches =
      (text[0] == 'D' || text[0] == 'd') &&
      (text[1] == 'Z' || text[1] == 'z') &&
      (text[2] == 'I' || text[2] == 'i');
  return prefixMatches && (composedPolishS || decomposedPolishS);
}

bool isWasteTomorrow(const char* text, size_t length) {
  return equalsAsciiIgnoreCase(text, length, "Jutro");
}

size_t utf8SequenceLength(const char* text, size_t remaining) {
  if (text == nullptr || remaining == 0) {
    return 0;
  }

  const uint8_t first = static_cast<uint8_t>(text[0]);
  size_t sequenceLength = 1;
  if ((first & 0xE0U) == 0xC0U) {
    sequenceLength = 2;
  } else if ((first & 0xF0U) == 0xE0U) {
    sequenceLength = 3;
  } else if ((first & 0xF8U) == 0xF0U) {
    sequenceLength = 4;
  }

  if (sequenceLength > remaining) {
    return 1;
  }
  for (size_t index = 1; index < sequenceLength; ++index) {
    if ((static_cast<uint8_t>(text[index]) & 0xC0U) != 0x80U) {
      return 1;
    }
  }
  return sequenceLength;
}

size_t utf8CharacterCount(const char* text, size_t byteLength) {
  size_t byteIndex = 0;
  size_t characterCount = 0;
  while (byteIndex < byteLength) {
    byteIndex += utf8SequenceLength(text + byteIndex, byteLength - byteIndex);
    ++characterCount;
  }
  return characterCount;
}

uint32_t utf8CodePoint(const char* text, size_t remaining,
                       size_t& sequenceLength) {
  sequenceLength = utf8SequenceLength(text, remaining);
  if (sequenceLength == 0) {
    return 0;
  }

  const uint8_t first = static_cast<uint8_t>(text[0]);
  if (sequenceLength == 1) {
    return first;
  }
  if (sequenceLength == 2) {
    return ((first & 0x1FU) << 6U) |
           (static_cast<uint8_t>(text[1]) & 0x3FU);
  }
  if (sequenceLength == 3) {
    return ((first & 0x0FU) << 12U) |
           ((static_cast<uint8_t>(text[1]) & 0x3FU) << 6U) |
           (static_cast<uint8_t>(text[2]) & 0x3FU);
  }
  return ((first & 0x07U) << 18U) |
         ((static_cast<uint8_t>(text[1]) & 0x3FU) << 12U) |
         ((static_cast<uint8_t>(text[2]) & 0x3FU) << 6U) |
         (static_cast<uint8_t>(text[3]) & 0x3FU);
}

uint8_t calendarGlyphWidth(uint32_t codePoint) {
  // Model istniejącego fontu Arial 16 (ID 1), skorygowany po teście HMI.
  if (codePoint == ' ') {
    return 4;
  }
  if (codePoint < 0x80U) {
    const char character = static_cast<char>(codePoint);
    if (strchr("!.,:;'|ijlI", character) != nullptr) {
      return 3;
    }
    if (strchr("frt()[]{}", character) != nullptr) {
      return 5;
    }
    if (strchr("mwMW@%&", character) != nullptr) {
      return 10;
    }
    if (character >= 'A' && character <= 'Z') {
      return 8;
    }
    return 7;
  }

  // Polskie litery w tym foncie mają szerokość zbliżoną do liter bazowych.
  return 8;
}

uint16_t calendarTextWidth(const char* text) {
  const size_t byteLength = strlen(text);
  size_t byteIndex = 0;
  uint16_t width = 0;
  while (byteIndex < byteLength) {
    size_t sequenceLength = 0;
    const uint32_t codePoint =
        utf8CodePoint(text + byteIndex, byteLength - byteIndex,
                      sequenceLength);
    width = static_cast<uint16_t>(width + calendarGlyphWidth(codePoint));
    byteIndex += sequenceLength;
  }
  return width;
}

size_t utf8ByteOffset(const char* text, uint16_t characterOffset) {
  const size_t byteLength = strlen(text);
  size_t byteIndex = 0;
  uint16_t characterIndex = 0;
  while (byteIndex < byteLength && characterIndex < characterOffset) {
    byteIndex += utf8SequenceLength(text + byteIndex, byteLength - byteIndex);
    ++characterIndex;
  }
  return byteIndex;
}

bool buildCalendarFragment(const char* fullText, uint16_t characterOffset,
                           char* output, size_t outputSize) {
  if (output == nullptr || outputSize == 0) {
    return true;
  }
  output[0] = '\0';

  const size_t byteLength = strlen(fullText);
  size_t byteIndex = utf8ByteOffset(fullText, characterOffset);
  size_t outputLength = 0;
  uint8_t outputCharacters = 0;
  uint16_t width = 0;
  while (byteIndex < byteLength) {
    size_t sequenceLength = 0;
    const uint32_t codePoint =
        utf8CodePoint(fullText + byteIndex, byteLength - byteIndex,
                      sequenceLength);
    const uint8_t glyphWidth = calendarGlyphWidth(codePoint);
    if (outputLength > 0 &&
        width + glyphWidth >
            Config::HomeAssistant::CALENDAR_VISIBLE_WIDTH_PX) {
      break;
    }
    if (outputCharacters >=
        Config::HomeAssistant::CALENDAR_COMPONENT_MAX_CHARS) {
      break;
    }
    if (outputLength + sequenceLength >= outputSize) {
      break;
    }
    memcpy(output + outputLength, fullText + byteIndex, sequenceLength);
    outputLength += sequenceLength;
    ++outputCharacters;
    byteIndex += sequenceLength;
    width = static_cast<uint16_t>(width + glyphWidth);
  }
  output[outputLength] = '\0';
  return byteIndex >= byteLength;
}

WasteUiValue formatWasteEntry(
    const StateText<Config::HomeAssistant::WASTE_TEXT_MAX_BYTES>& entry,
    bool wasteValid, uint16_t grayPicture, uint16_t colorPicture) {
  WasteUiValue result;
  result.picture = grayPicture;
  if (!wasteValid || !entry.received || !entry.valid) {
    return result;
  }

  const char* begin = entry.value;
  const char* end = begin + strlen(begin);
  while (begin < end && isCalendarWhitespace(*begin)) {
    ++begin;
  }
  while (end > begin && isCalendarWhitespace(*(end - 1))) {
    --end;
  }
  const size_t byteLength = static_cast<size_t>(end - begin);
  if (byteLength == 0 ||
      equalsAsciiIgnoreCase(begin, byteLength, "unknown") ||
      equalsAsciiIgnoreCase(begin, byteLength, "unavailable") ||
      equalsAsciiIgnoreCase(begin, byteLength, "null") ||
      (byteLength == 3 && memcmp(begin, "---", 3) == 0)) {
    return result;
  }

  const bool today = isWasteToday(begin, byteLength);
  const bool tomorrow = isWasteTomorrow(begin, byteLength);
  const bool urgent = today || tomorrow;
  if (today) {
    snprintf(result.text, sizeof(result.text), "Dziś");
  } else if (tomorrow) {
    snprintf(result.text, sizeof(result.text), "Jutro");
  } else {
    if (utf8CharacterCount(begin, byteLength) >
        Config::HomeAssistant::WASTE_UI_MAX_CHARS) {
      return result;
    }
    memcpy(result.text, begin, byteLength);
    result.text[byteLength] = '\0';
  }

  if (urgent) {
    result.color = NextionIds::Color::WASTE_ACTIVE;
    result.picture = colorPicture;
  }
  return result;
}

CalendarEntryType formatCalendarEntry(
    const StateText<Config::HomeAssistant::CALENDAR_ENTRY_MAX_BYTES>& entry,
    bool calendarValid, char* output, size_t outputSize) {
  if (output == nullptr || outputSize == 0) {
    return CalendarEntryType::NONE;
  }
  output[0] = '\0';
  if (!calendarValid || !entry.received || !entry.valid) {
    return CalendarEntryType::NONE;
  }

  const char* begin = entry.value;
  const char* end = begin + strlen(begin);
  while (begin < end && isCalendarWhitespace(*begin)) {
    ++begin;
  }
  while (end > begin && isCalendarWhitespace(*(end - 1))) {
    --end;
  }

  CalendarEntryType type = CalendarEntryType::NONE;
  if (end - begin >= 2 && begin[1] == '|') {
    if (begin[0] == 'R') {
      type = CalendarEntryType::MAGDA;
      begin += 2;
    } else if (begin[0] == 'B') {
      type = CalendarEntryType::PIOTREK;
      begin += 2;
    }
  }
  while (begin < end && isCalendarWhitespace(*begin)) {
    ++begin;
  }
  while (end > begin && isCalendarWhitespace(*(end - 1))) {
    --end;
  }
  if (begin == end) {
    return type;
  }

  const size_t byteLength = static_cast<size_t>(end - begin);
  if (byteLength >= outputSize) {
    return type;
  }

  memcpy(output, begin, byteLength);
  output[byteLength] = '\0';
  return type;
}

uint16_t calendarEntryColor(CalendarEntryType type) {
  switch (type) {
    case CalendarEntryType::MAGDA:
      return NextionIds::Color::CALENDAR_MAGDA;
    case CalendarEntryType::PIOTREK:
      return NextionIds::Color::CALENDAR_PIOTREK;
    case CalendarEntryType::NONE:
    default:
      return NextionIds::Color::TEXT_DEFAULT;
  }
}

UiPage pageFromId(uint8_t pageId) {
  switch (pageId) {
    case NextionIds::Page::BOOT:
      return UiPage::BOOT;
    case NextionIds::Page::START:
      return UiPage::START;
    case NextionIds::Page::RADIO:
      return UiPage::RADIO;
    case NextionIds::Page::BOILER:
      return UiPage::BOILER;
    case NextionIds::Page::UPDATE:
      return UiPage::UPDATE;
    default:
      return UiPage::UNKNOWN;
  }
}

const char* pageName(UiPage page) {
  switch (page) {
    case UiPage::BOOT:
      return "BOOT";
    case UiPage::START:
      return "START";
    case UiPage::RADIO:
      return "RADIO";
    case UiPage::BOILER:
      return "KOCIOŁ";
    case UiPage::UPDATE:
      return "UPDATE";
    case UiPage::UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

void formatTemperature(const StateValue<float>& temperature, char* buffer,
                       size_t bufferSize) {
  if (buffer == nullptr || bufferSize == 0) {
    return;
  }
  buffer[0] = '\0';
  if (!temperature.received || !temperature.valid ||
      !isfinite(temperature.value)) {
    return;
  }
  snprintf(buffer, bufferSize, "%.1f", temperature.value);
}

void formatWeatherTemperature(const StateValue<float>& temperature,
                              char* buffer, size_t bufferSize) {
  if (buffer == nullptr || bufferSize == 0) {
    return;
  }
  buffer[0] = '\0';
  if (!temperature.received || !temperature.valid ||
      !isfinite(temperature.value) ||
      temperature.value <
          Config::HomeAssistant::WEATHER_TEMPERATURE_MIN_C ||
      temperature.value >
          Config::HomeAssistant::WEATHER_TEMPERATURE_MAX_C) {
    return;
  }
  snprintf(buffer, bufferSize, "Temp %.1f\xC2\xB0" "C", temperature.value);
}

String utf8Prefix(const String& source, size_t maxCharacters) {
  size_t byteIndex = 0;
  size_t characterCount = 0;
  const size_t byteLength = source.length();

  while (byteIndex < byteLength && characterCount < maxCharacters) {
    const uint8_t first = static_cast<uint8_t>(source[byteIndex]);
    size_t sequenceLength = 1;
    if ((first & 0xE0U) == 0xC0U) {
      sequenceLength = 2;
    } else if ((first & 0xF0U) == 0xE0U) {
      sequenceLength = 3;
    } else if ((first & 0xF8U) == 0xF0U) {
      sequenceLength = 4;
    }

    if (byteIndex + sequenceLength > byteLength) {
      sequenceLength = 1;
    } else {
      for (size_t offset = 1; offset < sequenceLength; ++offset) {
        const uint8_t continuation =
            static_cast<uint8_t>(source[byteIndex + offset]);
        if ((continuation & 0xC0U) != 0x80U) {
          sequenceLength = 1;
          break;
        }
      }
    }

    byteIndex += sequenceLength;
    ++characterCount;
  }

  return byteIndex < byteLength ? source.substring(0, byteIndex) : source;
}

}  // namespace

NextionUi::NextionUi(AppState& state, NextionDriver& driver,
                     YoRadioClient& yoRadio, ClockService& clock,
                     HomeAssistantMqtt& mqtt, const RuntimeConfig& config)
    : state_(state),
      driver_(driver),
      yoRadio_(yoRadio),
      clock_(clock),
      mqtt_(mqtt),
      config_(config) {}

void NextionUi::begin(uint32_t firmwareBootStartedMs) {
  driver_.setTouchCallback(onTouch, this);
  driver_.setPageCallback(onPage, this);
  driver_.setReadyCallback(onReady, this);
  driver_.setErrorCallback(onError, this);
  bootStartedMs_ = firmwareBootStartedMs;
  nextStartupSyncMs_ = millis() + Config::Nextion::STARTUP_SYNC_SETTLE_MS;
  startupSyncActive_ = true;
  startupSyncAttempted_ = false;
  state_.ui.nextionSynchronized = false;
  state_.ui.activePageCacheValid = false;
  Serial.println("BOOT START");
}

void NextionUi::loop() {
  if (driver_.isUpdateMode()) {
    return;
  }

  if (updateScreenActive_) {
    return;
  }

  processStartupSync();

  processBootSplash();
  processPageTimeout();
  processVolumeHold();
  processTemperatureHold();
  processOptimisticTarget();

  // Podczas fizycznego przełączania na BOOT nie dokładamy do kolejki komend
  // komponentów strony START, RADIO ani KOCIOŁ.
  const bool bootPageTransitionPending =
      bootPageRequestPending_ || bootPageConfirmationPending_;
  if (state_.ui.nextionSynchronized && !bootPageTransitionPending) {
    if (state_.ui.currentPage == UiPage::START) {
      renderStart();
    } else if (state_.ui.currentPage == UiPage::RADIO) {
      renderRadio();
    } else if (state_.ui.currentPage == UiPage::BOILER) {
      renderBoiler();
    }
  }
}

bool NextionUi::showUpdateScreen(UpdateType type) {
  const char* status = type == UpdateType::NEXTION
                           ? "Aktualizacja Nextion"
                           : "Aktualizacja firmware";
  updateScreenActive_ = true;
  return queueUpdateScreen(status, "Przygotowanie...",
                           "Nie wyłączaj zasilania");
}

bool NextionUi::showUpdateProgress(uint8_t percent) {
  if (percent > 100U) {
    return false;
  }

  updateScreenActive_ = true;
  char text[5]{};
  snprintf(text, sizeof(text), "%u%%", percent);
  return driver_.setText(NextionIds::UpdateObject::PROGRESS, text);
}

bool NextionUi::showUpdateSuccess() {
  updateScreenActive_ = true;
  return queueUpdateScreen("Aktualizacja zakończona", "Restart...",
                           "Nie wyłączaj zasilania");
}

bool NextionUi::showUpdateError(const char* message) {
  updateScreenActive_ = true;
  return queueUpdateScreen("Aktualizacja nieudana", message,
                           "Sprawdź komunikat WWW");
}

bool NextionUi::updateScreenPrepared() const {
  return updateScreenActive_ &&
         updateScreenPhase_ == UpdateScreenPhase::IDLE &&
         state_.ui.currentPage == UiPage::UPDATE &&
         driver_.commandQueueIdle();
}

bool NextionUi::queueUpdateScreen(const char* status, const char* progress,
                                   const char* warning) {
  if (status == nullptr || progress == nullptr || warning == nullptr) {
    return false;
  }

  if (updateScreenPhase_ == UpdateScreenPhase::WAIT_TEXT_TX) {
    if (!driver_.commandQueueIdle()) {
      return false;
    }
    driver_.flushNormalTx();
    updateScreenPhase_ = UpdateScreenPhase::IDLE;
    return true;
  }

  if (updateScreenPhase_ == UpdateScreenPhase::IDLE &&
      state_.ui.currentPage != UiPage::UPDATE) {
    if (!driver_.canQueueCommands(1) ||
        !driver_.changePage(NextionIds::Page::UPDATE)) {
      return false;
    }
    Serial.printf("NEXTION TX: page %u\n", NextionIds::Page::UPDATE);
    setCurrentPage(UiPage::UPDATE);
    updateScreenPhase_ = UpdateScreenPhase::WAIT_PAGE_TX;
    return false;
  }

  if (updateScreenPhase_ == UpdateScreenPhase::WAIT_PAGE_TX) {
    if (!driver_.commandQueueIdle()) {
      return false;
    }
    driver_.flushNormalTx();
    updatePageSettledFromMs_ = millis();
    updateScreenPhase_ = UpdateScreenPhase::WAIT_PAGE_SETTLE;
    return false;
  }

  if (updateScreenPhase_ == UpdateScreenPhase::WAIT_PAGE_SETTLE) {
    if (static_cast<uint32_t>(millis() - updatePageSettledFromMs_) <
        Config::NextionUpdate::UPDATE_PAGE_SETTLE_MS) {
      return false;
    }
    updateScreenPhase_ = UpdateScreenPhase::IDLE;
  }

  constexpr size_t TEXT_COMMAND_COUNT = 3;
  if (!driver_.canQueueCommands(TEXT_COMMAND_COUNT)) {
    return false;
  }

  const auto logTextCommand = [](const char* component, const char* value) {
    Serial.printf("NEXTION TX: %s.txt=", component);
    Serial.write(static_cast<uint8_t>(0x22));
    Serial.print(value);
    Serial.write(static_cast<uint8_t>(0x22));
    Serial.println();
  };
  logTextCommand(NextionIds::UpdateObject::STATUS, status);
  logTextCommand(NextionIds::UpdateObject::PROGRESS, progress);
  logTextCommand(NextionIds::UpdateObject::WARNING, warning);
  const bool queued =
      driver_.setTextUtf8(NextionIds::UpdateObject::STATUS, status) &&
      driver_.setTextUtf8(NextionIds::UpdateObject::PROGRESS, progress) &&
      driver_.setTextUtf8(NextionIds::UpdateObject::WARNING, warning);
  if (queued) {
    updateScreenPhase_ = UpdateScreenPhase::WAIT_TEXT_TX;
  }
  return false;
}

void NextionUi::onTouch(void* context, const NextionTouchEvent& event) {
  static_cast<NextionUi*>(context)->handleTouch(event);
}

void NextionUi::onPage(void* context, uint8_t pageId) {
  static_cast<NextionUi*>(context)->handlePage(pageId);
}

void NextionUi::onReady(void* context) {
  static_cast<NextionUi*>(context)->handleReady();
}

void NextionUi::onError(void* context, uint8_t errorCode) {
  static_cast<NextionUi*>(context)->handleError(errorCode);
}

void NextionUi::handleTouch(const NextionTouchEvent& event) {
  state_.ui.lastTouchMs = millis();
  const bool isPress = event.action == NextionTouchAction::PRESS;
  const bool isRelease = event.action == NextionTouchAction::RELEASE;
  const char* action = isPress ? "PRESS" : (isRelease ? "RELEASE" : "UNKNOWN");
  Serial.printf("NEXTION TOUCH page=%u component=%u event=%s\n", event.pageId,
                event.componentId, action);

  if (event.pageId == NextionIds::Page::RADIO) {
    handleRadioTouch(event);
  } else if (event.pageId == NextionIds::Page::BOILER) {
    handleBoilerTouch(event);
  }

  if (isRelease) {
    dispatchNavigation(event);
  }
}

void NextionUi::handleBoilerTouch(const NextionTouchEvent& event) {
  const bool temperatureUp =
      event.componentId == NextionIds::BoilerComponent::TEMPERATURE_PLUS;
  const bool temperatureDown =
      event.componentId == NextionIds::BoilerComponent::TEMPERATURE_MINUS;
  if (temperatureUp || temperatureDown) {
    if (event.action == NextionTouchAction::PRESS) {
      startTemperatureHold(temperatureUp);
    } else if (event.action == NextionTouchAction::RELEASE) {
      finishTemperatureHold(temperatureUp);
    }
    return;
  }

  if (event.action != NextionTouchAction::RELEASE) {
    return;
  }

  processOptimisticTarget();

  if (event.componentId == NextionIds::BoilerComponent::HEAT) {
    const StateValue<HvacMode>& current =
        state_.homeAssistant.climate.hvacMode;
    if (!current.received || !current.valid ||
        current.value == HvacMode::UNKNOWN) {
      Serial.println("KOCIOL CMD ignored: HVAC invalid");
      return;
    }
    const HvacMode target =
        current.value == HvacMode::HEAT ? HvacMode::OFF : HvacMode::HEAT;
    if (!mqtt_.publishHvacMode(target)) {
      Serial.println("KOCIOL CMD ignored: MQTT offline");
      return;
    }
    Serial.printf("KOCIOL CMD HVAC %s\n",
                  target == HvacMode::HEAT ? "heat" : "off");
    return;
  }

  if (event.componentId == NextionIds::BoilerComponent::MODE) {
    const StateValue<PresetMode>& current =
        state_.homeAssistant.climate.presetMode;
    if (!current.received || !current.valid ||
        current.value == PresetMode::UNKNOWN) {
      Serial.println("KOCIOL CMD ignored: PRESET invalid");
      return;
    }
    const PresetMode target =
        current.value == PresetMode::COMFORT ? PresetMode::SLEEP
                                             : PresetMode::COMFORT;
    if (!mqtt_.publishPresetMode(target)) {
      Serial.println("KOCIOL CMD ignored: MQTT offline");
      return;
    }
    Serial.printf("KOCIOL CMD PRESET %s\n",
                  target == PresetMode::COMFORT ? "comfort" : "sleep");
    return;
  }

  if (event.componentId == NextionIds::BoilerComponent::POWER) {
    const StateValue<BinaryState>& current =
        state_.homeAssistant.boilerPower.state;
    if (!current.received || !current.valid ||
        current.value == BinaryState::UNKNOWN) {
      Serial.println("KOCIOL CMD ignored: POWER invalid");
      return;
    }
    const BinaryState target =
        current.value == BinaryState::ON ? BinaryState::OFF : BinaryState::ON;
    if (!mqtt_.publishBoilerPower(target)) {
      Serial.println("KOCIOL CMD ignored: MQTT offline");
      return;
    }
    Serial.printf("KOCIOL CMD POWER %s\n",
                  target == BinaryState::ON ? "ON" : "OFF");
    return;
  }

}

void NextionUi::startTemperatureHold(bool temperatureUp) {
  temperatureHoldActive_ = true;
  temperatureHoldUp_ = temperatureUp;
  temperatureHoldCommandStarted_ = false;
  temperatureHoldStartedMs_ = millis();
  nextTemperatureRepeatMs_ = 0;
}

void NextionUi::finishTemperatureHold(bool temperatureUp) {
  if (!temperatureHoldActive_ || temperatureHoldUp_ != temperatureUp) {
    sendTemperatureCommand(temperatureUp, true);
    temperatureHoldActive_ = false;
    return;
  }
  if (!temperatureHoldCommandStarted_) {
    sendTemperatureCommand(temperatureUp, true);
  }
  temperatureHoldActive_ = false;
}

void NextionUi::processTemperatureHold() {
  if (!temperatureHoldActive_) {
    return;
  }
  if (state_.ui.currentPage != UiPage::BOILER) {
    temperatureHoldActive_ = false;
    return;
  }

  const uint32_t now = millis();
  if (!temperatureHoldCommandStarted_) {
    if (static_cast<uint32_t>(now - temperatureHoldStartedMs_) <
        Config::Boiler::TEMP_HOLD_DELAY_MS) {
      return;
    }
    temperatureHoldCommandStarted_ = true;
    sendTemperatureCommand(temperatureHoldUp_, true);
    nextTemperatureRepeatMs_ = now + Config::Boiler::TEMP_REPEAT_MS;
    state_.ui.lastTouchMs = now;
    return;
  }

  if (static_cast<int32_t>(now - nextTemperatureRepeatMs_) < 0) {
    return;
  }
  sendTemperatureCommand(temperatureHoldUp_, false);
  nextTemperatureRepeatMs_ = now + Config::Boiler::TEMP_REPEAT_MS;
  state_.ui.lastTouchMs = now;
}

bool NextionUi::sendTemperatureCommand(bool temperatureUp, bool logCommand) {
  processOptimisticTarget();

  float baseTemperature = 0.0F;
  if (state_.ui.optimisticTargetActive) {
    baseTemperature = state_.ui.optimisticTargetValue;
  } else {
    const StateValue<float>& current =
        state_.homeAssistant.climate.targetTemperature;
    if (!current.received || !current.valid || !isfinite(current.value)) {
      if (logCommand) {
        Serial.println("KOCIOL CMD ignored: TEMP invalid");
      }
      return false;
    }
    baseTemperature = current.value;
  }

  int targetTenths = static_cast<int>(lroundf(baseTemperature * 10.0F));
  const int stepTenths =
      static_cast<int>(lroundf(Config::Boiler::TARGET_STEP_C * 10.0F));
  targetTenths += temperatureUp ? stepTenths : -stepTenths;
  const int minimumTenths =
      static_cast<int>(lroundf(Config::Boiler::TARGET_MIN_C * 10.0F));
  const int maximumTenths =
      static_cast<int>(lroundf(Config::Boiler::TARGET_MAX_C * 10.0F));
  if (targetTenths < minimumTenths) {
    targetTenths = minimumTenths;
  } else if (targetTenths > maximumTenths) {
    targetTenths = maximumTenths;
  }
  const float targetTemperature =
      static_cast<float>(targetTenths) / 10.0F;

  if (!mqtt_.publishTargetTemperature(targetTemperature)) {
    if (logCommand) {
      Serial.println("KOCIOL CMD ignored: MQTT offline");
    }
    return false;
  }

  state_.ui.optimisticTargetActive = true;
  state_.ui.optimisticTargetValue = targetTemperature;
  state_.ui.optimisticTargetStartedMs = millis();
  optimisticTargetSourceUpdateMs_ =
      state_.homeAssistant.climate.group.lastUpdateMs;
  if (logCommand) {
    Serial.printf("KOCIOL CMD TEMP %.1f\n", targetTemperature);
  }
  return true;
}

void NextionUi::processOptimisticTarget() {
  if (!state_.ui.optimisticTargetActive) {
    return;
  }

  const uint32_t now = millis();
  const bool stateConfirmed =
      state_.homeAssistant.climate.group.lastUpdateMs !=
      optimisticTargetSourceUpdateMs_;
  const bool timedOut =
      static_cast<uint32_t>(now - state_.ui.optimisticTargetStartedMs) >=
      Config::Boiler::MQTT_CONFIRM_TIMEOUT_MS;
  if (!stateConfirmed && !timedOut) {
    return;
  }

  state_.ui.optimisticTargetActive = false;
  boilerCache_.targetTemperatureValid = false;
}

void NextionUi::handlePage(uint8_t pageId) {
  const UiPage synchronizedPage = pageFromId(pageId);

  Serial.printf("NEXTION PAGE page=%u\n", pageId);

  if (synchronizedPage == UiPage::UNKNOWN) {
    state_.ui.currentPage = UiPage::UNKNOWN;
    state_.ui.nextionSynchronized = false;
    state_.ui.fullRefreshRequested = true;
    state_.ui.activePageCacheValid = false;
    return;
  }

  // Dopiero poprawna ramka 0x66 kończy aktywną synchronizację startową.
  startupSyncActive_ = false;

  // Poprawna odpowiedź 0x66 potwierdza gotowość komunikacji także wtedy,
  // gdy startowa ramka 0x88 została wysłana przed uruchomieniem ESP.
  state_.ui.nextionReady = true;
  state_.ui.nextionSynchronized = true;

  if (synchronizedPage == UiPage::BOOT) {
    // Po komendzie "page 3" potwierdzenie przyjmujemy dopiero po jawnym
    // zapytaniu sendme. Ewentualna spontaniczna ramka strony nie kończy fazy.
    if (bootPageConfirmationPending_ && !bootPageVerificationSent_) {
      return;
    }

    bootPageRequestPending_ = false;
    bootPageConfirmationPending_ = false;
    bootPageVerificationSent_ = false;
    if (!bootSplashCompleted_) {
      Serial.println("BOOT PAGE CONFIRMED");
    }
    if (bootSplashCompleted_) {
      setCurrentPage(UiPage::BOOT);
      bootStartPagePending_ = true;
      finishBootSplash();
    } else {
      setCurrentPage(UiPage::BOOT);
      startBootSplash();
    }
    return;
  }

  if (!bootSplashStarted_ && !bootSplashCompleted_) {
    // Po restarcie ESP fizyczna strona Nextiona musi zostać potwierdzona
    // odpowiedzią 0x66. Samo zakolejkowanie "page 3" nie jest synchronizacją.
    state_.ui.currentPage = synchronizedPage;
    state_.ui.fullRefreshRequested = true;
    state_.ui.activePageCacheValid = false;
    bootPageRequestPending_ = true;
    requestPhysicalBootPage();
    return;
  }

  setCurrentPage(synchronizedPage);
}

void NextionUi::handleReady() {
  Serial.println("NEXTION READY");

  if (updateScreenActive_) {
    // Po udanej aktualizacji czekamy już wyłącznie na kontrolowany restart ESP.
    // Ramka 0x88 z restartującego się HMI nie może uruchomić zwykłego sync.
    driver_.clearNormalTxQueue();
    updateScreenPhase_ = UpdateScreenPhase::IDLE;
    state_.ui.currentPage = UiPage::UNKNOWN;
    state_.ui.nextionSynchronized = false;
    state_.ui.fullRefreshRequested = true;
    state_.ui.activePageCacheValid = false;
    startupSyncActive_ = false;
    startupSyncAttempted_ = false;
    Serial.println("NEXTION READY: startup sync wstrzymany do restartu ESP");
    Serial.println("NEXTION UPDATE: ponowne przygotowanie page=4");
    return;
  }

  state_.ui.nextionReady = true;
  state_.ui.nextionSynchronized = false;
  state_.ui.currentPage = UiPage::UNKNOWN;
  state_.ui.fullRefreshRequested = true;
  state_.ui.activePageCacheValid = false;

  // Po restarcie samego Nextiona ponownie ustalamy fizyczną stronę przez 0x66.
  // Zakończony wcześniej BOOT zachowa dzięki temu szybką ścieżkę recovery.
  startupSyncActive_ = true;
  startupSyncAttempted_ = false;
  nextStartupSyncMs_ = millis() + Config::Nextion::STARTUP_SYNC_SETTLE_MS;
}

void NextionUi::handleError(uint8_t errorCode) {
  Serial.printf("NEXTION ERROR code=0x%02X\n", errorCode);
  if (startupSyncActive_) {
    Serial.println(
        "NEXTION STARTUP SYNC: odpowiedź błędna, próba zostanie ponowiona");
  }
}

void NextionUi::processStartupSync() {
  if (!startupSyncActive_) {
    return;
  }

  const uint32_t now = millis();
  if (static_cast<int32_t>(now - nextStartupSyncMs_) < 0) {
    return;
  }

  if (startupSyncAttempted_) {
    ++state_.diagnostics.nextion.startupSyncRetries;
  }

  if (!driver_.requestCurrentPage()) {
    nextStartupSyncMs_ = now + Config::Nextion::STARTUP_SYNC_RETRY_MS;
    Serial.println(
        "NEXTION WARNING: nie udało się zakolejkować startowego sendme");
    return;
  }

  Serial.println(startupSyncAttempted_ ? "NEXTION STARTUP SYNC RETRY"
                                       : "NEXTION STARTUP SYNC");
  Serial.println("NEXTION STARTUP TX: sendme");
  startupSyncAttempted_ = true;
  nextStartupSyncMs_ = now + Config::Nextion::STARTUP_SYNC_RETRY_MS;
}

bool NextionUi::requestPhysicalBootPage() {
  if (!driver_.changePage(NextionIds::Page::BOOT)) {
    return false;
  }

  bootPageRequestPending_ = false;
  bootPageConfirmationPending_ = true;
  bootPageVerificationSent_ = false;
  bootPageRequestedMs_ = millis();
  Serial.printf("BOOT PAGE REQUEST page=%u\n", NextionIds::Page::BOOT);
  return true;
}

void NextionUi::startBootSplash() {
  if (bootSplashStarted_ || bootSplashCompleted_) {
    return;
  }

  bootSplashStarted_ = true;
  nextBootDotMs_ = millis();
  nextBootDotCount_ = 1;
  renderedBootDotCount_ = 0xFF;
  bootStatus_ = BootStatus::UNKNOWN;
  bootRecoveredLogged_ = false;
}

void NextionUi::processBootSplash() {
  if (bootPageRequestPending_) {
    requestPhysicalBootPage();
    return;
  }

  if (bootPageConfirmationPending_) {
    const uint32_t now = millis();
    const uint32_t phaseElapsedMs =
        static_cast<uint32_t>(now - bootPageRequestedMs_);

    if (!bootPageVerificationSent_) {
      if (phaseElapsedMs < Config::Nextion::BOOT_PAGE_SETTLE_MS) {
        return;
      }
      if (!driver_.requestCurrentPage()) {
        return;
      }

      bootPageVerificationSent_ = true;
      bootPageRequestedMs_ = now;
      Serial.println("BOOT PAGE VERIFY sendme");
      return;
    }

    if (phaseElapsedMs >= Config::Nextion::LIVENESS_CHECK_INTERVAL_MS) {
      ++state_.diagnostics.nextion.pageVerifyRetries;
      Serial.println("BOOT PAGE RETRY: brak potwierdzenia page=3");
      bootPageConfirmationPending_ = false;
      bootPageVerificationSent_ = false;
      bootPageRequestPending_ = true;
    }
    return;
  }

  if (!state_.ui.nextionSynchronized ||
      state_.ui.currentPage != UiPage::BOOT) {
    return;
  }

  if (bootStartPagePending_) {
    finishBootSplash();
    return;
  }

  if (!bootSplashStarted_ || bootSplashCompleted_) {
    return;
  }

  const uint32_t now = millis();
  const uint32_t elapsedMs =
      static_cast<uint32_t>(now - bootStartedMs_);

  if (state_.clock.timeValid) {
    if (!bootClockReadyLogged_) {
      Serial.printf("BOOT CLOCK READY elapsed=%lu\n",
                    static_cast<unsigned long>(elapsedMs));
      bootClockReadyLogged_ = true;
    }
    if (!bootRecoveredLogged_ &&
        (bootStatus_ == BootStatus::ERROR_WIFI ||
         bootStatus_ == BootStatus::ERROR_NTP)) {
      Serial.println("BOOT RECOVERED");
      bootRecoveredLogged_ = true;
    }
    bootStartPagePending_ = true;
    finishBootSplash();
    return;
  }

  if (elapsedMs >= Config::Ui::BOOT_MAX_WAIT_MS) {
    const BootStatus desiredStatus = state_.network.connected
                                         ? BootStatus::ERROR_NTP
                                         : BootStatus::ERROR_WIFI;
    const char* statusText = desiredStatus == BootStatus::ERROR_WIFI
                                 ? "Brak Wi-Fi"
                                 : "Brak synchronizacji czasu";
    const BootStatus previousStatus = bootStatus_;
    if (setBootStatus(desiredStatus, statusText) &&
        previousStatus != desiredStatus) {
      Serial.println(desiredStatus == BootStatus::ERROR_WIFI
                         ? "BOOT ERROR WIFI"
                         : "BOOT ERROR NTP");
    }
    setBootDots(0);
    return;
  }

  setBootStatus(BootStatus::STARTING, "Uruchamianie");
  if (static_cast<int32_t>(now - nextBootDotMs_) < 0) {
    return;
  }

  if (setBootDots(nextBootDotCount_)) {
    nextBootDotCount_ =
        static_cast<uint8_t>(nextBootDotCount_ % 3U + 1U);
    nextBootDotMs_ = now + Config::Ui::BOOT_SPLASH_DOT_INTERVAL_MS;
  }
}

bool NextionUi::finishBootSplash() {
  if (!driver_.changePage(NextionIds::Page::START)) {
    return false;
  }

  bootStartPagePending_ = false;
  bootSplashStarted_ = false;
  bootSplashCompleted_ = true;
  Serial.println("BOOT -> START");
  setCurrentPage(UiPage::START);
  return true;
}

bool NextionUi::setBootStatus(BootStatus status, const char* text) {
  if (bootStatus_ == status) {
    return true;
  }
  if (!driver_.setTextUtf8(NextionIds::BootObject::STATUS, text)) {
    return false;
  }
  bootStatus_ = status;
  return true;
}

bool NextionUi::setBootDots(uint8_t count) {
  if (renderedBootDotCount_ == count) {
    return true;
  }

  static constexpr const char* DOT_TEXTS[4] = {"", ".", "..", "..."};
  if (count > 3U ||
      !driver_.setText(NextionIds::BootObject::DOTS, DOT_TEXTS[count])) {
    return false;
  }
  renderedBootDotCount_ = count;
  return true;
}

void NextionUi::dispatchNavigation(const NextionTouchEvent& event) {
  UiPage destination = UiPage::UNKNOWN;

  switch (event.pageId) {
    case NextionIds::Page::START:
      if (event.componentId == NextionIds::StartComponent::NAVIGATE_BOILER) {
        destination = UiPage::BOILER;
      } else if (event.componentId ==
                 NextionIds::StartComponent::NAVIGATE_RADIO) {
        destination = UiPage::RADIO;
      }
      break;

    case NextionIds::Page::RADIO:
      if (event.componentId == NextionIds::RadioComponent::NAVIGATE_BOILER) {
        destination = UiPage::BOILER;
      } else if (event.componentId ==
                 NextionIds::RadioComponent::NAVIGATE_START) {
        destination = UiPage::START;
      }
      break;

    case NextionIds::Page::BOILER:
      if (event.componentId == NextionIds::BoilerComponent::NAVIGATE_START) {
        destination = UiPage::START;
      } else if (event.componentId ==
                 NextionIds::BoilerComponent::NAVIGATE_RADIO) {
        destination = UiPage::RADIO;
      }
      break;

    default:
      break;
  }

  if (destination != UiPage::UNKNOWN) {
    // Nawigację wykonuje lokalny event HMI. Firmware tylko śledzi jej wynik.
    const UiPage source = pageFromId(event.pageId);
    Serial.printf("UI NAV source=%s component=%u target=%s\n", pageName(source),
                  event.componentId, pageName(destination));
    state_.ui.nextionReady = true;
    state_.ui.nextionSynchronized = true;
    setCurrentPage(destination);
  }
}

void NextionUi::setCurrentPage(UiPage page) {
  if (page != UiPage::RADIO) {
    volumeHoldActive_ = false;
  } else {
    invalidateRadioCache();
  }
  if (page != UiPage::BOILER) {
    temperatureHoldActive_ = false;
  }
  if (page == UiPage::START) {
    invalidateStartCache();
  } else if (page == UiPage::BOILER) {
    invalidateBoilerCache();
  }

  state_.ui.currentPage = page;
  state_.ui.fullRefreshRequested = true;
  state_.ui.activePageCacheValid = false;
  state_.ui.lastTouchMs = millis();

  if (page != UiPage::UNKNOWN) {
    Serial.printf("UI PAGE -> %s\n", pageName(page));
  }
}

void NextionUi::handleRadioTouch(const NextionTouchEvent& event) {
  const bool isPress = event.action == NextionTouchAction::PRESS;
  const bool isRelease = event.action == NextionTouchAction::RELEASE;

  if (event.componentId == NextionIds::RadioComponent::VOLUME_UP) {
    if (isPress) {
      startVolumeHold(true);
    } else if (isRelease) {
      finishVolumeHold(true);
    }
    return;
  }

  if (event.componentId == NextionIds::RadioComponent::VOLUME_DOWN) {
    if (isPress) {
      startVolumeHold(false);
    } else if (isRelease) {
      finishVolumeHold(false);
    }
    return;
  }

  if (!isRelease) {
    return;
  }

  bool sent = false;
  if (event.componentId == NextionIds::RadioComponent::PREVIOUS) {
    Serial.println("RADIO CMD PREV");
    sent = yoRadio_.previous();
  } else if (event.componentId ==
             NextionIds::RadioComponent::PLAY_PAUSE) {
    Serial.println("RADIO CMD PLAY");
    sent = yoRadio_.togglePlayPause();
  } else if (event.componentId == NextionIds::RadioComponent::NEXT) {
    Serial.println("RADIO CMD NEXT");
    sent = yoRadio_.next();
  } else {
    return;
  }

  if (!sent) {
    Serial.println("RADIO WARNING: yoRadio nie jest ONLINE");
  }
}

void NextionUi::startVolumeHold(bool volumeUp) {
  volumeHoldActive_ = true;
  volumeHoldUp_ = volumeUp;
  volumeHoldCommandStarted_ = false;
  volumeHoldStartedMs_ = millis();
  nextVolumeRepeatMs_ = 0;
}

void NextionUi::finishVolumeHold(bool volumeUp) {
  if (!volumeHoldActive_ || volumeHoldUp_ != volumeUp) {
    sendVolumeCommand(volumeUp, true);
    volumeHoldActive_ = false;
    return;
  }

  if (!volumeHoldCommandStarted_) {
    sendVolumeCommand(volumeUp, true);
  }
  volumeHoldActive_ = false;
}

void NextionUi::processVolumeHold() {
  if (!volumeHoldActive_) {
    return;
  }
  if (state_.ui.currentPage != UiPage::RADIO) {
    volumeHoldActive_ = false;
    return;
  }

  const uint32_t now = millis();
  if (!volumeHoldCommandStarted_) {
    if (static_cast<uint32_t>(now - volumeHoldStartedMs_) <
        Config::Ui::HOLD_DELAY_MS) {
      return;
    }
    volumeHoldCommandStarted_ = true;
    sendVolumeCommand(volumeHoldUp_, true);
    nextVolumeRepeatMs_ = now + Config::Ui::VOLUME_REPEAT_MS;
    state_.ui.lastTouchMs = now;
    return;
  }

  if (static_cast<int32_t>(now - nextVolumeRepeatMs_) < 0) {
    return;
  }

  sendVolumeCommand(volumeHoldUp_, false);
  nextVolumeRepeatMs_ = now + Config::Ui::VOLUME_REPEAT_MS;
  state_.ui.lastTouchMs = now;
}

bool NextionUi::sendVolumeCommand(bool volumeUp, bool logCommand) {
  if (logCommand) {
    Serial.println(volumeUp ? "RADIO CMD VOL+" : "RADIO CMD VOL-");
  }

  const bool sent =
      volumeUp ? yoRadio_.volumeUp() : yoRadio_.volumeDown();
  if (!sent && logCommand) {
    Serial.println("RADIO WARNING: yoRadio nie jest ONLINE");
  }
  return sent;
}

void NextionUi::syncCalendarMarquee(CalendarMarqueeState& marquee,
                                    const char* fullText, uint32_t now) {
  if (strcmp(marquee.fullText, fullText) == 0) {
    return;
  }

  snprintf(marquee.fullText, sizeof(marquee.fullText), "%s", fullText);
  marquee.offsetCharacters = 0;
  marquee.scrollNeeded =
      calendarTextWidth(marquee.fullText) >
          Config::HomeAssistant::CALENDAR_VISIBLE_WIDTH_PX ||
      utf8CharacterCount(marquee.fullText, strlen(marquee.fullText)) >
          Config::HomeAssistant::CALENDAR_COMPONENT_MAX_CHARS;
  buildCalendarFragment(marquee.fullText, 0, marquee.visibleText,
                        sizeof(marquee.visibleText));
  if (marquee.scrollNeeded) {
    marquee.phase = CalendarMarqueePhase::START_PAUSE;
    marquee.nextStepMs =
        now + Config::HomeAssistant::CALENDAR_MARQUEE_START_PAUSE_MS;
  } else {
    marquee.phase = CalendarMarqueePhase::IDLE;
    marquee.nextStepMs = 0;
  }
}

bool NextionUi::advanceCalendarMarquee(CalendarMarqueeState& marquee,
                                       uint32_t now) {
  if (!marquee.scrollNeeded ||
      static_cast<int32_t>(now - marquee.nextStepMs) < 0) {
    return false;
  }

  char previous[Config::HomeAssistant::CALENDAR_ENTRY_MAX_BYTES + 1]{};
  snprintf(previous, sizeof(previous), "%s", marquee.visibleText);

  if (marquee.phase == CalendarMarqueePhase::END_PAUSE) {
    marquee.offsetCharacters = 0;
    buildCalendarFragment(marquee.fullText, 0, marquee.visibleText,
                          sizeof(marquee.visibleText));
    marquee.phase = CalendarMarqueePhase::START_PAUSE;
    marquee.nextStepMs =
        now + Config::HomeAssistant::CALENDAR_MARQUEE_START_PAUSE_MS;
    return strcmp(previous, marquee.visibleText) != 0;
  }

  ++marquee.offsetCharacters;
  const bool reachedEnd =
      buildCalendarFragment(marquee.fullText, marquee.offsetCharacters,
                            marquee.visibleText, sizeof(marquee.visibleText));
  if (reachedEnd) {
    marquee.phase = CalendarMarqueePhase::END_PAUSE;
    marquee.nextStepMs =
        now + Config::HomeAssistant::CALENDAR_MARQUEE_END_PAUSE_MS;
  } else {
    marquee.phase = CalendarMarqueePhase::SCROLLING;
    marquee.nextStepMs =
        now + Config::HomeAssistant::CALENDAR_MARQUEE_STEP_MS;
  }
  return strcmp(previous, marquee.visibleText) != 0;
}

void NextionUi::resetCalendarMarquees() {
  for (size_t index = 0; index < 4; ++index) {
    calendarTodayMarquee_[index] = CalendarMarqueeState{};
    calendarTomorrowMarquee_[index] = CalendarMarqueeState{};
  }
  nextCalendarMarqueeIndex_ = 0;
}

void NextionUi::renderStart() {
  const bool fullRefresh = state_.ui.fullRefreshRequested ||
                           !state_.ui.activePageCacheValid;

  char clockText[6]{};
  char dateText[11]{};
  char weatherTemperature[24]{};
  char calendarToday[4]
                    [Config::HomeAssistant::CALENDAR_ENTRY_MAX_BYTES + 1]{};
  char calendarTomorrow[4]
                       [Config::HomeAssistant::CALENDAR_ENTRY_MAX_BYTES + 1]{};
  uint16_t calendarTodayColor[4]{};
  uint16_t calendarTomorrowColor[4]{};
  WasteUiValue waste[4]{};
  if (state_.clock.timeValid) {
    clock_.formatTime(clockText, sizeof(clockText));
    clock_.formatDate(dateText, sizeof(dateText));
  }
  formatWeatherTemperature(state_.homeAssistant.weather.temperature,
                           weatherTemperature, sizeof(weatherTemperature));

  const bool calendarValid = state_.homeAssistant.calendar.group.received &&
                             state_.homeAssistant.calendar.group.valid;
  for (size_t index = 0; index < 4; ++index) {
    const CalendarEntryType todayType = formatCalendarEntry(
        state_.homeAssistant.calendar.today[index], calendarValid,
        calendarToday[index], sizeof(calendarToday[index]));
    const CalendarEntryType tomorrowType = formatCalendarEntry(
        state_.homeAssistant.calendar.tomorrow[index], calendarValid,
        calendarTomorrow[index], sizeof(calendarTomorrow[index]));
    calendarTodayColor[index] = calendarEntryColor(todayType);
    calendarTomorrowColor[index] = calendarEntryColor(tomorrowType);
  }

  const uint32_t now = millis();
  for (size_t index = 0; index < 4; ++index) {
    syncCalendarMarquee(calendarTodayMarquee_[index], calendarToday[index],
                        now);
    syncCalendarMarquee(calendarTomorrowMarquee_[index],
                        calendarTomorrow[index], now);
  }

  uint8_t marqueeUpdates = 0;
  uint8_t marqueeStatesChecked = 0;
  while (marqueeStatesChecked < 8 &&
         marqueeUpdates <
             Config::HomeAssistant::CALENDAR_MARQUEE_MAX_UPDATES_PER_LOOP) {
    const uint8_t stateIndex = nextCalendarMarqueeIndex_;
    nextCalendarMarqueeIndex_ =
        static_cast<uint8_t>((nextCalendarMarqueeIndex_ + 1U) % 8U);
    ++marqueeStatesChecked;
    CalendarMarqueeState& marquee =
        stateIndex < 4 ? calendarTodayMarquee_[stateIndex]
                       : calendarTomorrowMarquee_[stateIndex - 4U];
    if (advanceCalendarMarquee(marquee, now)) {
      ++marqueeUpdates;
    }
  }

  const bool wasteValid = state_.homeAssistant.waste.group.received &&
                          state_.homeAssistant.waste.group.valid;
  waste[0] = formatWasteEntry(
      state_.homeAssistant.waste.mixed, wasteValid,
      NextionIds::Picture::WASTE_MIXED_GRAY,
      NextionIds::Picture::WASTE_MIXED_COLOR);
  waste[1] = formatWasteEntry(
      state_.homeAssistant.waste.plastic, wasteValid,
      NextionIds::Picture::WASTE_PLASTIC_GRAY,
      NextionIds::Picture::WASTE_PLASTIC_COLOR);
  waste[2] = formatWasteEntry(
      state_.homeAssistant.waste.paper, wasteValid,
      NextionIds::Picture::WASTE_PAPER_GRAY,
      NextionIds::Picture::WASTE_PAPER_COLOR);
  waste[3] = formatWasteEntry(
      state_.homeAssistant.waste.glass, wasteValid,
      NextionIds::Picture::WASTE_GLASS_GRAY,
      NextionIds::Picture::WASTE_GLASS_COLOR);

  const bool clockReady =
      renderStartText(NextionIds::StartObject::CLOCK, clockText,
                      startCache_.clock, sizeof(startCache_.clock),
                      startCache_.clockValid, "clock", false, true);
  const bool dateReady =
      renderStartText(NextionIds::StartObject::DATE, dateText,
                      startCache_.date, sizeof(startCache_.date),
                      startCache_.dateValid, "date", false, true);
  const bool weatherReady = renderStartText(
      NextionIds::StartObject::WEATHER_TEMPERATURE, weatherTemperature,
      startCache_.weatherTemperature, sizeof(startCache_.weatherTemperature),
      startCache_.weatherTemperatureValid, "weather temp", true, true);

  bool calendarReady = true;
  for (size_t index = 0; index < 4; ++index) {
    const bool todayColorReady = renderStartColor(
        NextionIds::StartObject::CALENDAR_TODAY[index],
        calendarTodayColor[index], startCache_.calendarTodayColor[index],
        startCache_.calendarTodayColorValid[index],
        CALENDAR_TODAY_LOG_NAMES[index]);
    const bool todayReady = renderStartText(
        NextionIds::StartObject::CALENDAR_TODAY[index],
        calendarTodayMarquee_[index].visibleText,
        startCache_.calendarToday[index],
        sizeof(startCache_.calendarToday[index]),
        startCache_.calendarTodayValid[index], CALENDAR_TODAY_LOG_NAMES[index],
        true, false);
    const bool tomorrowColorReady = renderStartColor(
        NextionIds::StartObject::CALENDAR_TOMORROW[index],
        calendarTomorrowColor[index], startCache_.calendarTomorrowColor[index],
        startCache_.calendarTomorrowColorValid[index],
        CALENDAR_TOMORROW_LOG_NAMES[index]);
    const bool tomorrowReady = renderStartText(
        NextionIds::StartObject::CALENDAR_TOMORROW[index],
        calendarTomorrowMarquee_[index].visibleText,
        startCache_.calendarTomorrow[index],
        sizeof(startCache_.calendarTomorrow[index]),
        startCache_.calendarTomorrowValid[index],
        CALENDAR_TOMORROW_LOG_NAMES[index], true, false);
    calendarReady = todayColorReady && todayReady && tomorrowColorReady &&
                    tomorrowReady && calendarReady;
  }

  bool wasteReady = true;
  for (size_t index = 0; index < 4; ++index) {
    const bool entryReady = renderStartWasteEntry(
        NextionIds::WasteObject::TERM[index],
        NextionIds::WasteObject::PICTURE[index], waste[index].text,
        waste[index].color, waste[index].picture, startCache_.waste[index],
        WASTE_LOG_NAMES[index]);
    wasteReady = entryReady && wasteReady;
  }

  if (fullRefresh && clockReady && dateReady && weatherReady &&
      calendarReady && wasteReady) {
    state_.ui.fullRefreshRequested = false;
    state_.ui.activePageCacheValid = true;
  }
}

bool NextionUi::renderStartText(const char* component, const char* value,
                                char* cachedValue, size_t cachedSize,
                                bool& cacheValid, const char* logName,
                                bool utf8, bool logUpdate) {
  if (cacheValid && strcmp(cachedValue, value) == 0) {
    return true;
  }
  const bool queued = utf8 ? driver_.setTextUtf8(component, value)
                           : driver_.setText(component, value);
  if (!queued) {
    return false;
  }

  snprintf(cachedValue, cachedSize, "%s", value);
  cacheValid = true;
  if (logUpdate) {
    Serial.printf("START UI %s updated\n", logName);
  }
  return true;
}

bool NextionUi::renderStartColor(const char* component, uint16_t color,
                                 uint16_t& cachedColor, bool& cacheValid,
                                 const char* logName) {
  if (cacheValid && cachedColor == color) {
    return true;
  }
  if (!driver_.setColor(component, color)) {
    return false;
  }

  cachedColor = color;
  cacheValid = true;
  Serial.printf("START UI %s color updated\n", logName);
  return true;
}

bool NextionUi::renderStartWasteEntry(
    const char* textComponent, const char* pictureComponent, const char* text,
    uint16_t color, uint16_t picture, StartCache::WasteEntry& cache,
    const char* logName) {
  bool ready = true;
  bool updated = false;

  if (!cache.colorValid || cache.color != color) {
    if (driver_.setColor(textComponent, color)) {
      cache.color = color;
      cache.colorValid = true;
      updated = true;
    } else {
      ready = false;
    }
  }
  if (!cache.pictureValid || cache.picture != picture) {
    if (driver_.setPicture(pictureComponent, picture)) {
      cache.picture = picture;
      cache.pictureValid = true;
      updated = true;
    } else {
      ready = false;
    }
  }
  if (!cache.textValid || strcmp(cache.text, text) != 0) {
    if (driver_.setTextUtf8(textComponent, text)) {
      snprintf(cache.text, sizeof(cache.text), "%s", text);
      cache.textValid = true;
      updated = true;
    } else {
      ready = false;
    }
  }

  if (updated) {
    Serial.printf("START UI waste %s updated\n", logName);
  }
  return ready;
}

void NextionUi::invalidateStartCache() {
  resetCalendarMarquees();
  startCache_.clockValid = false;
  startCache_.dateValid = false;
  startCache_.weatherTemperatureValid = false;
  for (size_t index = 0; index < 4; ++index) {
    startCache_.calendarTodayValid[index] = false;
    startCache_.calendarTomorrowValid[index] = false;
    startCache_.calendarTodayColorValid[index] = false;
    startCache_.calendarTomorrowColorValid[index] = false;
    startCache_.waste[index].textValid = false;
    startCache_.waste[index].colorValid = false;
    startCache_.waste[index].pictureValid = false;
  }
}

void NextionUi::renderBoiler() {
  const bool fullRefresh = state_.ui.fullRefreshRequested ||
                           !state_.ui.activePageCacheValid;

  char currentTemperature[16]{};
  char targetTemperature[16]{};
  char weatherTemperature[24]{};
  formatTemperature(state_.homeAssistant.climate.currentTemperature,
                    currentTemperature, sizeof(currentTemperature));
  if (state_.ui.optimisticTargetActive) {
    snprintf(targetTemperature, sizeof(targetTemperature), "%.1f",
             state_.ui.optimisticTargetValue);
  } else {
    formatTemperature(state_.homeAssistant.climate.targetTemperature,
                      targetTemperature, sizeof(targetTemperature));
  }
  formatWeatherTemperature(state_.homeAssistant.weather.temperature,
                           weatherTemperature, sizeof(weatherTemperature));

  const StateValue<HvacMode>& hvacMode =
      state_.homeAssistant.climate.hvacMode;
  const uint16_t heatPicture =
      hvacMode.received && hvacMode.valid && hvacMode.value == HvacMode::HEAT
          ? NextionIds::Picture::HEAT_COLOR
          : NextionIds::Picture::HEAT_GRAY;

  const StateValue<PresetMode>& presetMode =
      state_.homeAssistant.climate.presetMode;
  uint16_t modePicture = NextionIds::Picture::MANUAL;
  if (presetMode.received && presetMode.valid) {
    if (presetMode.value == PresetMode::COMFORT) {
      modePicture = NextionIds::Picture::SUN_COLOR;
    } else if (presetMode.value == PresetMode::SLEEP) {
      modePicture = NextionIds::Picture::MOON_COLOR;
    }
  }

  const StateValue<HvacAction>& hvacAction =
      state_.homeAssistant.climate.hvacAction;
  const uint16_t firePicture =
      hvacAction.received && hvacAction.valid &&
              hvacAction.value == HvacAction::HEATING
          ? NextionIds::Picture::FIRE_COLOR
          : NextionIds::Picture::FIRE_GRAY;

  const StateValue<BinaryState>& boilerPower =
      state_.homeAssistant.boilerPower.state;
  const uint16_t powerPicture =
      boilerPower.received && boilerPower.valid &&
              boilerPower.value == BinaryState::ON
          ? NextionIds::Picture::POWER_COLOR
          : NextionIds::Picture::POWER_GRAY;

  const StateValue<BinaryState>& windows =
      state_.homeAssistant.windows.state;
  const uint16_t windowsPicture =
      windows.received && windows.valid && windows.value == BinaryState::OFF
          ? NextionIds::Picture::WINDOW_GREEN
          : NextionIds::Picture::WINDOW_RED;

  bool snapshotReady = true;
  snapshotReady &= renderBoilerText(
      NextionIds::BoilerObject::CURRENT_TEMPERATURE, currentTemperature,
      boilerCache_.currentTemperature,
      sizeof(boilerCache_.currentTemperature),
      boilerCache_.currentTemperatureValid, "current temp", false);
  snapshotReady &= renderBoilerText(
      NextionIds::BoilerObject::TARGET_TEMPERATURE, targetTemperature,
      boilerCache_.targetTemperature, sizeof(boilerCache_.targetTemperature),
      boilerCache_.targetTemperatureValid, "target temp", false);
  snapshotReady &= renderBoilerText(
      NextionIds::BoilerObject::WEATHER_TEMPERATURE, weatherTemperature,
      boilerCache_.weatherTemperature,
      sizeof(boilerCache_.weatherTemperature),
      boilerCache_.weatherTemperatureValid, "weather temp", true);
  snapshotReady &= renderBoilerPicture(
      NextionIds::BoilerObject::HEAT, heatPicture, boilerCache_.heatPicture,
      boilerCache_.heatPictureValid, "heat");
  snapshotReady &= renderBoilerPicture(
      NextionIds::BoilerObject::MODE, modePicture, boilerCache_.modePicture,
      boilerCache_.modePictureValid, "mode");
  snapshotReady &= renderBoilerPicture(
      NextionIds::BoilerObject::FIRE, firePicture, boilerCache_.firePicture,
      boilerCache_.firePictureValid, "fire");
  snapshotReady &= renderBoilerPicture(
      NextionIds::BoilerObject::POWER, powerPicture, boilerCache_.powerPicture,
      boilerCache_.powerPictureValid, "power");
  snapshotReady &= renderBoilerPicture(
      NextionIds::BoilerObject::WINDOWS, windowsPicture,
      boilerCache_.windowsPicture, boilerCache_.windowsPictureValid,
      "windows");

  if (fullRefresh && snapshotReady) {
    state_.ui.fullRefreshRequested = false;
    state_.ui.activePageCacheValid = true;
  }
}

bool NextionUi::renderBoilerText(const char* component, const char* value,
                                 char* cachedValue, size_t cachedSize,
                                 bool& cacheValid, const char* logName,
                                 bool utf8) {
  if (cacheValid && strcmp(cachedValue, value) == 0) {
    return true;
  }
  const bool queued = utf8 ? driver_.setTextUtf8(component, value)
                           : driver_.setText(component, value);
  if (!queued) {
    return false;
  }

  snprintf(cachedValue, cachedSize, "%s", value);
  cacheValid = true;
  Serial.printf("KOCIOL UI %s updated\n", logName);
  return true;
}

bool NextionUi::renderBoilerPicture(const char* component, uint16_t pictureId,
                                    uint16_t& cachedPicture, bool& cacheValid,
                                    const char* logName) {
  if (cacheValid && cachedPicture == pictureId) {
    return true;
  }
  if (!driver_.setPicture(component, pictureId)) {
    return false;
  }

  cachedPicture = pictureId;
  cacheValid = true;
  Serial.printf("KOCIOL UI %s updated\n", logName);
  return true;
}

void NextionUi::invalidateBoilerCache() {
  boilerCache_.currentTemperatureValid = false;
  boilerCache_.targetTemperatureValid = false;
  boilerCache_.weatherTemperatureValid = false;
  boilerCache_.heatPictureValid = false;
  boilerCache_.modePictureValid = false;
  boilerCache_.firePictureValid = false;
  boilerCache_.powerPictureValid = false;
  boilerCache_.windowsPictureValid = false;
}

void NextionUi::renderRadio() {
  const bool fullRefresh = state_.ui.fullRefreshRequested ||
                           !state_.ui.activePageCacheValid;
  if (fullRefresh) {
    invalidateRadioCache();
  }

  const String station = utf8Prefix(
      state_.radio.station.received && state_.radio.station.valid
          ? state_.radio.station.value
          : String(),
      Config::YoRadio::UI_MEDIA_TEXT_MAX_CHARS);
  const String artist = utf8Prefix(
      state_.radio.artist.received && state_.radio.artist.valid
          ? state_.radio.artist.value
          : String(),
      Config::YoRadio::UI_MEDIA_TEXT_MAX_CHARS);
  const String title = utf8Prefix(
      state_.radio.title.received && state_.radio.title.valid
          ? state_.radio.title.value
          : String(),
      Config::YoRadio::UI_MEDIA_TEXT_MAX_CHARS);
  const bool radioStopped =
      state_.radio.playback.received && state_.radio.playback.valid &&
      state_.radio.playback.value == RadioPlaybackState::STOPPED;

  String uiFormat;
  if (state_.radio.format.received && state_.radio.format.valid) {
    uiFormat = state_.radio.format.value;
    uiFormat.trim();
    uiFormat = utf8Prefix(uiFormat, Config::YoRadio::UI_FORMAT_MAX_CHARS);
  }
  const String bitrate =
      state_.radio.bitrate.received && state_.radio.bitrate.valid &&
              state_.radio.bitrate.value >= Config::YoRadio::BITRATE_MIN &&
              state_.radio.bitrate.value <= Config::YoRadio::BITRATE_MAX &&
              !uiFormat.isEmpty() && !radioStopped
          ? String(state_.radio.bitrate.value) + ":" + uiFormat
          : String();
  const String volume =
      state_.radio.volume.received && state_.radio.volume.valid &&
              state_.radio.volume.value >= Config::YoRadio::VOLUME_MIN &&
              state_.radio.volume.value <= Config::YoRadio::VOLUME_MAX
          ? String("VOL ") + String(state_.radio.volume.value)
          : String();

  renderRadioText(NextionIds::RadioObject::STATION, station,
                  radioCache_.station, radioCache_.stationValid,
                  "station", true);
  renderRadioText(NextionIds::RadioObject::ARTIST, artist,
                  radioCache_.artist, radioCache_.artistValid,
                  "artist", true);
  renderRadioText(NextionIds::RadioObject::TITLE, title,
                  radioCache_.title, radioCache_.titleValid,
                  "title", true);
  renderRadioText(NextionIds::RadioObject::BITRATE, bitrate,
                  radioCache_.bitrate, radioCache_.bitrateValid,
                  "bitrate", false);
  renderRadioText(NextionIds::RadioObject::VOLUME, volume,
                  radioCache_.volume, radioCache_.volumeValid,
                  "volume", false);

  if (state_.radio.playback.received && state_.radio.playback.valid) {
    const uint8_t playback =
        static_cast<uint8_t>(state_.radio.playback.value);
    if (!radioCache_.playbackValid ||
        radioCache_.playback != playback) {
      const uint8_t picture =
          state_.radio.playback.value == RadioPlaybackState::PLAYING
              ? NextionIds::Picture::RADIO_PLAY
              : NextionIds::Picture::RADIO_PAUSE;
      if (driver_.setPicture(NextionIds::RadioObject::PLAY_STATE,
                             picture)) {
        radioCache_.playback = playback;
        radioCache_.playbackValid = true;
      }
    }
  }

  if (fullRefresh) {
    state_.ui.fullRefreshRequested = false;
    state_.ui.activePageCacheValid = true;
  }
}

bool NextionUi::renderRadioText(const char* component,
                                const String& value,
                                String& cachedValue, bool& cacheValid,
                                const char* logName, bool utf8) {
  if (cacheValid && cachedValue == value) {
    return true;
  }

  const bool queued = utf8 ? driver_.setTextUtf8(component, value.c_str())
                           : driver_.setText(component, value.c_str());
  if (!queued) {
    return false;
  }

  cachedValue = value;
  cacheValid = true;
  Serial.printf("RADIO UI %s updated\n", logName);
  return true;
}

void NextionUi::invalidateRadioCache() {
  radioCache_.stationValid = false;
  radioCache_.artistValid = false;
  radioCache_.titleValid = false;
  radioCache_.bitrateValid = false;
  radioCache_.volumeValid = false;
  radioCache_.playbackValid = false;
}

void NextionUi::processPageTimeout() {
  // Poprawne 0x66 lub rozpoznany touch nawigacyjny wystarczają do działania UI.
  // Ramka 0x88 nie jest wymagana po restarcie samego ESP.
  if (!state_.ui.nextionSynchronized) {
    return;
  }

  if (state_.ui.currentPage != UiPage::RADIO &&
      state_.ui.currentPage != UiPage::BOILER) {
    return;
  }

  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - state_.ui.lastTouchMs) <
      config_.uiPageTimeoutMs()) {
    return;
  }

  if (!driver_.changePage(NextionIds::Page::START)) {
    Serial.println(
        "UI WARNING: nie udało się zakolejkować powrotu do START");
    state_.ui.lastTouchMs = now;
    return;
  }

  Serial.println("UI TIMEOUT -> START");
  setCurrentPage(UiPage::START);
}
