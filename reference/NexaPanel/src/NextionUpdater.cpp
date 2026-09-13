#include "NextionUpdater.h"

#include <ArduinoJson.h>
#include <Update.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <strings.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>

#include "AppState.h"
#include "FirmwareInfo.h"
#include "NextionDriver.h"
#include "NextionUi.h"
#include "RuntimeConfig.h"
#include "WebPanelPage.h"

namespace {
static_assert(Config::NextionUpdate::CHUNK_SIZE == 4096,
              "Protokół uploadu Nextiona wymaga bloków 4096 B");

constexpr char UPLOAD_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="pl">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Aktualizacja HMI Nextion</title>
  <style>
    body{font-family:Arial,sans-serif;max-width:620px;margin:40px auto;padding:0 16px;color:#222}
    fieldset{border:1px solid #bbb;border-radius:8px;padding:20px}
    button{margin-top:14px;padding:10px 18px}progress{width:100%;height:22px;margin-top:18px}
    #status{white-space:pre-wrap;margin-top:12px}.warn{color:#a33;font-weight:bold}
  </style>
</head>
<body>
  <h1>Aktualizacja HMI Nextion</h1>
  <fieldset>
    <legend>Plik TFT</legend>
    <input id="file" type="file" accept=".tft">
    <br><button id="upload" type="button">Wgraj</button>
    <progress id="progress" value="0" max="100"></progress>
    <div id="status">Wybierz plik .tft.</div>
  </fieldset>
  <p class="warn">Podczas aktualizacji nie odłączaj zasilania panelu ani ESP32.</p>
  <script>
    const file=document.getElementById('file');
    const button=document.getElementById('upload');
    const progress=document.getElementById('progress');
    const status=document.getElementById('status');
    button.onclick=()=>{
      const selected=file.files[0];
      if(!selected){status.textContent='Najpierw wybierz plik .tft.';return;}
      if(!selected.name.toLowerCase().endsWith('.tft')){status.textContent='Dozwolony jest wyłącznie plik .tft.';return;}
      button.disabled=true;file.disabled=true;progress.value=0;
      status.textContent='Wysyłanie pliku do panelu: 0%';
      const xhr=new XMLHttpRequest();
      const url='/nextion/upload?name='+encodeURIComponent(selected.name)+'&size='+selected.size;
      xhr.open('POST',url,true);
      xhr.setRequestHeader('Content-Type','application/octet-stream');
      xhr.upload.onprogress=e=>{
        if(e.lengthComputable){
          const percent=Math.round(e.loaded*100/e.total);
          progress.value=percent;
          status.textContent=percent>=100
            ? 'Plik odebrany przez ESP. Trwa programowanie Nextiona...'
            : 'Wysyłanie pliku do panelu: '+percent+'%';
        }
      };
      xhr.onload=()=>{
        if(xhr.status===200){
          progress.value=100;
          status.textContent='Aktualizacja Nextiona zakończona pomyślnie.';
        }else{
          status.textContent=xhr.responseText||('Błąd HTTP '+xhr.status+'.');
        }
        button.disabled=false;file.disabled=false;
      };
      xhr.onerror=()=>{status.textContent='Błąd połączenia z ESP32.';button.disabled=false;file.disabled=false;};
      xhr.onabort=()=>{status.textContent='Wysyłanie przerwane.';button.disabled=false;file.disabled=false;};
      xhr.send(selected);
    };
  </script>
</body>
</html>)HTML";

bool elapsed(uint32_t startedMs, uint32_t intervalMs) {
  return static_cast<uint32_t>(millis() - startedMs) >= intervalMs;
}

const char* otaStateName(esp_ota_img_states_t state) {
  switch (state) {
    case ESP_OTA_IMG_NEW:
      return "NEW";
    case ESP_OTA_IMG_PENDING_VERIFY:
      return "PENDING_VERIFY";
    case ESP_OTA_IMG_VALID:
      return "VALID";
    case ESP_OTA_IMG_INVALID:
      return "INVALID";
    case ESP_OTA_IMG_ABORTED:
      return "ABORTED";
    case ESP_OTA_IMG_UNDEFINED:
    default:
      return "UNDEFINED";
  }
}

const char* yoRadioDisconnectReasonName(YoRadioDisconnectReason reason) {
  switch (reason) {
    case YoRadioDisconnectReason::WIFI_LOST:
      return "WIFI_LOST";
    case YoRadioDisconnectReason::CONNECT_TIMEOUT:
      return "CONNECT_TIMEOUT";
    case YoRadioDisconnectReason::FIRST_DATA_TIMEOUT:
      return "FIRST_DATA_TIMEOUT";
    case YoRadioDisconnectReason::REMOTE_DISCONNECT:
      return "REMOTE_DISCONNECT";
    case YoRadioDisconnectReason::WEBSOCKET_ERROR:
      return "WEBSOCKET_ERROR";
    case YoRadioDisconnectReason::NONE:
    default:
      return "NONE";
  }
}

void logHttpStack(const char* stage) {
  if (!Config::Logging::DEBUG_HTTP_STACK) return;
  Serial.printf("API STACK stage=%s highWater=%lu bytes\n", stage,
                static_cast<unsigned long>(
                    uxTaskGetStackHighWaterMark(nullptr)));
}

int hexValue(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

bool urlDecode(const char* input, size_t inputLength, char* output,
               size_t outputSize) {
  if (input == nullptr || output == nullptr || outputSize == 0) return false;

  size_t outputLength = 0;
  for (size_t index = 0; index < inputLength; ++index) {
    char decoded = input[index];
    if (decoded == '%' && index + 2 < inputLength) {
      const int high = hexValue(input[index + 1]);
      const int low = hexValue(input[index + 2]);
      if (high < 0 || low < 0) return false;
      decoded = static_cast<char>((high << 4) | low);
      index += 2;
    } else if (decoded == '+') {
      decoded = ' ';
    }

    if (static_cast<uint8_t>(decoded) < 0x20U || decoded == '/' ||
        decoded == '\\') {
      return false;
    }
    if (outputLength + 1 >= outputSize) return false;
    output[outputLength++] = decoded;
  }
  output[outputLength] = '\0';
  return outputLength > 0;
}

bool getQueryValue(const char* target, const char* key, char* output,
                   size_t outputSize) {
  const char* query = strchr(target, '?');
  if (query == nullptr) return false;
  ++query;

  const size_t keyLength = strlen(key);
  while (*query != '\0') {
    const char* valueEnd = strchr(query, '&');
    if (valueEnd == nullptr) valueEnd = query + strlen(query);
    const char* equals = static_cast<const char*>(memchr(query, '=', valueEnd - query));
    if (equals != nullptr && static_cast<size_t>(equals - query) == keyLength &&
        strncmp(query, key, keyLength) == 0) {
      return urlDecode(equals + 1, valueEnd - equals - 1, output, outputSize);
    }
    query = *valueEnd == '&' ? valueEnd + 1 : valueEnd;
  }
  return false;
}

const char* findHeaderValue(const char* header, const char* name) {
  const size_t nameLength = strlen(name);
  const char* line = strstr(header, "\r\n");
  if (line == nullptr) return nullptr;
  line += 2;

  while (*line != '\0' && !(line[0] == '\r' && line[1] == '\n')) {
    const char* lineEnd = strstr(line, "\r\n");
    if (lineEnd == nullptr) return nullptr;
    if (static_cast<size_t>(lineEnd - line) > nameLength &&
        strncasecmp(line, name, nameLength) == 0 && line[nameLength] == ':') {
      const char* value = line + nameLength + 1;
      while (value < lineEnd && (*value == ' ' || *value == '\t')) ++value;
      return value;
    }
    line = lineEnd + 2;
  }
  return nullptr;
}

bool parsePositiveSize(const char* text, size_t& result) {
  if (text == nullptr) return false;
  char* end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 10);
  if (end == text || parsed == 0 || parsed > SIZE_MAX) return false;
  while (*end == ' ' || *end == '\t') ++end;
  if (*end != '\0' && *end != '\r' && *end != '&') return false;
  result = static_cast<size_t>(parsed);
  return true;
}

bool endsWithTft(const char* fileName) {
  const size_t length = strlen(fileName);
  return length >= 4 && strcasecmp(fileName + length - 4, ".tft") == 0;
}

bool endsWithBin(const char* fileName) {
  const size_t length = strlen(fileName);
  return length >= 4 && strcasecmp(fileName + length - 4, ".bin") == 0;
}

template <size_t N>
bool mergeText(JsonObjectConst root, const char* key, char (&target)[N],
               const char*& error) {
  if (!root.containsKey(key)) return true;
  JsonVariantConst value = root[key];
  if (!value.is<const char*>()) {
    error = "Invalid field type";
    return false;
  }
  const char* text = value.as<const char*>();
  if (text == nullptr || strlen(text) >= N) {
    error = "Field value too long";
    return false;
  }
  strlcpy(target, text, N);
  return true;
}

bool mergeBool(JsonObjectConst root, const char* key, bool& target,
               const char*& error) {
  if (!root.containsKey(key)) return true;
  JsonVariantConst value = root[key];
  if (!value.is<bool>()) {
    error = "Invalid field type";
    return false;
  }
  target = value.as<bool>();
  return true;
}

bool mergePort(JsonObjectConst root, const char* key, uint16_t& target,
               const char* portError, const char*& error) {
  if (!root.containsKey(key)) return true;
  JsonVariantConst value = root[key];
  if (!value.is<uint32_t>()) {
    error = portError;
    return false;
  }
  const uint32_t port = value.as<uint32_t>();
  if (port > UINT16_MAX) {
    error = portError;
    return false;
  }
  target = static_cast<uint16_t>(port);
  return true;
}

bool mergeUnsigned(JsonObjectConst root, const char* key, uint32_t& target,
                   const char*& error) {
  if (!root.containsKey(key)) return true;
  JsonVariantConst value = root[key];
  if (!value.is<uint32_t>()) {
    error = "Invalid unsigned value";
    return false;
  }
  target = value.as<uint32_t>();
  return true;
}

template <size_t N>
bool extractSecret(JsonObjectConst root, const char* key, char (&target)[N],
                   size_t maximumLength, const char*& error) {
  target[0] = '\0';
  if (!root.containsKey(key) || root[key].isNull()) return true;
  JsonVariantConst value = root[key];
  if (!value.is<const char*>()) {
    error = "Invalid password type";
    return false;
  }
  const char* text = value.as<const char*>();
  if (text == nullptr || strlen(text) > maximumLength) {
    error = "Invalid password length";
    return false;
  }
  strlcpy(target, text, N);
  return true;
}

void populatePublicConfig(JsonObject object,
                          const RuntimeConfig::PublicValues& values) {
  object["deviceName"] = values.deviceName;
  object["hostname"] = values.hostname;
  object["wifiSsid"] = values.wifiSsid;
  object["wifiPasswordSet"] = values.wifiPasswordConfigured;
  object["wifiDhcp"] = values.wifiDhcp;
  object["wifiStaticIp"] = values.wifiStaticIp;
  object["wifiGateway"] = values.wifiGateway;
  object["wifiSubnet"] = values.wifiSubnet;
  object["wifiDns1"] = values.wifiDns1;
  object["wifiDns2"] = values.wifiDns2;
  object["mqttHost"] = values.mqttHost;
  object["mqttPort"] = values.mqttPort;
  object["mqttUser"] = values.mqttUsername;
  object["mqttPasswordSet"] = values.mqttPasswordConfigured;
  object["mqttClientId"] = values.mqttClientId;
  object["mqttBaseTopic"] = values.mqttBaseTopic;
  object["discoveryPrefix"] = values.haDiscoveryPrefix;
  object["discoveryEnabled"] = values.haDiscoveryEnabled;
  object["yoRadioHost"] = values.yoRadioHost;
  object["yoRadioPort"] = values.yoRadioPort;
  object["yoRadioPath"] = values.yoRadioPath;
  object["timezoneName"] = values.timezoneName;
  object["timezoneRule"] = values.timezoneRule;
  object["uiTimeoutMs"] = values.uiPageTimeoutMs;
}

bool mergePublicConfig(JsonObjectConst root,
                       RuntimeConfig::PublicValues& values,
                       const char*& error) {
  return
      mergeText(root, "deviceName", values.deviceName, error) &&
      mergeText(root, "hostname", values.hostname, error) &&
      mergeText(root, "wifiSsid", values.wifiSsid, error) &&
      mergeBool(root, "wifiDhcp", values.wifiDhcp, error) &&
      mergeText(root, "wifiStaticIp", values.wifiStaticIp, error) &&
      mergeText(root, "wifiGateway", values.wifiGateway, error) &&
      mergeText(root, "wifiSubnet", values.wifiSubnet, error) &&
      mergeText(root, "wifiDns1", values.wifiDns1, error) &&
      mergeText(root, "wifiDns2", values.wifiDns2, error) &&
      mergeText(root, "mqttHost", values.mqttHost, error) &&
      mergePort(root, "mqttPort", values.mqttPort, "Invalid MQTT port",
                error) &&
      mergeText(root, "mqttUser", values.mqttUsername, error) &&
      mergeText(root, "mqttClientId", values.mqttClientId, error) &&
      mergeText(root, "mqttBaseTopic", values.mqttBaseTopic, error) &&
      mergeText(root, "discoveryPrefix", values.haDiscoveryPrefix, error) &&
      mergeBool(root, "discoveryEnabled", values.haDiscoveryEnabled,
                error) &&
      mergeText(root, "yoRadioHost", values.yoRadioHost, error) &&
      mergePort(root, "yoRadioPort", values.yoRadioPort,
                "Invalid yoRadio port", error) &&
      mergeText(root, "yoRadioPath", values.yoRadioPath, error) &&
      mergeText(root, "timezoneName", values.timezoneName, error) &&
      mergeText(root, "timezoneRule", values.timezoneRule, error) &&
      mergeUnsigned(root, "uiTimeoutMs", values.uiPageTimeoutMs, error);
}

template <typename TDocument>
bool sendJsonDocument(NetworkClient& client, int status, const char* reason,
                      TDocument& document, const char* disposition = nullptr) {
  const size_t bodyLength = measureJson(document);
  char responseHeader[288]{};
  const int length = snprintf(
      responseHeader, sizeof(responseHeader),
      "HTTP/1.1 %d %s\r\nContent-Type: application/json; charset=utf-8\r\n"
      "Content-Length: %u\r\nConnection: close\r\n"
      "Cache-Control: no-store\r\n%s%s%s\r\n",
      status, reason, static_cast<unsigned>(bodyLength),
      disposition == nullptr ? "" : "Content-Disposition: attachment; filename=\"",
      disposition == nullptr ? "" : disposition,
      disposition == nullptr ? "" : "\"\r\n");
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(responseHeader)) {
    return false;
  }
  client.write(reinterpret_cast<const uint8_t*>(responseHeader), length);
  return serializeJson(document, client) == bodyLength;
}
}  // namespace

NextionUpdater::NextionUpdater(AppState& state, NextionDriver& driver,
                               NextionUi& ui, RuntimeConfig& config)
    : state_(state),
      driver_(driver),
      ui_(ui),
      config_(config),
      server_(Config::NextionUpdate::WEB_PORT, 2) {}

void NextionUpdater::begin() {
  if (!Config::NextionUpdate::ENABLED) {
    Serial.println("NEXTION UPDATE: serwer wyłączony w konfiguracji");
    return;
  }

  server_.setNoDelay(true);
  server_.begin();
  Serial.printf("NEXTION UPDATE: strona HTTP /nextion na porcie %u, upload baud=%lu\n",
                Config::NextionUpdate::WEB_PORT,
                static_cast<unsigned long>(Config::NextionUpdate::UPLOAD_BAUD));
}

void NextionUpdater::loop() {
  if (!Config::NextionUpdate::ENABLED) return;

  if (restartPending_ &&
      elapsed(restartScheduledMs_, restartDelayMs_)) {
    Serial.println("API: kontrolowany restart ESP");
    ESP.restart();
    return;
  }

  acceptClient();
  switch (phase_) {
    case Phase::IDLE:
      break;
    case Phase::READ_HTTP_HEADER:
      readHttpHeader();
      break;
    case Phase::READ_JSON_BODY:
      readJsonBody();
      break;
    case Phase::RECEIVE_FIRMWARE:
      receiveFirmware();
      break;
    case Phase::PREPARE_UPDATE_SCREEN:
      prepareUpdateScreen();
      break;
    case Phase::DRAIN_UPDATE_SCREEN_RX:
      drainUpdateScreenRx();
      break;
    case Phase::WAIT_CONNECT_RESPONSE:
      waitForConnectResponse();
      break;
    case Phase::WAIT_BAUD_SWITCH:
      if (elapsed(phaseStartedMs_, Config::NextionUpdate::BAUD_SWITCH_DELAY_MS)) {
        driver_.clearUpdateRx();
        driver_.setUpdateBaud(Config::NextionUpdate::UPLOAD_BAUD);
        phase_ = Phase::WAIT_INITIAL_ACK;
        phaseStartedMs_ = millis();
      }
      break;
    case Phase::WAIT_INITIAL_ACK:
      waitForInitialAck();
      break;
    case Phase::RECEIVE_BLOCK:
      receiveBlock();
      break;
    case Phase::WAIT_BLOCK_ACK:
      waitForBlockAck();
      break;
    case Phase::WAIT_NEXTION_REBOOT:
      if (elapsed(phaseStartedMs_, Config::NextionUpdate::NEXTION_REBOOT_WAIT_MS)) {
        beginNormalRecovery(true);
      }
      break;
    case Phase::WAIT_NORMAL_BAUD_SETTLE:
      waitForNormalBaudSettle();
      break;
    case Phase::WAIT_RECOVERY_RESPONSE:
      waitForRecoveryResponse();
      break;
    case Phase::PREPARE_SUCCESS_SCREEN:
      prepareSuccessScreen();
      break;
    case Phase::PREPARE_ERROR_SCREEN:
      prepareErrorScreen();
      break;
    case Phase::WAIT_ERROR_SCREEN_STABLE:
      if (!ui_.updateScreenPrepared()) {
        resultScreenQueued_ = false;
        phase_ = Phase::PREPARE_ERROR_SCREEN;
      } else {
        active_ = false;
        phase_ = Phase::IDLE;
        Serial.println("NEXTION UPDATE: komunikat błędu pokazany na LCD");
      }
      break;
    case Phase::WAIT_ESP_RESTART:
      if (!ui_.updateScreenPrepared()) {
        resultScreenQueued_ = false;
        phase_ = Phase::PREPARE_SUCCESS_SCREEN;
        break;
      }
      if (elapsed(phaseStartedMs_,
                  Config::NextionUpdate::SUCCESS_RESTART_DELAY_MS)) {
        Serial.println("NEXTION UPDATE: kontrolowany restart ESP");
        ESP.restart();
      }
      break;
  }
}

bool NextionUpdater::active() const {
  return active_;
}

void NextionUpdater::acceptClient() {
  if (!server_.hasClient()) return;

  NetworkClient incoming = server_.accept();
  if (!incoming) return;
  incoming.setNoDelay(true);
  ++state_.diagnostics.http.requests;

  if (client_ || active_ || firmwareUpdateActive_ ||
      phase_ != Phase::IDLE || restartPending_) {
    sendBusyResponse(incoming);
    incoming.stop();
    return;
  }

  client_ = incoming;
  releaseJsonBody();
  headerLength_ = 0;
  header_[0] = '\0';
  expectedJsonBodyLength_ = 0;
  jsonBodyLength_ = 0;
  lastHttpActivityMs_ = millis();
  phaseStartedMs_ = millis();
  phase_ = Phase::READ_HTTP_HEADER;
}

void NextionUpdater::readHttpHeader() {
  size_t processed = 0;
  while (client_.available() > 0 &&
         processed < Config::NextionUpdate::HTTP_READ_BYTES_PER_LOOP) {
    const int value = client_.read();
    if (value < 0) break;
    lastHttpActivityMs_ = millis();
    ++processed;

    if (headerLength_ >= Config::NextionUpdate::HTTP_HEADER_MAX_SIZE) {
      sendTextResponse(client_, 431, "Request Header Fields Too Large",
                       "Nagłówek HTTP jest zbyt duży.");
      closeHttpClient();
      phase_ = Phase::IDLE;
      return;
    }

    header_[headerLength_++] = static_cast<char>(value);
    header_[headerLength_] = '\0';
    if (headerLength_ >= 4 &&
        memcmp(header_ + headerLength_ - 4, "\r\n\r\n", 4) == 0) {
      handleHttpRequest();
      return;
    }
  }

  if (!client_.connected() && client_.available() == 0) {
    closeHttpClient();
    phase_ = Phase::IDLE;
  } else if (requestTimedOut(Config::NextionUpdate::HTTP_HEADER_TIMEOUT_MS)) {
    sendTextResponse(client_, 408, "Request Timeout",
                     "Timeout podczas odczytu nagłówka HTTP.");
    closeHttpClient();
    phase_ = Phase::IDLE;
  }
}

void NextionUpdater::readJsonBody() {
  if (jsonBody_ == nullptr) {
    if (jsonRequest_ == JsonRequest::CONFIG_IMPORT) {
      ++state_.diagnostics.http.configImportFailures;
    } else {
      ++state_.diagnostics.http.configSaveFailures;
    }
    sendJsonResponse(
        client_, 500, "Internal Server Error",
        "{\"ok\":false,\"error\":\"JSON buffer unavailable\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  size_t processed = 0;
  while (client_.available() > 0 &&
         processed < Config::NextionUpdate::HTTP_READ_BYTES_PER_LOOP &&
         jsonBodyLength_ < expectedJsonBodyLength_) {
    const size_t available = static_cast<size_t>(client_.available());
    const size_t remaining = expectedJsonBodyLength_ - jsonBodyLength_;
    const size_t loopBudget =
        Config::NextionUpdate::HTTP_READ_BYTES_PER_LOOP - processed;
    const size_t requested = min(available, min(remaining, loopBudget));
    const int readLength = client_.read(
        reinterpret_cast<uint8_t*>(jsonBody_ + jsonBodyLength_), requested);
    if (readLength <= 0) break;
    jsonBodyLength_ += static_cast<size_t>(readLength);
    processed += static_cast<size_t>(readLength);
    lastHttpActivityMs_ = millis();
  }

  jsonBody_[jsonBodyLength_] = '\0';
  if (jsonBodyLength_ == expectedJsonBodyLength_) {
    logHttpStack("body-received");
    if (jsonRequest_ == JsonRequest::CONFIG_IMPORT) {
      importJsonConfig();
    } else {
      applyJsonConfig();
    }
    return;
  }

  if (!client_.connected() && client_.available() == 0) {
    if (jsonRequest_ == JsonRequest::CONFIG_IMPORT) {
      ++state_.diagnostics.http.configImportFailures;
    } else {
      ++state_.diagnostics.http.configSaveFailures;
    }
    sendJsonResponse(client_, 400, "Bad Request",
                     "{\"ok\":false,\"error\":\"Incomplete JSON body\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
  } else if (elapsed(lastHttpActivityMs_,
                      Config::NextionUpdate::HTTP_BODY_TIMEOUT_MS)) {
    if (jsonRequest_ == JsonRequest::CONFIG_IMPORT) {
      ++state_.diagnostics.http.configImportFailures;
    } else {
      ++state_.diagnostics.http.configSaveFailures;
    }
    sendJsonResponse(client_, 408, "Request Timeout",
                     "{\"ok\":false,\"error\":\"JSON body timeout\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
  }
}

void NextionUpdater::handleHttpRequest() {
  char method[8]{};
  char target[384]{};
  char version[16]{};
  if (sscanf(header_, "%7s %383s %15s", method, target, version) != 3) {
    sendTextResponse(client_, 400, "Bad Request", "Niepoprawne żądanie HTTP.");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  if (strcmp(method, "GET") == 0 && strcmp(target, "/nextion") == 0) {
    serveUploadPage();
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  if (strcmp(method, "GET") == 0 && strcmp(target, "/") == 0) {
    serveMainPage();
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  if (strcmp(method, "GET") == 0 && strcmp(target, "/api/config") == 0) {
    serveConfig();
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  if (strcmp(method, "GET") == 0 &&
      strcmp(target, "/api/config/export") == 0) {
    exportConfig();
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  if (strcmp(method, "GET") == 0 && strcmp(target, "/api/status") == 0) {
    serveStatus();
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  if (strcmp(method, "POST") == 0 &&
      strcmp(target, "/api/config/reset") == 0) {
    resetConfig();
    return;
  }

  if (strcmp(method, "POST") == 0 &&
      strcmp(target, "/api/restart") == 0) {
    scheduleRestart();
    return;
  }

  const bool configUpdate =
      strcmp(method, "POST") == 0 && strcmp(target, "/api/config") == 0;
  const bool configImport =
      strcmp(method, "POST") == 0 &&
      strcmp(target, "/api/config/import") == 0;
  if (configUpdate || configImport) {
    jsonRequest_ = configImport ? JsonRequest::CONFIG_IMPORT
                                : JsonRequest::CONFIG_UPDATE;
    logHttpStack("post-begin");
    size_t contentLength = 0;
    const char* contentLengthText =
        findHeaderValue(header_, "Content-Length");
    const char* contentType = findHeaderValue(header_, "Content-Type");
    if (!parsePositiveSize(contentLengthText, contentLength)) {
      sendJsonResponse(
          client_, 400, "Bad Request",
          "{\"ok\":false,\"error\":\"Missing JSON Content-Length\"}");
    } else if (contentLength > Config::NextionUpdate::JSON_BODY_MAX_SIZE) {
      sendJsonResponse(
          client_, 413, "Payload Too Large",
          "{\"ok\":false,\"error\":\"JSON body is too large\"}");
    } else if (contentType == nullptr ||
               strncasecmp(contentType, "application/json", 16) != 0) {
      sendJsonResponse(
          client_, 415, "Unsupported Media Type",
          "{\"ok\":false,\"error\":\"Content-Type must be application/json\"}");
    } else {
      beginJsonRequest(
          contentLength, configImport ? JsonRequest::CONFIG_IMPORT
                                      : JsonRequest::CONFIG_UPDATE);
      return;
    }

    if (configImport) {
      ++state_.diagnostics.http.configImportFailures;
    } else {
      ++state_.diagnostics.http.configSaveFailures;
    }
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  if (strcmp(method, "POST") == 0 &&
      strncmp(target, "/api/firmware/upload?",
              strlen("/api/firmware/upload?")) == 0) {
    char decodedName[Config::NextionUpdate::FILE_NAME_MAX_SIZE + 1]{};
    size_t contentLength = 0;
    const char* contentLengthText =
        findHeaderValue(header_, "Content-Length");
    const char* contentType = findHeaderValue(header_, "Content-Type");
    const esp_partition_t* updatePartition =
        esp_ota_get_next_update_partition(nullptr);

    if (!getQueryValue(target, "name", decodedName, sizeof(decodedName)) ||
        !endsWithBin(decodedName)) {
      sendJsonResponse(
          client_, 415, "Unsupported Media Type",
          "{\"ok\":false,\"error\":\"Dozwolony jest tylko plik .bin\"}");
    } else if (!parsePositiveSize(contentLengthText, contentLength)) {
      sendJsonResponse(
          client_, 400, "Bad Request",
          "{\"ok\":false,\"error\":\"Brak poprawnego Content-Length\"}");
    } else if (updatePartition == nullptr) {
      sendJsonResponse(
          client_, 500, "Internal Server Error",
          "{\"ok\":false,\"error\":\"Brak nieaktywnej partycji OTA\"}");
    } else if (contentLength > updatePartition->size) {
      sendJsonResponse(
          client_, 413, "Payload Too Large",
          "{\"ok\":false,\"error\":\"Firmware przekracza rozmiar slotu OTA\"}");
    } else if (contentType == nullptr ||
               strncasecmp(contentType, "application/octet-stream", 24) != 0) {
      sendJsonResponse(
          client_, 415, "Unsupported Media Type",
          "{\"ok\":false,\"error\":\"Wymagany jest application/octet-stream\"}");
    } else {
      startFirmwareUpload(decodedName, contentLength);
      return;
    }

    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  if (strcmp(method, "POST") != 0 ||
      strncmp(target, "/nextion/upload?", strlen("/nextion/upload?")) != 0) {
    sendTextResponse(client_, 404, "Not Found", "Nie znaleziono zasobu.");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  char encodedSize[24]{};
  char decodedName[Config::NextionUpdate::FILE_NAME_MAX_SIZE + 1]{};
  size_t querySize = 0;
  size_t contentLength = 0;
  const char* contentLengthText = findHeaderValue(header_, "Content-Length");
  const char* contentType = findHeaderValue(header_, "Content-Type");

  if (!getQueryValue(target, "name", decodedName, sizeof(decodedName)) ||
      !endsWithTft(decodedName)) {
    sendTextResponse(client_, 415, "Unsupported Media Type",
                     "Dozwolony jest wyłącznie plik z rozszerzeniem .tft.");
  } else if (!getQueryValue(target, "size", encodedSize, sizeof(encodedSize)) ||
             !parsePositiveSize(encodedSize, querySize) ||
             !parsePositiveSize(contentLengthText, contentLength)) {
    sendTextResponse(client_, 400, "Bad Request",
                     "Brak poprawnego, niezerowego rozmiaru pliku.");
  } else if (querySize != contentLength) {
    sendTextResponse(client_, 400, "Bad Request",
                     "Rozmiar pliku nie zgadza się z Content-Length.");
  } else if (contentLength > Config::NextionUpdate::MAX_FILE_SIZE) {
    sendTextResponse(client_, 413, "Payload Too Large",
                     "Plik przekracza limit pamięci modelu Nextion.");
  } else if (contentType == nullptr ||
             strncasecmp(contentType, "application/octet-stream", 24) != 0) {
    sendTextResponse(client_, 415, "Unsupported Media Type",
                     "Wymagany jest typ application/octet-stream.");
  } else {
    startUpload(decodedName, contentLength);
    return;
  }

  closeHttpClient();
  phase_ = Phase::IDLE;
}

void NextionUpdater::serveMainPage() {
  char responseHeader[192]{};
  const int length = snprintf(
      responseHeader, sizeof(responseHeader),
      "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
      "Content-Length: %u\r\nConnection: close\r\n"
      "Cache-Control: no-store\r\n\r\n",
      static_cast<unsigned>(WEB_PANEL_PAGE_LENGTH));
  if (length > 0 && static_cast<size_t>(length) < sizeof(responseHeader)) {
    client_.write(reinterpret_cast<const uint8_t*>(responseHeader), length);
    client_.write_P(WEB_PANEL_PAGE, WEB_PANEL_PAGE_LENGTH);
    recordHttpResponse(200);
  }
}

void NextionUpdater::serveConfig() {
  RuntimeConfig::PublicValues values;
  config_.copyPublic(values);

  DynamicJsonDocument document(2048);
  document["schema"] = RuntimeConfig::SCHEMA_VERSION;
  document["source"] = config_.sourceName();
  populatePublicConfig(document.as<JsonObject>(), values);
  const bool sent = !document.overflowed() &&
                    sendJsonDocument(client_, 200, "OK", document);
  if (!sent) {
    sendJsonResponse(
        client_, 500, "Internal Server Error",
        "{\"ok\":false,\"error\":\"JSON serialization failed\"}");
  } else {
    recordHttpResponse(200);
  }
}

void NextionUpdater::exportConfig() {
  RuntimeConfig::PublicValues values;
  config_.copyPublic(values);

  DynamicJsonDocument document(2560);
  document["format"] = "kuchnia-panel-config";
  document["version"] = 1;
  document["firmware"] = FirmwareInfo::VERSION;
  populatePublicConfig(document.createNestedObject("config"), values);
  const bool sent = !document.overflowed() &&
                    sendJsonDocument(client_, 200, "OK", document,
                                     "kuchnia-panel-config.json");
  if (!sent) {
    sendJsonResponse(
        client_, 500, "Internal Server Error",
        "{\"ok\":false,\"error\":\"JSON export failed\"}");
  } else {
    recordHttpResponse(200);
  }
}

void NextionUpdater::serveStatus() {
  DynamicJsonDocument document(6144);
  const uint32_t nowMs = millis();
  const DiagnosticsState& diagnostics = state_.diagnostics;
  const esp_partition_t* runningPartition = esp_ota_get_running_partition();
  const esp_partition_t* nextPartition =
      esp_ota_get_next_update_partition(nullptr);
  esp_ota_img_states_t otaState = ESP_OTA_IMG_UNDEFINED;
  if (runningPartition != nullptr) {
    esp_ota_get_state_partition(runningPartition, &otaState);
  }
  document["firmwareVersion"] = FirmwareInfo::VERSION;
  document["build"] = FirmwareInfo::BUILD_INFO;
  document["uptime"] = nowMs / 1000UL;
  document["freeHeap"] = ESP.getFreeHeap();
  document["freePsram"] = ESP.getFreePsram();
  document["wifiConnected"] = state_.network.connected;
  document["ip"] = state_.network.ipAddress;
  document["rssi"] = state_.network.rssi;
  document["mqttConnected"] = state_.homeAssistant.mqttConnected;
  document["yoRadioConnected"] =
      state_.radio.connection == RadioConnectionState::ONLINE &&
      state_.radio.firstDataReceived;
  document["ntpValid"] = state_.clock.timeValid;

  if (state_.clock.timeValid) {
    const time_t now = time(nullptr);
    tm localTime{};
    char formattedTime[32]{};
    if (now > 0 && localtime_r(&now, &localTime) != nullptr &&
        strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%dT%H:%M:%S%z",
                 &localTime) > 0) {
      document["currentTime"] = formattedTime;
    } else {
      document["currentTime"] = nullptr;
    }
  } else {
    document["currentTime"] = nullptr;
  }

  document["configSource"] = config_.sourceName();
  document["configSchema"] = RuntimeConfig::SCHEMA_VERSION;
  document["firmwareUpdateActive"] = firmwareUpdateActive_;
  document["firmwareUpdateProgress"] = firmwareUpdateProgress_;

  JsonObject system = document.createNestedObject("system");
  system["resetReasonCode"] = diagnostics.system.resetReasonCode;
  system["resetReason"] = diagnostics.system.resetReason;
  system["activeOtaSlot"] =
      runningPartition != nullptr ? runningPartition->label : "unknown";
  system["activeOtaAddress"] =
      runningPartition != nullptr ? runningPartition->address : 0;
  system["activeOtaSize"] =
      runningPartition != nullptr ? runningPartition->size : 0;
  system["nextOtaSlot"] =
      nextPartition != nullptr ? nextPartition->label : "unknown";
  system["nextOtaAddress"] =
      nextPartition != nullptr ? nextPartition->address : 0;
  system["nextOtaSize"] =
      nextPartition != nullptr ? nextPartition->size : 0;
  system["otaState"] = otaStateName(otaState);
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
  system["rollbackEnabled"] = true;
#else
  system["rollbackEnabled"] = false;
#endif

  JsonObject memory = document.createNestedObject("memory");
  memory["heapFree"] = ESP.getFreeHeap();
  memory["heapMinimum"] = ESP.getMinFreeHeap();
  memory["heapLargestBlock"] =
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  memory["psramFree"] = ESP.getFreePsram();
  memory["psramMinimum"] = ESP.getPsramSize() > 0
                                ? heap_caps_get_minimum_free_size(
                                      MALLOC_CAP_SPIRAM)
                                : 0;
  memory["loopStackHighWaterBytes"] =
      diagnostics.system.loopStackHighWaterBytes;
  memory["loopStackMinimumHighWaterBytes"] =
      diagnostics.system.minimumLoopStackHighWaterBytes;

  JsonObject wifi = document.createNestedObject("wifi");
  wifi["connectionAttempts"] = diagnostics.wifi.connectionAttempts;
  wifi["successfulConnections"] = diagnostics.wifi.successfulConnections;
  wifi["disconnects"] = diagnostics.wifi.disconnects;
  wifi["connectTimeouts"] = diagnostics.wifi.connectTimeouts;
  wifi["reconnects"] = diagnostics.wifi.reconnects;
  wifi["lastConnectedMs"] = diagnostics.wifi.lastConnectedMs;
  wifi["lastDisconnectedMs"] = diagnostics.wifi.lastDisconnectedMs;
  wifi["rssi"] = state_.network.rssi;
  if (diagnostics.wifi.minimumRssiValid) {
    wifi["minimumRssi"] = diagnostics.wifi.minimumRssi;
  } else {
    wifi["minimumRssi"] = nullptr;
  }

  JsonObject mqtt = document.createNestedObject("mqtt");
  mqtt["connectionAttempts"] = diagnostics.mqtt.connectionAttempts;
  mqtt["successfulConnections"] = diagnostics.mqtt.successfulConnections;
  mqtt["disconnects"] = diagnostics.mqtt.disconnects;
  mqtt["reconnects"] = diagnostics.mqtt.reconnects;
  mqtt["lastSuccessfulConnectionMs"] =
      diagnostics.mqtt.lastSuccessfulConnectionMs;
  mqtt["lastDisconnectedMs"] = diagnostics.mqtt.lastDisconnectedMs;
  mqtt["lastHaStateMs"] = diagnostics.mqtt.lastHaStateMs;
  mqtt["lastSnapshotCompleteMs"] =
      diagnostics.mqtt.lastSnapshotCompleteMs;
  mqtt["discoveryPublishCount"] = diagnostics.mqtt.discoveryPublishCount;
  mqtt["lastError"] = diagnostics.mqtt.lastError;
  mqtt["lastReturnCode"] = diagnostics.mqtt.lastReturnCode;

  JsonObject yoRadio = document.createNestedObject("yoRadio");
  yoRadio["connectionAttempts"] = diagnostics.yoRadio.connectionAttempts;
  yoRadio["successfulConnections"] =
      diagnostics.yoRadio.successfulConnections;
  yoRadio["disconnects"] = diagnostics.yoRadio.disconnects;
  yoRadio["reconnects"] = diagnostics.yoRadio.reconnects;
  yoRadio["firstDataCount"] = diagnostics.yoRadio.firstDataCount;
  yoRadio["lastDataMs"] = diagnostics.yoRadio.lastDataMs;
  if (diagnostics.yoRadio.lastDataMs > 0) {
    yoRadio["lastDataAgeMs"] =
        static_cast<uint32_t>(nowMs - diagnostics.yoRadio.lastDataMs);
  } else {
    yoRadio["lastDataAgeMs"] = nullptr;
  }
  yoRadio["lastSuccessfulConnectionMs"] =
      diagnostics.yoRadio.lastSuccessfulConnectionMs;
  yoRadio["currentBackoffMs"] = diagnostics.yoRadio.currentBackoffMs;
  yoRadio["lastDisconnectReason"] =
      yoRadioDisconnectReasonName(diagnostics.yoRadio.lastDisconnectReason);

  JsonObject nextion = document.createNestedObject("nextion");
  nextion["receivedFrames"] = diagnostics.nextion.receivedFrames;
  nextion["touchFrames"] = diagnostics.nextion.touchFrames;
  nextion["pageFrames"] = diagnostics.nextion.pageFrames;
  nextion["error1AFrames"] = diagnostics.nextion.error1AFrames;
  nextion["parserTimeouts"] = diagnostics.nextion.parserTimeouts;
  nextion["malformedFrames"] = diagnostics.nextion.malformedFrames;
  nextion["txQueued"] = diagnostics.nextion.txQueued;
  nextion["txSent"] = diagnostics.nextion.txSent;
  nextion["txQueueHighWater"] = diagnostics.nextion.txQueueHighWater;
  nextion["txDropped"] = diagnostics.nextion.txDropped;
  nextion["startupSyncRetries"] =
      diagnostics.nextion.startupSyncRetries;
  nextion["pageVerifyRetries"] = diagnostics.nextion.pageVerifyRetries;
  nextion["tftUpdateSuccesses"] =
      diagnostics.nextion.tftUpdateSuccesses;
  nextion["tftUpdateFailures"] = diagnostics.nextion.tftUpdateFailures;

  JsonObject loop = document.createNestedObject("loop");
  loop["iterations"] = diagnostics.loop.iterations;
  loop["maximumUs"] = diagnostics.loop.maximumDurationUs;
  loop["averageUs"] = diagnostics.loop.iterations > 0
                          ? diagnostics.loop.totalDurationUs /
                                diagnostics.loop.iterations
                          : 0;
  loop["rollingAverageUs"] = diagnostics.loop.rollingAverageUs;
  loop["over10Ms"] = diagnostics.loop.over10Ms;
  loop["over50Ms"] = diagnostics.loop.over50Ms;
  loop["over100Ms"] = diagnostics.loop.over100Ms;

  JsonObject http = document.createNestedObject("http");
  http["requests"] = diagnostics.http.requests;
  http["responses2xx"] = diagnostics.http.responses2xx;
  http["responses4xx"] = diagnostics.http.responses4xx;
  http["responses5xx"] = diagnostics.http.responses5xx;
  http["activeRequest"] = static_cast<bool>(client_);
  http["firmwareUpdateSuccesses"] =
      diagnostics.http.firmwareUpdateSuccesses;
  http["firmwareUpdateFailures"] =
      diagnostics.http.firmwareUpdateFailures;
  http["configSaveSuccesses"] = diagnostics.http.configSaveSuccesses;
  http["configSaveFailures"] = diagnostics.http.configSaveFailures;
  http["configImportSuccesses"] = diagnostics.http.configImportSuccesses;
  http["configImportFailures"] = diagnostics.http.configImportFailures;

  if (document.overflowed() ||
      !sendJsonDocument(client_, 200, "OK", document)) {
    sendJsonResponse(
        client_, 500, "Internal Server Error",
        "{\"ok\":false,\"error\":\"JSON serialization failed\"}");
  } else {
    recordHttpResponse(200);
  }
}

void NextionUpdater::beginJsonRequest(size_t contentLength,
                                      JsonRequest request) {
  jsonBody_ = new (std::nothrow) char[contentLength + 1U];
  if (jsonBody_ == nullptr) {
    if (request == JsonRequest::CONFIG_IMPORT) {
      ++state_.diagnostics.http.configImportFailures;
    } else {
      ++state_.diagnostics.http.configSaveFailures;
    }
    sendJsonResponse(
        client_, 500, "Internal Server Error",
        "{\"ok\":false,\"error\":\"JSON buffer allocation failed\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }
  jsonRequest_ = request;
  expectedJsonBodyLength_ = contentLength;
  jsonBodyLength_ = 0;
  jsonBody_[0] = '\0';
  lastHttpActivityMs_ = millis();
  phaseStartedMs_ = millis();
  phase_ = Phase::READ_JSON_BODY;
}

void NextionUpdater::applyJsonConfig() {
  DynamicJsonDocument request(Config::NextionUpdate::JSON_BODY_MAX_SIZE);
  if (request.capacity() < Config::NextionUpdate::JSON_BODY_MAX_SIZE) {
    ++state_.diagnostics.http.configSaveFailures;
    sendJsonResponse(
        client_, 500, "Internal Server Error",
        "{\"ok\":false,\"error\":\"JSON document allocation failed\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  const DeserializationError jsonError =
      deserializeJson(request, jsonBody_, jsonBodyLength_);
  if (jsonError || !request.is<JsonObject>()) {
    ++state_.diagnostics.http.configSaveFailures;
    logHttpStack("before-response");
    sendJsonResponse(
        client_, 400, "Bad Request",
        "{\"ok\":false,\"error\":\"Invalid JSON object\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }
  logHttpStack("json-parsed");

  RuntimeConfig::PublicValues values;
  config_.copyPublic(values);
  JsonObjectConst root = request.as<JsonObjectConst>();
  const char* error = nullptr;
  char wifiPassword[65]{};
  char mqttPassword[65]{};

  const bool merged =
      mergePublicConfig(root, values, error) &&
      extractSecret(root, "wifiPassword", wifiPassword, 63, error) &&
      extractSecret(root, "mqttPassword", mqttPassword, 64, error);

  if (!merged) {
    ++state_.diagnostics.http.configSaveFailures;
    StaticJsonDocument<192> response;
    response["ok"] = false;
    response["error"] = error;
    char body[192]{};
    serializeJson(response, body, sizeof(body));
    logHttpStack("before-response");
    sendJsonResponse(client_, 400, "Bad Request", body);
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  logHttpStack("before-apply-save");
  const RuntimeConfig::ApplyResult result =
      config_.applyChecked(values, wifiPassword, mqttPassword);
  logHttpStack("after-apply-save");
  logHttpStack("before-response");
  if (result == RuntimeConfig::ApplyResult::VALIDATION_ERROR) {
    ++state_.diagnostics.http.configSaveFailures;
    sendJsonResponse(
        client_, 400, "Bad Request",
        "{\"ok\":false,\"error\":\"Invalid configuration\"}");
  } else if (result == RuntimeConfig::ApplyResult::STORAGE_ERROR) {
    ++state_.diagnostics.http.configSaveFailures;
    sendJsonResponse(
        client_, 500, "Internal Server Error",
        "{\"ok\":false,\"error\":\"NVS save failed\"}");
  } else {
    ++state_.diagnostics.http.configSaveSuccesses;
    sendJsonResponse(
        client_, 200, "OK",
        "{\"ok\":true,\"restartRequired\":true}");
  }

  closeHttpClient();
  phase_ = Phase::IDLE;
}

void NextionUpdater::importJsonConfig() {
  DynamicJsonDocument request(Config::NextionUpdate::JSON_BODY_MAX_SIZE);
  const DeserializationError jsonError =
      deserializeJson(request, jsonBody_, jsonBodyLength_);
  if (jsonError || !request.is<JsonObject>()) {
    ++state_.diagnostics.http.configImportFailures;
    sendJsonResponse(
        client_, 400, "Bad Request",
        "{\"ok\":false,\"error\":\"Invalid JSON object\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  JsonObjectConst root = request.as<JsonObjectConst>();
  const char* format = root["format"] | "";
  if (strcmp(format, "kuchnia-panel-config") != 0) {
    ++state_.diagnostics.http.configImportFailures;
    sendJsonResponse(
        client_, 400, "Bad Request",
        "{\"ok\":false,\"error\":\"Invalid configuration format\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }
  if (!root["version"].is<uint32_t>() ||
      root["version"].as<uint32_t>() != 1U) {
    ++state_.diagnostics.http.configImportFailures;
    sendJsonResponse(
        client_, 400, "Bad Request",
        "{\"ok\":false,\"error\":\"Unsupported configuration version\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }
  if (!root["config"].is<JsonObjectConst>()) {
    ++state_.diagnostics.http.configImportFailures;
    sendJsonResponse(
        client_, 400, "Bad Request",
        "{\"ok\":false,\"error\":\"Missing configuration object\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  JsonObjectConst imported = root["config"].as<JsonObjectConst>();
  if (root.containsKey("wifiPassword") ||
      root.containsKey("mqttPassword") ||
      imported.containsKey("wifiPassword") ||
      imported.containsKey("mqttPassword")) {
    ++state_.diagnostics.http.configImportFailures;
    sendJsonResponse(
        client_, 400, "Bad Request",
        "{\"ok\":false,\"error\":\"Plaintext passwords are not accepted\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  RuntimeConfig::PublicValues values;
  config_.copyPublic(values);
  const char* error = nullptr;
  if (!mergePublicConfig(imported, values, error)) {
    ++state_.diagnostics.http.configImportFailures;
    sendJsonResponse(
        client_, 400, "Bad Request",
        "{\"ok\":false,\"error\":\"Invalid imported field\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  // Puste wejscia zachowuja sekrety znajdujace sie juz w RuntimeConfig.
  const RuntimeConfig::ApplyResult result =
      config_.applyChecked(values, "", "");
  if (result == RuntimeConfig::ApplyResult::VALIDATION_ERROR) {
    ++state_.diagnostics.http.configImportFailures;
    sendJsonResponse(
        client_, 400, "Bad Request",
        "{\"ok\":false,\"error\":\"Invalid configuration\"}");
  } else if (result == RuntimeConfig::ApplyResult::STORAGE_ERROR) {
    ++state_.diagnostics.http.configImportFailures;
    sendJsonResponse(
        client_, 500, "Internal Server Error",
        "{\"ok\":false,\"error\":\"NVS save failed\"}");
  } else {
    ++state_.diagnostics.http.configImportSuccesses;
    sendJsonResponse(
        client_, 200, "OK",
        "{\"ok\":true,\"restartRequired\":true}");
  }
  closeHttpClient();
  phase_ = Phase::IDLE;
}

void NextionUpdater::resetConfig() {
  if (config_.resetToDefaults()) {
    sendJsonResponse(
        client_, 200, "OK",
        "{\"ok\":true,\"restartRequired\":true}");
  } else {
    sendJsonResponse(
        client_, 500, "Internal Server Error",
        "{\"ok\":false,\"error\":\"NVS reset failed\"}");
  }
  closeHttpClient();
  phase_ = Phase::IDLE;
}

void NextionUpdater::scheduleRestart() {
  sendJsonResponse(
      client_, 200, "OK",
      "{\"ok\":true,\"message\":\"Restarting\"}");
  closeHttpClient();
  phase_ = Phase::IDLE;
  restartScheduledMs_ = millis();
  restartDelayMs_ = Config::NextionUpdate::API_RESTART_DELAY_MS;
  restartPending_ = true;
}

void NextionUpdater::startFirmwareUpload(const char* fileName,
                                         size_t fileSize) {
  const esp_partition_t* updatePartition =
      esp_ota_get_next_update_partition(nullptr);
  if (active_ || driver_.isUpdateMode()) {
    sendJsonResponse(
        client_, 409, "Conflict",
        "{\"ok\":false,\"error\":\"Trwa aktualizacja Nextiona\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }
  if (firmwareUpdateActive_ || Update.isRunning()) {
    sendJsonResponse(
        client_, 409, "Conflict",
        "{\"ok\":false,\"error\":\"Aktualizacja firmware jest juz aktywna\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }
  if (updatePartition == nullptr || fileSize > updatePartition->size) {
    sendJsonResponse(
        client_, 413, "Payload Too Large",
        "{\"ok\":false,\"error\":\"Firmware nie miesci sie w slocie OTA\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  Update.clearError();
  if (!Update.begin(fileSize, U_FLASH)) {
    char message[160]{};
    snprintf(message, sizeof(message),
             "{\"ok\":false,\"error\":\"Nie mozna rozpoczac OTA: %s\"}",
             Update.errorString());
    sendJsonResponse(client_, 500, "Internal Server Error", message);
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  strlcpy(fileName_, fileName, sizeof(fileName_));
  totalBytes_ = fileSize;
  receivedBytes_ = 0;
  firmwareUpdateProgress_ = 0;
  firmwareUpdateActive_ = true;
  lastHttpActivityMs_ = millis();
  phaseStartedMs_ = millis();
  phase_ = Phase::RECEIVE_FIRMWARE;
  Serial.printf(
      "FIRMWARE OTA START: file=%s size=%u partition=%s offset=0x%06lx "
      "capacity=%u\n",
      fileName_, static_cast<unsigned>(fileSize), updatePartition->label,
      static_cast<unsigned long>(updatePartition->address),
      static_cast<unsigned>(updatePartition->size));
}

void NextionUpdater::receiveFirmware() {
  if (receivedBytes_ < totalBytes_ && client_.available() > 0) {
    const size_t remaining = totalBytes_ - receivedBytes_;
    const size_t available = static_cast<size_t>(client_.available());
    const size_t requested =
        min(remaining,
            min(available, Config::NextionUpdate::HTTP_READ_BYTES_PER_LOOP));
    const int readLength = client_.read(chunk_, requested);
    if (readLength > 0) {
      const size_t bytesRead = static_cast<size_t>(readLength);
      const size_t written = Update.write(chunk_, bytesRead);
      if (written != bytesRead) {
        failFirmwareUpload(500, "Internal Server Error",
                           "Blad zapisu firmware do Flash");
        return;
      }
      receivedBytes_ += bytesRead;
      firmwareUpdateProgress_ = static_cast<uint8_t>(
          (static_cast<uint64_t>(receivedBytes_) * 100U) / totalBytes_);
      lastHttpActivityMs_ = millis();
      yield();
    }
  }

  if (receivedBytes_ == totalBytes_) {
    if (!Update.end(true) || Update.hasError()) {
      failFirmwareUpload(500, "Internal Server Error",
                         "Finalizacja firmware nie powiodla sie");
      return;
    }

    firmwareUpdateProgress_ = 100;
    ++state_.diagnostics.http.firmwareUpdateSuccesses;
    sendJsonResponse(
        client_, 200, "OK",
        "{\"ok\":true,\"message\":\"Firmware zapisany. Restart urzadzenia...\"}");
    closeHttpClient();
    phase_ = Phase::IDLE;
    restartScheduledMs_ = millis();
    restartDelayMs_ = Config::NextionUpdate::FIRMWARE_RESTART_DELAY_MS;
    restartPending_ = true;
    Serial.printf("FIRMWARE OTA COMPLETE: %u bytes, restart za %lu ms\n",
                  static_cast<unsigned>(receivedBytes_),
                  static_cast<unsigned long>(restartDelayMs_));
    return;
  }

  if (!client_.connected() && client_.available() == 0) {
    failFirmwareUpload(400, "Bad Request",
                       "Upload firmware zostal przerwany");
  } else if (elapsed(lastHttpActivityMs_,
                     Config::NextionUpdate::HTTP_BODY_TIMEOUT_MS)) {
    failFirmwareUpload(408, "Request Timeout",
                       "Timeout podczas odbioru firmware");
  }
}

void NextionUpdater::failFirmwareUpload(int status, const char* reason,
                                         const char* message) {
  ++state_.diagnostics.http.firmwareUpdateFailures;
  const char* updateError = Update.hasError() ? Update.errorString() : nullptr;
  char response[224]{};
  snprintf(response, sizeof(response),
           "{\"ok\":false,\"error\":\"%s%s%s\"}", message,
           updateError == nullptr ? "" : ": ",
           updateError == nullptr ? "" : updateError);
  Serial.printf("FIRMWARE OTA FAILED: %s received=%u/%u%s%s\n", message,
                static_cast<unsigned>(receivedBytes_),
                static_cast<unsigned>(totalBytes_),
                updateError == nullptr ? "" : " update=",
                updateError == nullptr ? "" : updateError);
  if (Update.isRunning()) Update.abort();
  if (client_) sendJsonResponse(client_, status, reason, response);
  closeHttpClient();
  firmwareUpdateActive_ = false;
  firmwareUpdateProgress_ = 0;
  phase_ = Phase::IDLE;
}

void NextionUpdater::serveUploadPage() {
  char responseHeader[192]{};
  const size_t pageLength = strlen_P(UPLOAD_PAGE);
  const int length = snprintf(
      responseHeader, sizeof(responseHeader),
      "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
      "Content-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",
      static_cast<unsigned>(pageLength));
  if (length > 0 && static_cast<size_t>(length) < sizeof(responseHeader)) {
    client_.write(reinterpret_cast<const uint8_t*>(responseHeader), length);
    client_.write_P(UPLOAD_PAGE, pageLength);
    recordHttpResponse(200);
  }
}

void NextionUpdater::startUpload(const char* fileName, size_t fileSize) {
  if (driver_.isUpdateMode()) {
    sendTextResponse(client_, 409, "Conflict",
                     "UART Nextiona pozostaje w trybie aktualizacji.");
    closeHttpClient();
    phase_ = Phase::IDLE;
    return;
  }

  active_ = true;
  tftFailureRecorded_ = false;
  recoveryAfterSuccessfulUpload_ = false;
  resultScreenQueued_ = false;
  strlcpy(fileName_, fileName, sizeof(fileName_));
  totalBytes_ = fileSize;
  receivedBytes_ = 0;
  sentBytes_ = 0;
  acknowledgedBytes_ = 0;
  chunkLength_ = 0;
  pendingBlockLength_ = 0;
  nextProgressPercent_ = 10;
  resetResponseBuffer();
  lastHttpActivityMs_ = millis();
  phase_ = Phase::PREPARE_UPDATE_SCREEN;

  Serial.println("NEXTION UPDATE START");
  Serial.printf("NEXTION UPDATE file=%s size=%u normal=%lu upload=%lu\n",
                fileName_, static_cast<unsigned>(totalBytes_),
                static_cast<unsigned long>(Config::Nextion::BAUD_RATE),
                static_cast<unsigned long>(Config::NextionUpdate::UPLOAD_BAUD));
}

void NextionUpdater::prepareUpdateScreen() {
  if (!resultScreenQueued_) {
    if (!ui_.showUpdateScreen(UpdateType::NEXTION)) {
      return;
    }
    resultScreenQueued_ = true;
  }

  if (!driver_.commandQueueIdle()) {
    return;
  }

  // Kolejka jest pusta, a flush potwierdza fizyczne wysłanie ostatnich bajtów.
  driver_.flushNormalTx();
  driver_.clearUpdateRx();
  phase_ = Phase::DRAIN_UPDATE_SCREEN_RX;
  phaseStartedMs_ = millis();
}

void NextionUpdater::drainUpdateScreenRx() {
  // Usuwamy odpowiedzi zwykłego protokołu dopiero po fizycznym wysłaniu UI.
  driver_.clearUpdateRx();
  if (!elapsed(phaseStartedMs_, Config::NextionUpdate::UI_RX_DRAIN_MS)) {
    return;
  }

  // Ostatni drain odbywa się przed pustą komendą i connect.
  driver_.clearUpdateRx();
  beginUploadProtocol();
}

void NextionUpdater::beginUploadProtocol() {
  if (!driver_.enterUpdateMode()) {
    failBeforeUpdateMode("UART Nextiona jest zajęty.");
    return;
  }

  driver_.setUpdateBaud(Config::Nextion::BAUD_RATE);
  driver_.clearUpdateRx();
  driver_.writeUpdateCommand("");
  driver_.writeUpdateCommand("connect");
  driver_.flushUpdateTx();
  phase_ = Phase::WAIT_CONNECT_RESPONSE;
  phaseStartedMs_ = millis();
  lastHttpActivityMs_ = millis();
}

void NextionUpdater::waitForConnectResponse() {
  bool responseComplete = false;
  if (readCompleteConnectResponse(responseComplete)) {
    Serial.printf("NEXTION UPDATE: handshake model=%s flash=%u bytes\n",
                  detectedModel_, static_cast<unsigned>(detectedFlashBytes_));
    if (totalBytes_ > detectedFlashBytes_) {
      failUpload("Plik TFT przekracza pamięć Flash zgłoszoną przez Nextiona.");
      return;
    }
    sendUploadCommand();
  } else if (responseComplete) {
    failUpload("Niepoprawna pełna odpowiedź na connect.");
  } else if (requestTimedOut(Config::NextionUpdate::NEXTION_CONNECT_TIMEOUT_MS)) {
    char reason[112]{};
    snprintf(reason, sizeof(reason),
             "Nextion nie odpowiedział na connect przy %lu baud.",
             static_cast<unsigned long>(Config::Nextion::BAUD_RATE));
    failUpload(reason);
  }
}

void NextionUpdater::sendUploadCommand() {
  char command[64]{};
  const int length = snprintf(
      command, sizeof(command), "whmi-wri %u,%lu,0",
      static_cast<unsigned>(totalBytes_),
      static_cast<unsigned long>(Config::NextionUpdate::UPLOAD_BAUD));
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(command)) {
    failUpload("Nie udało się przygotować komendy whmi-wri.");
    return;
  }

  driver_.clearUpdateRx();
  if (!driver_.writeUpdateCommand(command)) {
    failUpload("Nie udało się wysłać komendy whmi-wri.");
    return;
  }
  driver_.flushUpdateTx();
  Serial.printf("NEXTION UPDATE ENTER DOWNLOAD MODE baud=%lu\n",
                static_cast<unsigned long>(Config::NextionUpdate::UPLOAD_BAUD));
  phase_ = Phase::WAIT_BAUD_SWITCH;
  phaseStartedMs_ = millis();
}

void NextionUpdater::waitForInitialAck() {
  if (readAck()) {
    Serial.println("NEXTION UPDATE ACK");
    phase_ = Phase::RECEIVE_BLOCK;
    phaseStartedMs_ = millis();
    lastHttpActivityMs_ = millis();
  } else if (requestTimedOut(Config::NextionUpdate::ACK_TIMEOUT_MS)) {
    failUpload("Brak początkowego ACK 0x05 po whmi-wri.");
  }
}

void NextionUpdater::receiveBlock() {
  if (!client_.connected() && client_.available() == 0) {
    failUpload("Połączenie HTTP zostało przerwane przed końcem pliku.");
    return;
  }

  const size_t remainingFile = totalBytes_ - receivedBytes_;
  const size_t remainingChunk = Config::NextionUpdate::CHUNK_SIZE - chunkLength_;
  const int available = client_.available();
  if (available > 0 && remainingFile > 0 && remainingChunk > 0) {
    size_t toRead = static_cast<size_t>(available);
    if (toRead > remainingFile) toRead = remainingFile;
    if (toRead > remainingChunk) toRead = remainingChunk;
    if (toRead > Config::NextionUpdate::HTTP_READ_BYTES_PER_LOOP) {
      toRead = Config::NextionUpdate::HTTP_READ_BYTES_PER_LOOP;
    }

    const int read = client_.read(chunk_ + chunkLength_, toRead);
    if (read > 0) {
      chunkLength_ += static_cast<size_t>(read);
      receivedBytes_ += static_cast<size_t>(read);
      lastHttpActivityMs_ = millis();
    }
  }

  if (chunkLength_ == Config::NextionUpdate::CHUNK_SIZE ||
      (receivedBytes_ == totalBytes_ && chunkLength_ > 0)) {
    sendBufferedBlock();
  } else if (elapsed(lastHttpActivityMs_, Config::NextionUpdate::HTTP_BODY_TIMEOUT_MS)) {
    failUpload("Timeout podczas odbioru danych pliku przez HTTP.");
  }
}

void NextionUpdater::sendBufferedBlock() {
  const size_t written = driver_.writeUpdateData(chunk_, chunkLength_);
  if (written != chunkLength_) {
    failUpload("Niepełny zapis bloku do UART Nextiona.");
    return;
  }

  driver_.flushUpdateTx();
  sentBytes_ += written;
  pendingBlockLength_ = chunkLength_;
  chunkLength_ = 0;
  phase_ = Phase::WAIT_BLOCK_ACK;
  phaseStartedMs_ = millis();
}

void NextionUpdater::waitForBlockAck() {
  if (readAck()) {
    acknowledgedBytes_ += pendingBlockLength_;
    pendingBlockLength_ = 0;
    logProgress();

    if (acknowledgedBytes_ == totalBytes_) {
      phase_ = Phase::WAIT_NEXTION_REBOOT;
      phaseStartedMs_ = millis();
      strlcpy(lastResult_, "Dane TFT przesłane; trwa ponowna synchronizacja.",
              sizeof(lastResult_));
    } else {
      phase_ = Phase::RECEIVE_BLOCK;
      phaseStartedMs_ = millis();
      lastHttpActivityMs_ = millis();
    }
  } else if (requestTimedOut(Config::NextionUpdate::ACK_TIMEOUT_MS)) {
    failUpload("Brak ACK 0x05 po bloku danych TFT.");
  }
}

void NextionUpdater::beginNormalRecovery(bool afterSuccessfulUpload) {
  recoveryAfterSuccessfulUpload_ = afterSuccessfulUpload;
  driver_.clearUpdateRx();
  driver_.setUpdateBaud(Config::Nextion::BAUD_RATE);
  phase_ = Phase::WAIT_NORMAL_BAUD_SETTLE;
  phaseStartedMs_ = millis();
}

void NextionUpdater::waitForNormalBaudSettle() {
  if (readReady()) {
    Serial.println("NEXTION UPDATE: odebrano Ready 0x88 po restarcie HMI");
    completeNormalRecovery();
    return;
  }

  if (!elapsed(phaseStartedMs_, Config::NextionUpdate::NORMAL_BAUD_SETTLE_MS)) {
    return;
  }

  resetResponseBuffer();
  driver_.clearUpdateRx();
  driver_.writeUpdateCommand("");
  driver_.writeUpdateCommand("connect");
  driver_.flushUpdateTx();
  phase_ = Phase::WAIT_RECOVERY_RESPONSE;
  phaseStartedMs_ = millis();
}

void NextionUpdater::waitForRecoveryResponse() {
  if (readResponseToken("comok")) {
    completeNormalRecovery();
  } else if (requestTimedOut(Config::NextionUpdate::RECOVERY_TIMEOUT_MS)) {
    completeFailedRecovery();
  }
}

void NextionUpdater::completeNormalRecovery() {
  if (recoveryAfterSuccessfulUpload_) {
    completeSuccessfulUpload();
    return;
  }

  driver_.leaveUpdateMode();
  resultScreenQueued_ = false;
  phase_ = Phase::PREPARE_ERROR_SCREEN;
  Serial.printf("NEXTION UPDATE: po błędzie przywrócono UART %lu\n",
                static_cast<unsigned long>(Config::Nextion::BAUD_RATE));
}

void NextionUpdater::completeSuccessfulUpload() {
  ++state_.diagnostics.nextion.tftUpdateSuccesses;
  driver_.leaveUpdateMode();
  resultScreenQueued_ = false;
  phase_ = Phase::PREPARE_SUCCESS_SCREEN;
  snprintf(lastResult_, sizeof(lastResult_),
           "Aktualizacja HMI zakończona. Przywrócono UART %lu.",
           static_cast<unsigned long>(Config::Nextion::BAUD_RATE));
  Serial.printf("NEXTION UPDATE: sukces, potwierdzono %u/%u bytes\n",
                static_cast<unsigned>(acknowledgedBytes_),
                static_cast<unsigned>(totalBytes_));
  Serial.println("NEXTION UPDATE COMPLETE");
}

void NextionUpdater::failUpload(const char* reason) {
  recordTftFailure();
  snprintf(lastResult_, sizeof(lastResult_), "Błąd aktualizacji HMI: %s", reason);
  Serial.printf("NEXTION UPDATE FAILED: %s received=%u sent=%u acknowledged=%u expected=%u\n",
                reason, static_cast<unsigned>(receivedBytes_),
                static_cast<unsigned>(sentBytes_),
                static_cast<unsigned>(acknowledgedBytes_),
                static_cast<unsigned>(totalBytes_));
  if (client_) {
    sendTextResponse(client_, 500, "Internal Server Error", lastResult_);
    closeHttpClient();
  }
  beginNormalRecovery(false);
}

void NextionUpdater::failBeforeUpdateMode(const char* reason) {
  recordTftFailure();
  snprintf(lastResult_, sizeof(lastResult_), "Błąd aktualizacji HMI: %s", reason);
  Serial.printf("NEXTION UPDATE FAILED BEFORE DOWNLOAD MODE: %s\n", reason);
  if (client_) {
    sendTextResponse(client_, 500, "Internal Server Error", lastResult_);
    closeHttpClient();
  }
  resultScreenQueued_ = false;
  phase_ = Phase::PREPARE_ERROR_SCREEN;
}

void NextionUpdater::completeFailedRecovery() {
  active_ = false;
  phase_ = Phase::IDLE;

  if (recoveryAfterSuccessfulUpload_) {
    recordTftFailure();
    snprintf(lastResult_, sizeof(lastResult_),
             "TFT przesłano, ale nie potwierdzono powrotu Nextiona do %lu baud. "
             "Odłącz zasilanie całego zestawu i sprawdź panel.",
             static_cast<unsigned long>(Config::Nextion::BAUD_RATE));
    if (client_) {
      sendTextResponse(client_, 500, "Internal Server Error", lastResult_);
      closeHttpClient();
    }
  }

  // Sterownik pozostaje w trybie wyłącznym: stan UART nie został potwierdzony.
  Serial.printf("NEXTION UPDATE ERROR: %s Normalna obsługa HMI pozostaje wstrzymana.\n",
                lastResult_);
}

void NextionUpdater::prepareSuccessScreen() {
  if (!resultScreenQueued_) {
    if (!ui_.showUpdateSuccess()) {
      return;
    }
    resultScreenQueued_ = true;
  }

  if (!driver_.commandQueueIdle()) {
    return;
  }

  driver_.flushNormalTx();
  if (client_) {
    sendTextResponse(client_, 200, "OK", lastResult_);
    closeHttpClient();
  }
  phase_ = Phase::WAIT_ESP_RESTART;
  phaseStartedMs_ = millis();
}

void NextionUpdater::prepareErrorScreen() {
  if (!resultScreenQueued_) {
    if (!ui_.showUpdateError("Błąd komunikacji")) {
      return;
    }
    resultScreenQueued_ = true;
  }

  if (!driver_.commandQueueIdle()) {
    return;
  }

  driver_.flushNormalTx();
  phase_ = Phase::WAIT_ERROR_SCREEN_STABLE;
}

void NextionUpdater::logProgress() {
  const uint32_t percent = totalBytes_ == 0
                               ? 0
                               : static_cast<uint32_t>(acknowledgedBytes_) * 100U /
                                     static_cast<uint32_t>(totalBytes_);
  while (nextProgressPercent_ <= 100 && percent >= nextProgressPercent_) {
    Serial.printf("NEXTION UPDATE: %u%% (%u/%u bytes potwierdzone)\n",
                  nextProgressPercent_, static_cast<unsigned>(acknowledgedBytes_),
                  static_cast<unsigned>(totalBytes_));
    nextProgressPercent_ = static_cast<uint8_t>(nextProgressPercent_ + 10);
  }
}

bool NextionUpdater::readAck() {
  size_t processed = 0;
  int value = -1;
  while (processed < Config::Nextion::RX_BYTES_PER_LOOP &&
         (value = driver_.readUpdateByte()) >= 0) {
    ++processed;
    if (static_cast<uint8_t>(value) == 0x05U) return true;
  }
  return false;
}

bool NextionUpdater::readReady() {
  size_t processed = 0;
  int value = -1;
  while (processed < Config::Nextion::RX_BYTES_PER_LOOP &&
         (value = driver_.readUpdateByte()) >= 0) {
    ++processed;
    if (static_cast<uint8_t>(value) == 0x88U) return true;
  }
  return false;
}

bool NextionUpdater::readCompleteConnectResponse(bool& complete) {
  complete = false;
  size_t processed = 0;
  int value = -1;
  while (processed < Config::Nextion::RX_BYTES_PER_LOOP &&
         (value = driver_.readUpdateByte()) >= 0) {
    ++processed;
    const uint8_t byteValue = static_cast<uint8_t>(value);

    if (byteValue == 0xFFU) {
      ++responseTerminatorCount_;
      if (responseTerminatorCount_ < 3) continue;

      complete = true;
      responseBuffer_[responseLength_] = '\0';
      const char* comok = strstr(responseBuffer_, "comok");
      if (comok == nullptr) return false;

      const char* firstComma = strchr(comok, ',');
      const char* secondComma = firstComma == nullptr ? nullptr : strchr(firstComma + 1, ',');
      const char* thirdComma = secondComma == nullptr ? nullptr : strchr(secondComma + 1, ',');
      const char* lastComma = strrchr(comok, ',');
      if (secondComma == nullptr || thirdComma == nullptr || lastComma == nullptr) {
        return false;
      }

      const size_t modelLength = static_cast<size_t>(thirdComma - secondComma - 1);
      if (modelLength == 0 || modelLength >= sizeof(detectedModel_)) return false;
      memcpy(detectedModel_, secondComma + 1, modelLength);
      detectedModel_[modelLength] = '\0';

      size_t flashSize = 0;
      if (!parsePositiveSize(lastComma + 1, flashSize)) return false;
      detectedFlashBytes_ = flashSize;
      return true;
    }

    responseTerminatorCount_ = 0;
    if (byteValue < 0x20U || byteValue > 0x7EU) continue;
    if (responseLength_ + 1 >= sizeof(responseBuffer_)) return false;
    responseBuffer_[responseLength_++] = static_cast<char>(byteValue);
    responseBuffer_[responseLength_] = '\0';
  }
  return false;
}

bool NextionUpdater::readResponseToken(const char* token) {
  size_t processed = 0;
  int value = -1;
  while (processed < Config::Nextion::RX_BYTES_PER_LOOP &&
         (value = driver_.readUpdateByte()) >= 0) {
    ++processed;
    const uint8_t byteValue = static_cast<uint8_t>(value);
    if (byteValue < 0x20U || byteValue > 0x7EU) continue;

    if (responseLength_ + 1 >= sizeof(responseBuffer_)) {
      const size_t retained = sizeof(responseBuffer_) / 2;
      memmove(responseBuffer_, responseBuffer_ + responseLength_ - retained,
              retained);
      responseLength_ = retained;
    }
    responseBuffer_[responseLength_++] = static_cast<char>(byteValue);
    responseBuffer_[responseLength_] = '\0';
    if (strstr(responseBuffer_, token) != nullptr) return true;
  }
  return false;
}

bool NextionUpdater::requestTimedOut(uint32_t timeoutMs) const {
  return elapsed(phaseStartedMs_, timeoutMs);
}

void NextionUpdater::resetResponseBuffer() {
  responseLength_ = 0;
  responseTerminatorCount_ = 0;
  responseBuffer_[0] = '\0';
}

void NextionUpdater::recordHttpResponse(int status) {
  if (status >= 200 && status < 300) {
    ++state_.diagnostics.http.responses2xx;
  } else if (status >= 400 && status < 500) {
    ++state_.diagnostics.http.responses4xx;
  } else if (status >= 500 && status < 600) {
    ++state_.diagnostics.http.responses5xx;
  }
}

void NextionUpdater::recordTftFailure() {
  if (tftFailureRecorded_) return;
  tftFailureRecorded_ = true;
  ++state_.diagnostics.nextion.tftUpdateFailures;
}

void NextionUpdater::closeHttpClient() {
  if (client_) client_.stop();
  client_ = NetworkClient();
  releaseJsonBody();
  headerLength_ = 0;
  header_[0] = '\0';
}

void NextionUpdater::releaseJsonBody() {
  delete[] jsonBody_;
  jsonBody_ = nullptr;
  expectedJsonBodyLength_ = 0;
  jsonBodyLength_ = 0;
}

void NextionUpdater::sendTextResponse(NetworkClient& client, int status,
                                       const char* reason, const char* body) {
  recordHttpResponse(status);
  char responseHeader[224]{};
  const size_t bodyLength = strlen(body);
  const int length = snprintf(
      responseHeader, sizeof(responseHeader),
      "HTTP/1.1 %d %s\r\nContent-Type: text/plain; charset=utf-8\r\n"
      "Content-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",
      status, reason, static_cast<unsigned>(bodyLength));
  if (length > 0 && static_cast<size_t>(length) < sizeof(responseHeader)) {
    client.write(reinterpret_cast<const uint8_t*>(responseHeader), length);
    client.write(reinterpret_cast<const uint8_t*>(body), bodyLength);
  }
}

void NextionUpdater::sendJsonResponse(NetworkClient& client, int status,
                                       const char* reason, const char* body) {
  recordHttpResponse(status);
  char responseHeader[224]{};
  const size_t bodyLength = strlen(body);
  const int length = snprintf(
      responseHeader, sizeof(responseHeader),
      "HTTP/1.1 %d %s\r\nContent-Type: application/json; charset=utf-8\r\n"
      "Content-Length: %u\r\nConnection: close\r\n"
      "Cache-Control: no-store\r\n\r\n",
      status, reason, static_cast<unsigned>(bodyLength));
  if (length > 0 && static_cast<size_t>(length) < sizeof(responseHeader)) {
    client.write(reinterpret_cast<const uint8_t*>(responseHeader), length);
    client.write(reinterpret_cast<const uint8_t*>(body), bodyLength);
  }
}

void NextionUpdater::sendBusyResponse(NetworkClient& client) {
  sendTextResponse(client, 503, "Service Unavailable",
                   "BUSY: trwa aktualizacja urzadzenia.");
}
