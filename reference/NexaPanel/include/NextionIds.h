#pragma once

#include <Arduino.h>

// Stałe techniczne istniejącego projektu HMI. Brakujące identyfikatory zostaną
// uzupełnione po ich odczytaniu z loggera ramek dotyku.
namespace NextionIds {

namespace Page {
constexpr uint8_t START = 0;
constexpr uint8_t RADIO = 1;
constexpr uint8_t BOILER = 2;
constexpr uint8_t BOOT = 3;
constexpr uint8_t UPDATE = 4;
}  // namespace Page

namespace BootObject {
constexpr char STATUS[] = "boot.t_status";
constexpr char DOTS[] = "boot.t_dots";
}  // namespace BootObject

namespace UpdateObject {
constexpr char STATUS[] = "update.t_status";
constexpr char PROGRESS[] = "update.t_progress";
constexpr char WARNING[] = "update.t_warning";
}  // namespace UpdateObject

namespace StartComponent {
constexpr uint8_t NAVIGATE_BOILER = 23;
constexpr uint8_t NAVIGATE_RADIO = 28;
}  // namespace StartComponent

namespace StartObject {
constexpr char CLOCK[] = "main.t_clock";
constexpr char DATE[] = "main.t_date";
constexpr char WEATHER_TEMPERATURE[] = "main.t_temp";
constexpr const char* CALENDAR_TODAY[4] = {
    "main.t_d1", "main.t_d2", "main.t_d3", "main.t_d4"};
constexpr const char* CALENDAR_TOMORROW[4] = {
    "main.t_j1", "main.t_j2", "main.t_j3", "main.t_j4"};
}  // namespace StartObject

namespace WasteObject {
constexpr const char* TERM[4] = {
    "main.t_w_mixed", "main.t_w_plast", "main.t_w_paper",
    "main.t_w_glass"};
constexpr const char* PICTURE[4] = {
    "main.p_wmix", "main.p_wplas", "main.p_wpap", "main.p_wglas"};
}  // namespace WasteObject

namespace Color {
constexpr uint16_t TEXT_DEFAULT = 61407;
constexpr uint16_t CALENDAR_MAGDA = 65504;
constexpr uint16_t CALENDAR_PIOTREK = 61407;
constexpr uint16_t WASTE_ACTIVE = 63488;
}  // namespace Color

namespace RadioComponent {
constexpr uint8_t PREVIOUS = 7;
constexpr uint8_t PLAY_PAUSE = 10;
constexpr uint8_t NEXT = 13;
constexpr uint8_t VOLUME_UP = 16;
constexpr uint8_t NAVIGATE_BOILER = 20;
constexpr uint8_t NAVIGATE_START = 23;
constexpr uint8_t VOLUME_DOWN = 26;
}  // namespace RadioComponent

namespace RadioObject {
constexpr char STATION[] = "radio.t_station";
constexpr char ARTIST[] = "radio.t_artist";
constexpr char TITLE[] = "radio.t_title";
constexpr char BITRATE[] = "radio.t_codec";
constexpr char VOLUME[] = "radio.t_vol";
constexpr char PLAY_STATE[] = "radio.p_play";
}  // namespace RadioObject

namespace BoilerComponent {
constexpr uint8_t NAVIGATE_START = 4;
constexpr uint8_t NAVIGATE_RADIO = 7;
constexpr uint8_t HEAT = 11;
constexpr uint8_t MODE = 14;
constexpr uint8_t POWER = 27;
constexpr uint8_t TEMPERATURE_MINUS = 30;
constexpr uint8_t TEMPERATURE_PLUS = 33;
}  // namespace BoilerComponent

namespace BoilerObject {
constexpr char CURRENT_TEMPERATURE[] = "kociol.t_current_temp";
constexpr char TARGET_TEMPERATURE[] = "kociol.t_target_temp";
constexpr char WEATHER_TEMPERATURE[] = "kociol.t_temp";
constexpr char HEAT[] = "kociol.p_heat";
constexpr char MODE[] = "kociol.p_mode";
constexpr char FIRE[] = "kociol.p_fire";
constexpr char POWER[] = "kociol.p_power";
constexpr char WINDOWS[] = "kociol.p_win";
}  // namespace BoilerObject

namespace Picture {
constexpr uint8_t RADIO_PLAY = 2;
constexpr uint8_t RADIO_PAUSE = 3;
constexpr uint8_t WASTE_MIXED_GRAY = 11;
constexpr uint8_t WASTE_PAPER_GRAY = 12;
constexpr uint8_t WASTE_GLASS_GRAY = 13;
constexpr uint8_t WASTE_PLASTIC_GRAY = 14;
constexpr uint8_t WASTE_GLASS_COLOR = 15;
constexpr uint8_t WASTE_MIXED_COLOR = 16;
constexpr uint8_t WASTE_PAPER_COLOR = 17;
constexpr uint8_t WASTE_PLASTIC_COLOR = 18;
constexpr uint8_t MINUS = 19;
constexpr uint8_t PLUS = 20;
constexpr uint8_t HEAT_GRAY = 21;
constexpr uint8_t HEAT_COLOR = 22;
constexpr uint8_t MOON_COLOR = 23;
constexpr uint8_t SUN_COLOR = 24;
constexpr uint8_t FIRE_COLOR = 25;
constexpr uint8_t FIRE_GRAY = 26;
constexpr uint8_t POWER_GRAY = 27;
constexpr uint8_t POWER_COLOR = 28;
constexpr uint8_t THERMOMETER_COLOR = 29;
constexpr uint8_t THERMOMETER_WHITE = 30;
constexpr uint8_t WINDOW_RED = 31;
constexpr uint8_t WINDOW_GREEN = 32;
constexpr uint8_t MANUAL = 33;
}  // namespace Picture

}  // namespace NextionIds
