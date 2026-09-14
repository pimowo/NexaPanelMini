#include "services/TimeService.h"
#include <time.h>
#include "config.h"

void TimeService::begin() {
    configTime(AppConfig::TIMEZONE_RULE,
               AppConfig::NTP_SERVER_PRIMARY,
               AppConfig::NTP_SERVER_SECONDARY,
               AppConfig::NTP_SERVER_TERTIARY);
    Serial.println("NTP START: timezone Europe/Warsaw (CET/CEST)");
}

void TimeService::update(AppState& state) {
    if (millis() - lastFormatMs_ < 1000) return;
    lastFormatMs_ = millis();

    time_t now = time(nullptr);
    if (now < 1700000000) {
        if (state.timeValid) {
            state.timeValid = false;
            ++state.timeRevision;
        }
        return;
    }

    struct tm localTm {};
    localtime_r(&now, &localTm);

    char timeBuf[8];
    char dateBuf[16];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &localTm);
    strftime(dateBuf, sizeof(dateBuf), "%d.%m.%Y", &localTm);

    static const char* weekdays[] = {
        "Niedziela", "Poniedziałek", "Wtorek", "Środa",
        "Czwartek", "Piątek", "Sobota"
    };

    const bool changed = !state.timeValid || state.timeText != timeBuf ||
                         state.dateText != dateBuf ||
                         state.weekdayText != weekdays[localTm.tm_wday];
    state.timeText = timeBuf;
    state.dateText = dateBuf;
    state.weekdayText = weekdays[localTm.tm_wday];
    state.timeValid = true;
    if (changed) ++state.timeRevision;

    if (!synchronizationReported_) {
        synchronizationReported_ = true;
        char zoneBuf[8];
        strftime(zoneBuf, sizeof(zoneBuf), "%Z", &localTm);
        Serial.printf("NTP SYNCED: %s %s zone=%s DST=%s\n",
                      dateBuf, timeBuf, zoneBuf,
                      localTm.tm_isdst > 0 ? "yes" : "no");
    }
}
