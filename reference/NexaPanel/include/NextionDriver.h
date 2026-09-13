#pragma once

#include <Arduino.h>

#include "config.h"

struct AppState;

enum class NextionTouchAction : uint8_t {
  RELEASE = 0,
  PRESS = 1,
};

struct NextionTouchEvent {
  uint8_t pageId;
  uint8_t componentId;
  NextionTouchAction action;
};

class NextionDriver {
 public:
  using TouchCallback = void (*)(void* context, const NextionTouchEvent& event);
  using PageCallback = void (*)(void* context, uint8_t pageId);
  using ReadyCallback = void (*)(void* context);
  using ErrorCallback = void (*)(void* context, uint8_t errorCode);

  explicit NextionDriver(AppState& state);

  void begin();
  void loop();

  bool sendCommand(const char* command);
  bool setText(const char* component, const char* text);
  bool setTextUtf8(const char* component, const char* utf8Text);
  bool setPicture(const char* component, uint16_t pictureId);
  bool setColor(const char* component, uint16_t color);
  bool changePage(uint8_t pageId);
  bool requestCurrentPage();
  bool canQueueCommands(size_t count) const;
  bool commandQueueIdle() const;
  void clearNormalTxQueue();
  void flushNormalTx();

  // Wyłączny dostęp do UART używany tylko podczas aktualizacji pliku TFT.
  bool enterUpdateMode();
  void leaveUpdateMode();
  bool isUpdateMode() const;
  void setUpdateBaud(uint32_t baud);
  bool writeUpdateCommand(const char* command);
  size_t writeUpdateData(const uint8_t* data, size_t length);
  int readUpdateByte();
  void flushUpdateTx();
  void clearUpdateRx();

  void setTouchCallback(TouchCallback callback, void* context = nullptr);
  void setPageCallback(PageCallback callback, void* context = nullptr);
  void setReadyCallback(ReadyCallback callback, void* context = nullptr);
  void setErrorCallback(ErrorCallback callback, void* context = nullptr);

 private:
  struct TxCommand {
    char text[Config::Nextion::MAX_COMMAND_LENGTH];
  };

  void processRx();
  void processRxByte(uint8_t byteValue);
  void appendRxByte(uint8_t byteValue);
  void processFrame();
  void resetRxFrame();
  void processTx();
  void logRxFrame() const;

  AppState& state_;
  HardwareSerial serial_;

  TxCommand txQueue_[Config::Nextion::COMMAND_QUEUE_SIZE]{};
  size_t txHead_{0};
  size_t txTail_{0};
  size_t txCount_{0};
  uint32_t lastTxMs_{0};

  uint8_t rxBuffer_[Config::Nextion::RX_FRAME_BUFFER_SIZE]{};
  size_t rxLength_{0};
  uint8_t rxTerminatorCount_{0};
  bool rxDiscarding_{false};
  uint32_t lastRxByteMs_{0};

  TouchCallback touchCallback_{nullptr};
  PageCallback pageCallback_{nullptr};
  ReadyCallback readyCallback_{nullptr};
  ErrorCallback errorCallback_{nullptr};
  void* touchContext_{nullptr};
  void* pageContext_{nullptr};
  void* readyContext_{nullptr};
  void* errorContext_{nullptr};
  bool updateMode_{false};
};
