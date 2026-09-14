#pragma once
#include <Arduino.h>

// ============================================================
// TFT ILI9341
// ============================================================
// Mapowanie pinów TFT dla obecnej rewizji projektu.
constexpr uint8_t PIN_TFT_CS   = D8;
constexpr uint8_t PIN_TFT_DC   = D1;
constexpr int8_t  PIN_TFT_RST  = -1;

// ============================================================
// Wspólna magistrala SPI
// ============================================================
constexpr uint8_t BOARD_SPI_SCK  = D5;
constexpr uint8_t BOARD_SPI_MISO = D6;
constexpr uint8_t BOARD_SPI_MOSI = D7;

// ============================================================
// Dotyk XPT2046
// ============================================================
// GPIO16/D0 nie jest używany jako aktywne IRQ; sterownik działa w trybie
// polling i cyklicznie odpytuje kontroler dotyku.
constexpr uint8_t PIN_TOUCH_CS  = D2;
constexpr uint8_t PIN_TOUCH_IRQ = D0;

// ============================================================
// SD (nieużywane na tym etapie)
// ============================================================
constexpr uint8_t PIN_SD_CS = D4;

// ============================================================
// Backlight (nieużywany na tym etapie)
// ============================================================
// D3 = GPIO0 to pin bootujący ESP8266.
// Podświetlenie BL nie jest jeszcze potwierdzone sprzętowo na PCB.
// Firmware celowo nie steruje tym pinem (brak pinMode/digitalWrite/analogWrite).
constexpr uint8_t PIN_BACKLIGHT = D3;

// ============================================================
// Ostrzeżenia boot strap ESP8266
// ============================================================
// D8/GPIO15 musi być LOW podczas startu.
// D3/GPIO0 oraz D4/GPIO2 muszą być HIGH podczas startu.
