#include "WiFiManager.h"

#include <WiFi.h>

#include "AppState.h"
#include "RuntimeConfig.h"
#include "config.h"

namespace {

bool parseRequiredAddress(const char* text, IPAddress& address) {
  return text != nullptr && text[0] != '\0' && address.fromString(text);
}

bool parseOptionalAddress(const char* text, IPAddress& address) {
  if (text == nullptr || text[0] == '\0') {
    address = IPAddress();
    return true;
  }
  return address.fromString(text);
}

}  // namespace

WiFiManager::WiFiManager(AppState& state, const RuntimeConfig& config)
    : state_(state), config_(config) {}

void WiFiManager::begin() {
  Serial.println("WIFI START");

  state_.network.connected = false;
  state_.network.connecting = false;
  state_.network.ipAddress = "";
  state_.network.rssi = 0;
  currentBackoffMs_ = Config::Wifi::RECONNECT_MIN_MS;

  if (config_.wifiSsid()[0] == '\0') {
    Serial.println("WIFI CONFIG ERROR: WIFI_SSID jest pusty");
    phase_ = Phase::IDLE;
    return;
  }

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config_.hostname());

  configurationAvailable_ = configureNetwork();
  if (!configurationAvailable_) {
    Serial.println("WIFI CONFIG ERROR: niepoprawna konfiguracja adresów IP");
    phase_ = Phase::IDLE;
    return;
  }

  startConnection();
}

void WiFiManager::loop() {
  if (!configurationAvailable_) {
    return;
  }

  const uint32_t now = millis();
  const wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED && phase_ != Phase::CONNECTED) {
    handleConnected();
    return;
  }

  switch (phase_) {
    case Phase::CONNECTING:
      if (static_cast<uint32_t>(now - connectStartedMs_) >=
          Config::Wifi::CONNECT_TIMEOUT_MS) {
        handleDisconnected();
      }
      break;

    case Phase::CONNECTED:
      if (status != WL_CONNECTED) {
        handleDisconnected();
      } else if (static_cast<uint32_t>(now - lastStateRefreshMs_) >=
                 Config::Wifi::STATE_REFRESH_INTERVAL_MS) {
        refreshConnectionData();
      }
      break;

    case Phase::WAIT_RETRY:
      if (static_cast<uint32_t>(now - retryStartedMs_) >= retryDelayMs_) {
        startConnection();
      }
      break;

    case Phase::IDLE:
    default:
      break;
  }
}

bool WiFiManager::configureNetwork() {
  if (config_.wifiDhcp()) {
    return true;
  }

  IPAddress staticIp;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;

  if (!parseRequiredAddress(config_.wifiStaticIp(), staticIp) ||
      !parseRequiredAddress(config_.wifiGateway(), gateway) ||
      !parseRequiredAddress(config_.wifiSubnet(), subnet) ||
      !parseOptionalAddress(config_.wifiDns1(), dns1) ||
      !parseOptionalAddress(config_.wifiDns2(), dns2)) {
    return false;
  }

  return WiFi.config(staticIp, gateway, subnet, dns1, dns2);
}

void WiFiManager::startConnection() {
  ++state_.diagnostics.wifi.connectionAttempts;
  Serial.println("WIFI CONNECTING");
  state_.network.connecting = true;
  state_.network.connected = false;
  phase_ = Phase::CONNECTING;
  connectStartedMs_ = millis();
  WiFi.begin(config_.wifiSsid(), config_.wifiPassword());
}

void WiFiManager::handleConnected() {
  const bool reconnect = state_.diagnostics.wifi.successfulConnections > 0;
  phase_ = Phase::CONNECTED;
  state_.network.connected = true;
  state_.network.connecting = false;
  ++state_.network.sessionGeneration;
  state_.network.sessionStartedMs = millis();
  ++state_.diagnostics.wifi.successfulConnections;
  if (reconnect) {
    ++state_.diagnostics.wifi.reconnects;
  }
  state_.diagnostics.wifi.lastConnectedMs = state_.network.sessionStartedMs;
  currentBackoffMs_ = Config::Wifi::RECONNECT_MIN_MS;
  refreshConnectionData();

  const String gateway = WiFi.gatewayIP().toString();
  const String dns = WiFi.dnsIP(0).toString();
  const String mac = WiFi.macAddress();

  Serial.println("WIFI CONNECTED");
  Serial.printf("WIFI SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("WIFI IP: %s\n", state_.network.ipAddress.c_str());
  Serial.printf("WIFI gateway: %s\n", gateway.c_str());
  Serial.printf("WIFI DNS: %s\n", dns.c_str());
  Serial.printf("WIFI RSSI: %ld dBm\n",
                static_cast<long>(state_.network.rssi));
  Serial.printf("WIFI MAC: %s\n", mac.c_str());
  Serial.printf("WIFI hostname: %s\n", config_.hostname());
  Serial.printf("WIFI session: %lu\n",
                static_cast<unsigned long>(state_.network.sessionGeneration));
}

void WiFiManager::handleDisconnected() {
  const bool wasConnected =
      phase_ == Phase::CONNECTED || state_.network.connected;
  if (wasConnected) {
    Serial.println("WIFI DISCONNECTED");
    ++state_.diagnostics.wifi.disconnects;
  } else {
    Serial.println("WIFI DISCONNECTED: connect timeout");
    ++state_.diagnostics.wifi.connectTimeouts;
  }

  state_.network.connected = false;
  state_.network.connecting = false;
  state_.network.ipAddress = "";
  state_.network.rssi = 0;
  state_.network.lastDisconnectedMs = millis();
  state_.diagnostics.wifi.lastDisconnectedMs =
      state_.network.lastDisconnectedMs;
  WiFi.disconnect(false, false);
  scheduleRetry();
}

void WiFiManager::scheduleRetry() {
  phase_ = Phase::WAIT_RETRY;
  retryStartedMs_ = millis();
  retryDelayMs_ = currentBackoffMs_;
  Serial.printf("WIFI RETRY in %lu ms\n",
                static_cast<unsigned long>(retryDelayMs_));
  advanceBackoff();
}

void WiFiManager::refreshConnectionData() {
  state_.network.ipAddress = WiFi.localIP().toString();
  state_.network.rssi = WiFi.RSSI();
  WifiDiagnostics& diagnostics = state_.diagnostics.wifi;
  if (!diagnostics.minimumRssiValid ||
      state_.network.rssi < diagnostics.minimumRssi) {
    diagnostics.minimumRssi = state_.network.rssi;
    diagnostics.minimumRssiValid = true;
  }
  lastStateRefreshMs_ = millis();
}

void WiFiManager::advanceBackoff() {
  if (currentBackoffMs_ >= Config::Wifi::RECONNECT_MAX_MS / 2U) {
    currentBackoffMs_ = Config::Wifi::RECONNECT_MAX_MS;
    return;
  }

  currentBackoffMs_ *= 2U;
  if (currentBackoffMs_ > Config::Wifi::RECONNECT_MAX_MS) {
    currentBackoffMs_ = Config::Wifi::RECONNECT_MAX_MS;
  }
}
