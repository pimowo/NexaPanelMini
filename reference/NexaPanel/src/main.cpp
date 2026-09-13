#include <Arduino.h>

#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "AppState.h"
#include "ClockService.h"
#include "FirmwareInfo.h"
#include "HomeAssistantMqtt.h"
#include "NextionDriver.h"
#include "NextionUpdater.h"
#include "NextionUi.h"
#include "RuntimeConfig.h"
#include "WiFiManager.h"
#include "YoRadioClient.h"
#include "config.h"

AppState appState;
RuntimeConfig runtimeConfig;
WiFiManager wifi(appState, runtimeConfig);
HomeAssistantMqtt mqtt(appState, runtimeConfig);
YoRadioClient yoRadio(appState, runtimeConfig);
ClockService clockService(appState, runtimeConfig);
NextionDriver nextion(appState);
NextionUi ui(appState, nextion, yoRadio, clockService, mqtt, runtimeConfig);
NextionUpdater nextionUpdater(appState, nextion, ui, runtimeConfig);

namespace {

bool serial0DiagnosticsEnabled = false;
uint32_t lastHeartbeatMs = 0;
uint32_t lastHealthSampleMs = 0;

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_EXT:
      return "EXTERNAL";
    case ESP_RST_SW:
      return "SOFTWARE";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    case ESP_RST_UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

void printWifiMac(Print& output) {
  uint8_t mac[6]{};
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    output.printf("MAC Wi-Fi: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0],
                  mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    output.println("MAC Wi-Fi: błąd odczytu");
  }
}

void printStartupDiagnostics(Print& output) {
  const uint32_t psramSize = ESP.getPsramSize();
  const esp_reset_reason_t resetReason = esp_reset_reason();

  output.println();
  output.println("============================================================");
  output.println("DIAGNOSTYKA STARTOWA");
  output.printf("Firmware: %s\n", FirmwareInfo::NAME);
  output.printf("Wersja: %s\n", FirmwareInfo::VERSION);
  output.printf("Build: %s\n", FirmwareInfo::BUILD_INFO);
  output.printf("Model chipu: %s\n", ESP.getChipModel());
  output.printf("Revision chipu: %u\n", ESP.getChipRevision());
  output.printf("Liczba rdzeni: %u\n", ESP.getChipCores());
  output.printf("Flash runtime: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFlashChipSize()));

  if (psramSize > 0) {
    output.printf("PSRAM: OK, %lu bytes\n",
                  static_cast<unsigned long>(psramSize));
  } else {
    output.println("PSRAM: BRAK / NIEWYKRYTY");
  }

  output.printf("Wolny heap: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFreeHeap()));
  output.printf("Wolny PSRAM: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFreePsram()));
  printWifiMac(output);
  output.printf("Reset reason: %s (%d)\n", resetReasonName(resetReason),
                static_cast<int>(resetReason));
  output.printf("Nextion UART: TX=%d RX=%d baud=%lu 8N1\n",
                Config::Nextion::TX_PIN, Config::Nextion::RX_PIN,
                static_cast<unsigned long>(Config::Nextion::BAUD_RATE));
  output.println("============================================================");
}

void printDiagnosticLine(const char* text) {
  Serial.println(text);
  if (serial0DiagnosticsEnabled) {
    Serial0.println(text);
  }
}

void printConfigDiagnostics(Print& output) {
  output.printf("CONFIG schema=%u\n", RuntimeConfig::SCHEMA_VERSION);
  output.printf("CONFIG source=%s\n", runtimeConfig.sourceName());
  output.printf("CONFIG hostname=%s\n", runtimeConfig.hostname());
  output.printf("CONFIG mqtt=%s:%u\n", runtimeConfig.mqttHost(),
                runtimeConfig.mqttPort());
  output.printf("CONFIG yoradio=%s:%u%s\n", runtimeConfig.yoRadioHost(),
                runtimeConfig.yoRadioPort(), runtimeConfig.yoRadioPath());
  output.printf("CONFIG timezone=%s\n", runtimeConfig.timezoneName());
  output.println("CONFIG OK");
}

void printHeartbeat() {
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastHeartbeatMs) <
      Config::Logging::HEARTBEAT_INTERVAL_MS) {
    return;
  }

  lastHeartbeatMs = now;
  char localTime[6]{};
  const char* displayedTime =
      clockService.formatTime(localTime, sizeof(localTime)) ? localTime : "--:--";
  Serial.printf(
      "HEALTH uptime=%lu time=%s heap=%lu heap_min=%lu psram=%lu "
      "stack_hwm=%lu loop_max_us=%lu\n",
      static_cast<unsigned long>(now), displayedTime,
      static_cast<unsigned long>(ESP.getFreeHeap()),
      static_cast<unsigned long>(ESP.getMinFreeHeap()),
      static_cast<unsigned long>(ESP.getFreePsram()),
      static_cast<unsigned long>(
          appState.diagnostics.system.loopStackHighWaterBytes),
      static_cast<unsigned long>(appState.diagnostics.loop.maximumDurationUs));
  if (serial0DiagnosticsEnabled) {
    Serial0.printf(
        "HEALTH uptime=%lu time=%s heap=%lu heap_min=%lu psram=%lu "
        "stack_hwm=%lu loop_max_us=%lu\n",
        static_cast<unsigned long>(now), displayedTime,
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMinFreeHeap()),
        static_cast<unsigned long>(ESP.getFreePsram()),
        static_cast<unsigned long>(
            appState.diagnostics.system.loopStackHighWaterBytes),
        static_cast<unsigned long>(appState.diagnostics.loop.maximumDurationUs));
  }
}

void sampleHealth() {
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastHealthSampleMs) <
      Config::Logging::HEALTH_SAMPLE_INTERVAL_MS) {
    return;
  }
  lastHealthSampleMs = now;

  const uint32_t highWaterBytes =
      static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr)) *
      sizeof(StackType_t);
  SystemDiagnostics& diagnostics = appState.diagnostics.system;
  diagnostics.loopStackHighWaterBytes = highWaterBytes;
  if (diagnostics.minimumLoopStackHighWaterBytes == 0 ||
      highWaterBytes < diagnostics.minimumLoopStackHighWaterBytes) {
    diagnostics.minimumLoopStackHighWaterBytes = highWaterBytes;
  }

  if (highWaterBytes < Config::Logging::LOOP_STACK_WARNING_BYTES &&
      !diagnostics.lowStackWarningLogged) {
    diagnostics.lowStackWarningLogged = true;
    Serial.printf("HEALTH WARNING: niski stack loop, high-water=%lu bytes\n",
                  static_cast<unsigned long>(highWaterBytes));
  }
}

void recordLoopDuration(uint32_t durationUs) {
  LoopDiagnostics& diagnostics = appState.diagnostics.loop;
  if (diagnostics.iterations == 0) {
    diagnostics.rollingAverageUs = durationUs;
  } else {
    diagnostics.rollingAverageUs =
        static_cast<uint32_t>((static_cast<uint64_t>(
                                   diagnostics.rollingAverageUs) * 63U +
                               durationUs) /
                              64U);
  }
  ++diagnostics.iterations;
  diagnostics.totalDurationUs += durationUs;
  if (durationUs > diagnostics.maximumDurationUs) {
    diagnostics.maximumDurationUs = durationUs;
  }
  if (durationUs > 10000U) ++diagnostics.over10Ms;
  if (durationUs > 50000U) ++diagnostics.over50Ms;
  if (durationUs > 100000U) ++diagnostics.over100Ms;
}

void confirmRunningFirmware() {
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  if (running != nullptr &&
      esp_ota_get_state_partition(running, &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY) {
    const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
    Serial.printf("OTA: oznaczenie firmware jako valid: %s\n",
                  esp_err_to_name(result));
  }
#endif
}

}  // namespace

void setup() {
  const uint32_t firmwareBootStartedMs = millis();
  Serial.begin(Config::Logging::SERIAL_BAUD_RATE);

  const uint32_t serialWaitStartMs = millis();
  while (!Serial &&
         static_cast<uint32_t>(millis() - serialWaitStartMs) <
             Config::Logging::USB_CDC_READY_TIMEOUT_MS) {
    // Krótkie opóźnienie jest dozwolone wyłącznie podczas startu USB CDC.
    delay(Config::Logging::USB_CDC_POLL_INTERVAL_MS);
  }

  Serial.println("=== KUCHNIA PANEL BOOT ===");

  const esp_reset_reason_t resetReason = esp_reset_reason();
  appState.diagnostics.system.resetReasonCode =
      static_cast<uint32_t>(resetReason);
  strlcpy(appState.diagnostics.system.resetReason,
          resetReasonName(resetReason),
          sizeof(appState.diagnostics.system.resetReason));

  if (Config::Logging::SERIAL0_DIAGNOSTIC_FALLBACK) {
    Serial0.begin(Config::Logging::SERIAL_BAUD_RATE);
    serial0DiagnosticsEnabled = true;
    Serial0.println("=== KUCHNIA PANEL BOOT ===");
  }

  runtimeConfig.load();
  printConfigDiagnostics(Serial);
  if (serial0DiagnosticsEnabled) {
    printConfigDiagnostics(Serial0);
  }

  printStartupDiagnostics(Serial);
  if (serial0DiagnosticsEnabled) {
    printStartupDiagnostics(Serial0);
  }

  wifi.begin();
  mqtt.begin();
  yoRadio.begin();
  clockService.begin();
  nextion.begin();
  ui.begin(firmwareBootStartedMs);
  nextionUpdater.begin();
  confirmRunningFirmware();

  printDiagnosticLine("=== SETUP COMPLETE ===");
  lastHeartbeatMs = millis();
}

void loop() {
  const uint32_t loopStartedUs = micros();
  printHeartbeat();
  sampleHealth();
  wifi.loop();
  mqtt.loop();
  yoRadio.loop();
  clockService.loop();
  nextionUpdater.loop();
  nextion.loop();
  ui.loop();
  recordLoopDuration(static_cast<uint32_t>(micros() - loopStartedUs));
}
