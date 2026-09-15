#include "display/TouchDriver.h"
#include "config.h"
#include "display/Theme.h"
#include <EEPROM.h>

namespace {

constexpr uint16_t TOUCH_CAL_MAGIC = 0x5843;  // 'XC'
constexpr uint16_t TOUCH_CAL_VERSION = 1;
constexpr uint16_t TOUCH_CAL_EEPROM_SIZE = 128;
constexpr uint16_t TOUCH_CAL_EEPROM_OFFSET = 0;

constexpr int16_t SCREEN_W = 240;
constexpr int16_t SCREEN_H = 320;
constexpr uint8_t CAL_POINT_COUNT = 5;

struct CalibrationBlob {
    uint16_t magic;
    uint16_t version;
    int16_t xMin;
    int16_t xMax;
    int16_t yMin;
    int16_t yMax;
    uint8_t flags;
    uint8_t reserved;
    uint32_t checksum;
};

static_assert(sizeof(CalibrationBlob) <= TOUCH_CAL_EEPROM_SIZE,
              "CalibrationBlob does not fit in EEPROM storage");

uint32_t fnv1a(const uint8_t* data, size_t size) {
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619UL;
    }
    return hash;
}

int16_t median9(int16_t* values, uint8_t count) {
    for (uint8_t i = 1; i < count; ++i) {
        const int16_t key = values[i];
        int8_t j = static_cast<int8_t>(i) - 1;
        while (j >= 0 && values[j] > key) {
            values[j + 1] = values[j];
            --j;
        }
        values[j + 1] = key;
    }
    return values[count / 2];
}

void drawCrosshair(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color) {
    tft.drawCircle(x, y, 12, color);
    tft.drawFastHLine(x - 16, y, 33, color);
    tft.drawFastVLine(x, y - 16, 33, color);
}

}  // namespace

TouchDriver::TouchDriver()
    : touch_(PIN_TOUCH_CS) {}

void TouchDriver::begin() {
    touch_.begin();
    // Rotation 1 leaves the library's measured X/Y axes unchanged.
    touch_.setRotation(1);
    EEPROM.begin(TOUCH_CAL_EEPROM_SIZE);
    eepromReady_ = true;
    Serial.printf("TOUCH EEPROM: begin size=%u\n",
                  static_cast<unsigned>(TOUCH_CAL_EEPROM_SIZE));
    Serial.printf("TOUCH EEPROM: CalibrationData size=%u\n",
                  static_cast<unsigned>(sizeof(CalibrationBlob)));
    Serial.printf("TOUCH EEPROM: offset=%u\n",
                  static_cast<unsigned>(TOUCH_CAL_EEPROM_OFFSET));
    Serial.println("TOUCH XPT2046: ready");
}

TouchPoint TouchDriver::read() {
    TouchPoint out;
    if (!calibrated_ || !touch_.touched()) {
        return out;
    }

    TS_Point raw;
    if (!readRawMedian(raw, AppConfig::TOUCH_READ_SAMPLES)) {
        return out;
    }

    return mapToScreen(raw);
}

bool TouchDriver::hasCalibration() const {
    return calibrated_;
}

TouchDriver::CalibrationError TouchDriver::lastCalibrationError() const {
    return lastCalibrationError_;
}

bool TouchDriver::loadCalibration() {
    lastCalibrationError_ = CalibrationError::NONE;
    if (!eepromReady_) {
        calibrated_ = false;
        Serial.println("TOUCH EEPROM: load skipped, begin not ready");
        return false;
    }

    CalibrationBlob blob{};
    EEPROM.get(TOUCH_CAL_EEPROM_OFFSET, blob);

    Serial.println("TOUCH EEPROM: readback raw header");
    Serial.printf("TOUCH EEPROM: magic=0x%04X\n", blob.magic);
    Serial.printf("TOUCH EEPROM: version=%u\n", blob.version);

    const uint32_t expectedChecksum =
        fnv1a(reinterpret_cast<const uint8_t*>(&blob),
              sizeof(CalibrationBlob) - sizeof(blob.checksum));
    const bool checksumOk = blob.checksum == expectedChecksum;
    Serial.printf("TOUCH EEPROM: checksum %s\n",
                  checksumOk ? "OK" : "FAILED");

    if (blob.magic != TOUCH_CAL_MAGIC ||
        blob.version != TOUCH_CAL_VERSION ||
        !checksumOk) {
        calibrated_ = false;
        Serial.println("TOUCH CALIBRATION: no valid calibration");
        return false;
    }

    Calibration candidate;
    candidate.xMin = blob.xMin;
    candidate.xMax = blob.xMax;
    candidate.yMin = blob.yMin;
    candidate.yMax = blob.yMax;
    candidate.swapAxes = (blob.flags & 0x01U) != 0;
    candidate.invertX = (blob.flags & 0x02U) != 0;
    candidate.invertY = (blob.flags & 0x04U) != 0;

    if (!isCalibrationSane(candidate)) {
        calibrated_ = false;
        Serial.println("TOUCH CALIBRATION: no valid calibration");
        return false;
    }

    calibration_ = candidate;
    calibrated_ = true;
    Serial.println("TOUCH EEPROM: readback OK");
    Serial.println("TOUCH CALIBRATION: stored calibration loaded");
    return true;
}

bool TouchDriver::calibrate(DisplayDriver& display) {
    lastCalibrationError_ = CalibrationError::NONE;
    auto& tft = display.tft();
    tft.fillRect(0, 0, SCREEN_W, SCREEN_H, Theme::BG);
    display.drawUtf8("Kalibracja dotyku", SCREEN_W / 2, 22, 2,
                     Theme::ACCENT, Theme::BG, MC_DATUM);
    display.drawUtf8("Dotknij cel i pusc", SCREEN_W / 2, 48, 2,
                     Theme::TEXT, Theme::BG, MC_DATUM);

    const int16_t targetX[CAL_POINT_COUNT] = {20, 220, 220, 20, 120};
    const int16_t targetY[CAL_POINT_COUNT] = {20, 20, 300, 300, 160};
    TS_Point rawPoints[CAL_POINT_COUNT]{};

    for (uint8_t i = 0; i < CAL_POINT_COUNT; ++i) {
        tft.fillRect(0, 62, SCREEN_W, 258, Theme::BG);
        drawCrosshair(tft, targetX[i], targetY[i], Theme::ACCENT);

        TS_Point raw;
        if (!waitForPressSample(raw) ||
            !waitForRelease(AppConfig::TOUCH_RELEASE_TIMEOUT_MS)) {
            display.drawUtf8("Blad kalibracji", SCREEN_W / 2, 90, 2,
                             Theme::TEXT, Theme::BG, MC_DATUM);
            calibrated_ = false;
            return false;
        }
        rawPoints[i] = raw;
    }

    Calibration candidate;
    const int32_t spanXAsX =
        abs((rawPoints[1].x + rawPoints[2].x) - (rawPoints[0].x + rawPoints[3].x));
    const int32_t spanYAsX =
        abs((rawPoints[1].y + rawPoints[2].y) - (rawPoints[0].y + rawPoints[3].y));
    candidate.swapAxes = spanYAsX > spanXAsX;

    auto rawAxisX = [&](const TS_Point& p) -> int16_t {
        return candidate.swapAxes ? p.y : p.x;
    };
    auto rawAxisY = [&](const TS_Point& p) -> int16_t {
        return candidate.swapAxes ? p.x : p.y;
    };

    const int16_t left = static_cast<int16_t>((rawAxisX(rawPoints[0]) + rawAxisX(rawPoints[3])) / 2);
    const int16_t right = static_cast<int16_t>((rawAxisX(rawPoints[1]) + rawAxisX(rawPoints[2])) / 2);
    const int16_t top = static_cast<int16_t>((rawAxisY(rawPoints[0]) + rawAxisY(rawPoints[1])) / 2);
    const int16_t bottom = static_cast<int16_t>((rawAxisY(rawPoints[3]) + rawAxisY(rawPoints[2])) / 2);

    candidate.invertX = right < left;
    candidate.invertY = bottom < top;
    candidate.xMin = min(left, right);
    candidate.xMax = max(left, right);
    candidate.yMin = min(top, bottom);
    candidate.yMax = max(top, bottom);

    if (!isCalibrationSane(candidate)) {
        display.drawUtf8("Zakres poza norma", SCREEN_W / 2, 90, 2,
                         Theme::TEXT, Theme::BG, MC_DATUM);
        calibrated_ = false;
        return false;
    }

    calibration_ = candidate;
    calibrated_ = true;

    const TouchPoint centerMapped = mapToScreen(rawPoints[4]);
    if (!centerMapped.touched ||
        abs(centerMapped.x - targetX[4]) > AppConfig::TOUCH_CENTER_TOLERANCE_PX ||
        abs(centerMapped.y - targetY[4]) > AppConfig::TOUCH_CENTER_TOLERANCE_PX) {
        display.drawUtf8("Powtorz kalibracje", SCREEN_W / 2, 90, 2,
                         Theme::TEXT, Theme::BG, MC_DATUM);
        calibrated_ = false;
        lastCalibrationError_ = CalibrationError::GEOMETRY;
        return false;
    }

    if (!eepromReady_) {
        display.drawUtf8("Blad EEPROM", SCREEN_W / 2, 90, 2,
                         Theme::TEXT, Theme::BG, MC_DATUM);
        Serial.println("TOUCH EEPROM: put skipped, begin not ready");
        calibrated_ = false;
        lastCalibrationError_ = CalibrationError::EEPROM_NOT_READY;
        return false;
    }

    CalibrationBlob blob{};
    blob.magic = TOUCH_CAL_MAGIC;
    blob.version = TOUCH_CAL_VERSION;
    blob.xMin = calibration_.xMin;
    blob.xMax = calibration_.xMax;
    blob.yMin = calibration_.yMin;
    blob.yMax = calibration_.yMax;
    blob.flags = (calibration_.swapAxes ? 0x01U : 0U) |
                 (calibration_.invertX ? 0x02U : 0U) |
                 (calibration_.invertY ? 0x04U : 0U);
    blob.checksum = fnv1a(reinterpret_cast<const uint8_t*>(&blob),
                          sizeof(CalibrationBlob) - sizeof(blob.checksum));

    Serial.println("TOUCH EEPROM: put");
    EEPROM.put(TOUCH_CAL_EEPROM_OFFSET, blob);

    const bool commitOk = EEPROM.commit();
    Serial.printf("TOUCH EEPROM: commit %s\n", commitOk ? "OK" : "FAILED");
    if (!commitOk) {
        display.drawUtf8("Blad zapisu", SCREEN_W / 2, 90, 2,
                         Theme::TEXT, Theme::BG, MC_DATUM);
        calibrated_ = false;
        lastCalibrationError_ = CalibrationError::EEPROM_COMMIT;
        return false;
    }

    CalibrationBlob verify{};
    EEPROM.get(TOUCH_CAL_EEPROM_OFFSET, verify);
    const uint32_t verifyChecksum =
        fnv1a(reinterpret_cast<const uint8_t*>(&verify),
              sizeof(CalibrationBlob) - sizeof(verify.checksum));
    const bool readbackOk =
        verify.magic == TOUCH_CAL_MAGIC &&
        verify.version == TOUCH_CAL_VERSION &&
        verify.checksum == verifyChecksum &&
        verify.xMin == blob.xMin && verify.xMax == blob.xMax &&
        verify.yMin == blob.yMin && verify.yMax == blob.yMax &&
        verify.flags == blob.flags;

    Serial.printf("TOUCH EEPROM: readback %s\n", readbackOk ? "OK" : "FAILED");
    Serial.printf("TOUCH EEPROM: magic=0x%04X\n", verify.magic);
    Serial.printf("TOUCH EEPROM: version=%u\n", verify.version);
    Serial.printf("TOUCH EEPROM: checksum %s\n",
                  verify.checksum == verifyChecksum ? "OK" : "FAILED");

    if (!readbackOk) {
        display.drawUtf8("Blad zapisu", SCREEN_W / 2, 90, 2,
                         Theme::TEXT, Theme::BG, MC_DATUM);
        calibrated_ = false;
        lastCalibrationError_ = CalibrationError::EEPROM_READBACK;
        return false;
    }

    tft.fillRect(0, 62, SCREEN_W, 258, Theme::BG);
    display.drawUtf8("Kalibracja zapisana", SCREEN_W / 2, 90, 2,
                     Theme::ACCENT, Theme::BG, MC_DATUM);
    delay(600);
    Serial.println("TOUCH CALIBRATION: calibration saved");
    Serial.printf("TOUCH XPT2046: calibrated swap=%u invX=%u invY=%u x=[%d..%d] y=[%d..%d]\n",
                  calibration_.swapAxes ? 1U : 0U,
                  calibration_.invertX ? 1U : 0U,
                  calibration_.invertY ? 1U : 0U,
                  calibration_.xMin, calibration_.xMax,
                  calibration_.yMin, calibration_.yMax);
    return true;
}

bool TouchDriver::detectBootCalibrationHold(DisplayDriver& display,
                                            uint32_t holdMs) {
    if (!touch_.touched()) return false;

    auto& tft = display.tft();
    const uint32_t started = millis();
    while (static_cast<uint32_t>(millis() - started) < holdMs) {
        if (!touch_.touched()) {
            tft.fillRect(0, 280, SCREEN_W, 36, Theme::BG);
            return false;
        }

        const uint32_t elapsed = millis() - started;
        const int16_t width = static_cast<int16_t>((elapsed * (SCREEN_W - 20)) / holdMs);
        tft.drawRect(10, 288, SCREEN_W - 20, 14, Theme::DIM);
        tft.fillRect(11, 289, constrain(width, 0, SCREEN_W - 22), 12,
                     Theme::ACCENT);
        display.drawUtf8("Przytrzymaj: kalibracja", SCREEN_W / 2, 276, 2,
                         Theme::TEXT, Theme::BG, MC_DATUM);
        delay(15);
    }

    while (touch_.touched()) {
        delay(10);
    }
    tft.fillRect(0, 272, SCREEN_W, 48, Theme::BG);
    return true;
}

bool TouchDriver::readRawMedian(TS_Point& out, uint8_t samples) {
    if (!touch_.touched()) return false;

    const uint8_t count = constrain(samples, 3, 9);
    int16_t x[9]{};
    int16_t y[9]{};

    for (uint8_t i = 0; i < count; ++i) {
        if (!touch_.touched()) return false;
        const TS_Point point = touch_.getPoint();
        x[i] = point.x;
        y[i] = point.y;
        delayMicroseconds(800);
    }

    out.x = median9(x, count);
    out.y = median9(y, count);
    out.z = 0;
    return true;
}

bool TouchDriver::waitForPressSample(TS_Point& out) {
    while (touch_.touched()) {
        delay(10);
    }

    while (true) {
        if (touch_.touched() && readRawMedian(out, AppConfig::TOUCH_CALIBRATION_SAMPLES)) {
            return true;
        }
        delay(8);
    }
}

bool TouchDriver::waitForRelease(uint32_t timeoutMs) {
    const uint32_t started = millis();
    while (static_cast<uint32_t>(millis() - started) < timeoutMs) {
        if (!touch_.touched()) return true;
        delay(8);
    }
    return false;
}

TouchPoint TouchDriver::mapToScreen(const TS_Point& raw) const {
    TouchPoint out;
    if (!calibrated_) return out;

    const int32_t rawX = calibration_.swapAxes ? raw.y : raw.x;
    const int32_t rawY = calibration_.swapAxes ? raw.x : raw.y;

    const int32_t mappedX = calibration_.invertX
        ? map(rawX, calibration_.xMax, calibration_.xMin, 0, SCREEN_W - 1)
        : map(rawX, calibration_.xMin, calibration_.xMax, 0, SCREEN_W - 1);
    const int32_t mappedY = calibration_.invertY
        ? map(rawY, calibration_.yMax, calibration_.yMin, 0, SCREEN_H - 1)
        : map(rawY, calibration_.yMin, calibration_.yMax, 0, SCREEN_H - 1);

    out.x = static_cast<int16_t>(constrain(mappedX, 0L, static_cast<long>(SCREEN_W - 1)));
    out.y = static_cast<int16_t>(constrain(mappedY, 0L, static_cast<long>(SCREEN_H - 1)));
    out.touched = true;
    return out;
}

bool TouchDriver::isCalibrationSane(const Calibration& candidate) const {
    const int16_t xSpan = candidate.xMax - candidate.xMin;
    const int16_t ySpan = candidate.yMax - candidate.yMin;
    if (xSpan < AppConfig::TOUCH_MIN_AXIS_SPAN ||
        ySpan < AppConfig::TOUCH_MIN_AXIS_SPAN) {
        return false;
    }
    if (candidate.xMin < 0 || candidate.yMin < 0 ||
        candidate.xMax > 4095 || candidate.yMax > 4095) {
        return false;
    }
    return true;
}
