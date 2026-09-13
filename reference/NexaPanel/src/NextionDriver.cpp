#include "NextionDriver.h"

#include <cstdio>
#include <cstring>

#include "AppState.h"

namespace {
constexpr uint8_t FRAME_TERMINATOR = 0xFF;
constexpr uint8_t FRAME_TERMINATOR_LENGTH = 3;

uint8_t unicodeToIso88592(uint32_t codePoint) {
  switch (codePoint) {
    case 0x00B0: return 0xB0;  // Znak stopnia
    case 0x0104: return 0xA1;  // Ą
    case 0x0105: return 0xB1;  // ą
    case 0x0106: return 0xC6;  // Ć
    case 0x0107: return 0xE6;  // ć
    case 0x0118: return 0xCA;  // Ę
    case 0x0119: return 0xEA;  // ę
    case 0x0141: return 0xA3;  // Ł
    case 0x0142: return 0xB3;  // ł
    case 0x0143: return 0xD1;  // Ń
    case 0x0144: return 0xF1;  // ń
    case 0x00D3: return 0xD3;  // Ó
    case 0x00F3: return 0xF3;  // ó
    case 0x015A: return 0xA6;  // Ś
    case 0x015B: return 0xB6;  // ś
    case 0x0179: return 0xAC;  // Ź
    case 0x017A: return 0xBC;  // ź
    case 0x017B: return 0xAF;  // Ż
    case 0x017C: return 0xBF;  // ż
    default: return '?';
  }
}

bool utf8ToIso88592(const char* input, char* output, size_t outputSize) {
  if (input == nullptr || output == nullptr || outputSize == 0) return false;

  size_t inputIndex = 0;
  size_t outputIndex = 0;
  bool complete = true;
  while (input[inputIndex] != '\0') {
    const uint8_t first = static_cast<uint8_t>(input[inputIndex]);
    uint32_t codePoint = first;
    size_t consumed = 1;

    if ((first & 0xE0U) == 0xC0U && input[inputIndex + 1] != '\0') {
      const uint8_t second = static_cast<uint8_t>(input[inputIndex + 1]);
      if ((second & 0xC0U) == 0x80U) {
        codePoint = ((first & 0x1FU) << 6U) | (second & 0x3FU);
        consumed = 2;
      } else {
        codePoint = '?';
      }
    } else if ((first & 0xF0U) == 0xE0U &&
               input[inputIndex + 1] != '\0' &&
               input[inputIndex + 2] != '\0') {
      const uint8_t second = static_cast<uint8_t>(input[inputIndex + 1]);
      const uint8_t third = static_cast<uint8_t>(input[inputIndex + 2]);
      if ((second & 0xC0U) == 0x80U && (third & 0xC0U) == 0x80U) {
        codePoint = ((first & 0x0FU) << 12U) |
                    ((second & 0x3FU) << 6U) | (third & 0x3FU);
        consumed = 3;
      } else {
        codePoint = '?';
      }
    } else if (first >= 0x80U) {
      codePoint = '?';
    }

    const uint8_t encoded =
        codePoint < 0x80U ? static_cast<uint8_t>(codePoint)
                          : unicodeToIso88592(codePoint);
    if (outputIndex + 1 >= outputSize) {
      complete = false;
      break;
    }
    output[outputIndex++] = static_cast<char>(encoded);
    inputIndex += consumed;
  }
  output[outputIndex] = '\0';
  return complete;
}
}  // namespace

NextionDriver::NextionDriver(AppState& state) : state_(state), serial_(1) {}

void NextionDriver::begin() {
  state_.ui.nextionReady = false;
  resetRxFrame();
  txHead_ = 0;
  txTail_ = 0;
  txCount_ = 0;
  lastTxMs_ = millis() - Config::Nextion::COMMAND_INTERVAL_MS;

  serial_.begin(Config::Nextion::BAUD_RATE, SERIAL_8N1, Config::Nextion::RX_PIN,
                Config::Nextion::TX_PIN);

  Serial.printf("NEXTION UART uruchomiony: TX=%d RX=%d baud=%lu 8N1\n",
                Config::Nextion::TX_PIN, Config::Nextion::RX_PIN,
                static_cast<unsigned long>(Config::Nextion::BAUD_RATE));
}

void NextionDriver::loop() {
  if (updateMode_) {
    return;
  }

  processRx();

  const bool partialFrame = rxLength_ > 0 || rxTerminatorCount_ > 0 || rxDiscarding_;
  if (partialFrame &&
      static_cast<uint32_t>(millis() - lastRxByteMs_) >=
          Config::Nextion::RX_FRAME_TIMEOUT_MS) {
    Serial.println("NEXTION WARNING: timeout niepełnej ramki RX");
    ++state_.diagnostics.nextion.parserTimeouts;
    resetRxFrame();
  }

  processTx();
}

bool NextionDriver::sendCommand(const char* command) {
  if (updateMode_) {
    ++state_.diagnostics.nextion.txDropped;
    return false;
  }

  if (command == nullptr || command[0] == '\0') {
    ++state_.diagnostics.nextion.txDropped;
    return false;
  }

  const size_t commandLength =
      strnlen(command, Config::Nextion::MAX_COMMAND_LENGTH);
  if (commandLength >= Config::Nextion::MAX_COMMAND_LENGTH) {
    Serial.println("NEXTION WARNING: komenda TX jest za długa");
    ++state_.diagnostics.nextion.txDropped;
    return false;
  }

  if (txCount_ >= Config::Nextion::COMMAND_QUEUE_SIZE) {
    Serial.println("NEXTION WARNING: kolejka TX jest pełna");
    ++state_.diagnostics.nextion.txDropped;
    return false;
  }

  memcpy(txQueue_[txTail_].text, command, commandLength + 1);
  txTail_ = (txTail_ + 1) % Config::Nextion::COMMAND_QUEUE_SIZE;
  ++txCount_;
  ++state_.diagnostics.nextion.txQueued;
  if (txCount_ > state_.diagnostics.nextion.txQueueHighWater) {
    state_.diagnostics.nextion.txQueueHighWater = txCount_;
  }
  return true;
}

bool NextionDriver::setText(const char* component, const char* text) {
  if (component == nullptr || component[0] == '\0' || text == nullptr) {
    return false;
  }

  char command[Config::Nextion::MAX_COMMAND_LENGTH]{};
  const int prefixLength =
      snprintf(command, sizeof(command), "%s.txt=", component);
  if (prefixLength < 0 ||
      static_cast<size_t>(prefixLength) >= sizeof(command) - 2U) {
    Serial.println("NEXTION WARNING: polecenie setText jest za długie");
    return false;
  }

  size_t commandLength = static_cast<size_t>(prefixLength);
  command[commandLength++] = static_cast<char>(0x22);
  bool truncated = false;

  for (size_t index = 0; text[index] != '\0'; ++index) {
    uint8_t character = static_cast<uint8_t>(text[index]);
    if (character < 0x20U || character == 0x7FU) {
      character = ' ';
    } else if (character == 0xFFU) {
      character = '?';
    }

    const bool needsEscape = character == 0x22U || character == '\\';
    const size_t requiredSpace = needsEscape ? 2U : 1U;
    // Zostawiamy miejsce na końcowy cudzysłów i terminator C.
    if (commandLength + requiredSpace + 2U > sizeof(command)) {
      truncated = true;
      break;
    }
    if (needsEscape) {
      command[commandLength++] = '\\';
    }
    command[commandLength++] = static_cast<char>(character);
  }

  command[commandLength++] = static_cast<char>(0x22);
  command[commandLength] = '\0';
  if (truncated) {
    Serial.println("NEXTION WARNING: tekst komponentu został skrócony");
  }
  return sendCommand(command);
}

bool NextionDriver::setTextUtf8(const char* component,
                                const char* utf8Text) {
  char isoText[Config::Nextion::MAX_COMMAND_LENGTH]{};
  const bool complete =
      utf8ToIso88592(utf8Text, isoText, sizeof(isoText));
  if (!complete) {
    Serial.println("NEXTION WARNING: tekst UTF-8 został skrócony");
  }
  return setText(component, isoText);
}

bool NextionDriver::setPicture(const char* component, uint16_t pictureId) {
  if (component == nullptr || component[0] == '\0') {
    return false;
  }

  char command[Config::Nextion::MAX_COMMAND_LENGTH]{};
  const int result =
      snprintf(command, sizeof(command), "%s.pic=%u", component, pictureId);
  if (result < 0 || static_cast<size_t>(result) >= sizeof(command)) {
    return false;
  }
  return sendCommand(command);
}

bool NextionDriver::setColor(const char* component, uint16_t color) {
  if (component == nullptr || component[0] == '\0') {
    return false;
  }

  char command[Config::Nextion::MAX_COMMAND_LENGTH]{};
  const int result =
      snprintf(command, sizeof(command), "%s.pco=%u", component, color);
  if (result < 0 || static_cast<size_t>(result) >= sizeof(command)) {
    return false;
  }
  return sendCommand(command);
}

bool NextionDriver::changePage(uint8_t pageId) {
  char command[24]{};
  const int result = snprintf(command, sizeof(command), "page %u", pageId);
  return result > 0 && static_cast<size_t>(result) < sizeof(command) &&
         sendCommand(command);
}

bool NextionDriver::requestCurrentPage() {
  return sendCommand("sendme");
}

bool NextionDriver::canQueueCommands(size_t count) const {
  return !updateMode_ && count <= Config::Nextion::COMMAND_QUEUE_SIZE - txCount_;
}

bool NextionDriver::commandQueueIdle() const {
  return !updateMode_ && txCount_ == 0;
}

void NextionDriver::clearNormalTxQueue() {
  if (updateMode_) {
    return;
  }

  txHead_ = 0;
  txTail_ = 0;
  txCount_ = 0;
}

void NextionDriver::flushNormalTx() {
  if (!updateMode_) {
    // Po opróżnieniu kolejki czekamy, aż ostatnie bajty fizycznie opuszczą UART.
    serial_.flush(true);
  }
}

bool NextionDriver::enterUpdateMode() {
  if (updateMode_) {
    return false;
  }

  updateMode_ = true;
  txHead_ = 0;
  txTail_ = 0;
  txCount_ = 0;
  resetRxFrame();
  clearUpdateRx();

  state_.ui.currentPage = UiPage::UNKNOWN;
  state_.ui.nextionReady = false;
  state_.ui.nextionSynchronized = false;
  state_.ui.fullRefreshRequested = true;
  state_.ui.activePageCacheValid = false;
  return true;
}

void NextionDriver::leaveUpdateMode() {
  setUpdateBaud(Config::Nextion::BAUD_RATE);
  clearUpdateRx();
  resetRxFrame();
  txHead_ = 0;
  txTail_ = 0;
  txCount_ = 0;
  lastTxMs_ = millis() - Config::Nextion::COMMAND_INTERVAL_MS;

  state_.ui.currentPage = UiPage::UNKNOWN;
  state_.ui.nextionReady = false;
  state_.ui.nextionSynchronized = false;
  state_.ui.fullRefreshRequested = true;
  state_.ui.activePageCacheValid = false;
  updateMode_ = false;
}

bool NextionDriver::isUpdateMode() const {
  return updateMode_;
}

void NextionDriver::setUpdateBaud(uint32_t baud) {
  serial_.flush(true);
  serial_.updateBaudRate(baud);
}

bool NextionDriver::writeUpdateCommand(const char* command) {
  if (!updateMode_ || command == nullptr) {
    return false;
  }

  if (command[0] != '\0') {
    serial_.write(reinterpret_cast<const uint8_t*>(command), strlen(command));
  }
  serial_.write(FRAME_TERMINATOR);
  serial_.write(FRAME_TERMINATOR);
  serial_.write(FRAME_TERMINATOR);
  return true;
}

size_t NextionDriver::writeUpdateData(const uint8_t* data, size_t length) {
  if (!updateMode_ || data == nullptr || length == 0) {
    return 0;
  }
  return serial_.write(data, length);
}

int NextionDriver::readUpdateByte() {
  return updateMode_ ? serial_.read() : -1;
}

void NextionDriver::flushUpdateTx() {
  if (updateMode_) {
    serial_.flush(true);
  }
}

void NextionDriver::clearUpdateRx() {
  while (serial_.available() > 0) {
    serial_.read();
  }
}

void NextionDriver::setTouchCallback(TouchCallback callback, void* context) {
  touchCallback_ = callback;
  touchContext_ = context;
}

void NextionDriver::setPageCallback(PageCallback callback, void* context) {
  pageCallback_ = callback;
  pageContext_ = context;
}

void NextionDriver::setReadyCallback(ReadyCallback callback, void* context) {
  readyCallback_ = callback;
  readyContext_ = context;
}

void NextionDriver::setErrorCallback(ErrorCallback callback, void* context) {
  errorCallback_ = callback;
  errorContext_ = context;
}

void NextionDriver::processRx() {
  size_t processedBytes = 0;
  while (serial_.available() > 0 &&
         processedBytes < Config::Nextion::RX_BYTES_PER_LOOP) {
    const int received = serial_.read();
    if (received >= 0) {
      processRxByte(static_cast<uint8_t>(received));
      ++processedBytes;
    }
  }
}

void NextionDriver::processRxByte(uint8_t byteValue) {
  lastRxByteMs_ = millis();

  if (byteValue == FRAME_TERMINATOR) {
    ++rxTerminatorCount_;
    if (rxTerminatorCount_ == FRAME_TERMINATOR_LENGTH) {
      if (!rxDiscarding_ && rxLength_ > 0) {
        processFrame();
      }
      resetRxFrame();
    }
    return;
  }

  if (rxDiscarding_) {
    rxTerminatorCount_ = 0;
    return;
  }

  while (rxTerminatorCount_ > 0) {
    appendRxByte(FRAME_TERMINATOR);
    --rxTerminatorCount_;
  }
  appendRxByte(byteValue);
}

void NextionDriver::appendRxByte(uint8_t byteValue) {
  if (rxDiscarding_) {
    return;
  }
  if (rxLength_ >= Config::Nextion::RX_FRAME_BUFFER_SIZE) {
    Serial.println("NEXTION WARNING: przepełnienie bufora ramki RX");
    rxLength_ = 0;
    rxDiscarding_ = true;
    ++state_.diagnostics.nextion.malformedFrames;
    return;
  }
  rxBuffer_[rxLength_++] = byteValue;
}

void NextionDriver::processFrame() {
  ++state_.diagnostics.nextion.receivedFrames;
  if (Config::Logging::DEBUG_NEXTION_RX) {
    logRxFrame();
  }

  const uint8_t frameType = rxBuffer_[0];
  switch (frameType) {
    case 0x65:
      if (rxLength_ == 4 &&
          (rxBuffer_[3] == static_cast<uint8_t>(NextionTouchAction::RELEASE) ||
           rxBuffer_[3] == static_cast<uint8_t>(NextionTouchAction::PRESS))) {
        if (touchCallback_ != nullptr) {
          const NextionTouchEvent event{
              rxBuffer_[1], rxBuffer_[2],
              static_cast<NextionTouchAction>(rxBuffer_[3])};
          touchCallback_(touchContext_, event);
        }
        ++state_.diagnostics.nextion.touchFrames;
      } else {
        Serial.println("NEXTION WARNING: niepoprawna ramka touch 0x65");
        ++state_.diagnostics.nextion.malformedFrames;
      }
      break;

    case 0x66:
      if (rxLength_ == 2) {
        if (pageCallback_ != nullptr) {
          pageCallback_(pageContext_, rxBuffer_[1]);
        }
        ++state_.diagnostics.nextion.pageFrames;
      } else {
        Serial.println("NEXTION WARNING: niepoprawna ramka strony 0x66");
        ++state_.diagnostics.nextion.malformedFrames;
      }
      break;

    case 0x88:
      if (rxLength_ == 1) {
        if (readyCallback_ != nullptr) {
          readyCallback_(readyContext_);
        }
      } else {
        Serial.println("NEXTION WARNING: niepoprawna ramka Ready 0x88");
        ++state_.diagnostics.nextion.malformedFrames;
      }
      break;

    case 0x00:
    case 0x1A:
    case 0x1B:
      if (rxLength_ == 1) {
        if (frameType == 0x1AU) {
          ++state_.diagnostics.nextion.error1AFrames;
        }
        if (errorCallback_ != nullptr) {
          errorCallback_(errorContext_, frameType);
        }
      } else {
        Serial.println("NEXTION WARNING: niepoprawna ramka błędu");
        ++state_.diagnostics.nextion.malformedFrames;
      }
      break;

    default:
      if (Config::Logging::DEBUG_NEXTION_RX) {
        Serial.printf("NEXTION RX: pominięta ramka typu 0x%02X\n", frameType);
      }
      break;
  }
}

void NextionDriver::resetRxFrame() {
  rxLength_ = 0;
  rxTerminatorCount_ = 0;
  rxDiscarding_ = false;
  lastRxByteMs_ = millis();
}

void NextionDriver::processTx() {
  if (txCount_ == 0) {
    return;
  }

  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastTxMs_) <
      Config::Nextion::COMMAND_INTERVAL_MS) {
    return;
  }

  const char* command = txQueue_[txHead_].text;
  serial_.print(command);
  serial_.write(FRAME_TERMINATOR);
  serial_.write(FRAME_TERMINATOR);
  serial_.write(FRAME_TERMINATOR);

  if (Config::Logging::DEBUG_NEXTION_TX) {
    Serial.printf("NEXTION TX: %s [FF FF FF]\n", command);
  }

  txHead_ = (txHead_ + 1) % Config::Nextion::COMMAND_QUEUE_SIZE;
  --txCount_;
  ++state_.diagnostics.nextion.txSent;
  lastTxMs_ = now;
}

void NextionDriver::logRxFrame() const {
  Serial.print("NEXTION RX:");
  for (size_t index = 0; index < rxLength_; ++index) {
    Serial.printf(" %02X", rxBuffer_[index]);
  }
  Serial.println(" FF FF FF");
}
