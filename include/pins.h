#pragma once
#include <Arduino.h>

// LCD ILI9341 - założenie do potwierdzenia na konkretnej rewizji PCB.
constexpr uint8_t PIN_TFT_CS   = D8;
constexpr uint8_t PIN_TFT_DC   = D1;
constexpr int8_t  PIN_TFT_RST  = -1;

// Wspólna magistrala SPI
constexpr uint8_t BOARD_SPI_SCK  = D5;
constexpr uint8_t BOARD_SPI_MISO = D6;
constexpr uint8_t BOARD_SPI_MOSI = D7;

// Touch XPT2046. GPIO16/D0 nie jest używany jako IRQ; sterownik odpytuje touch.
constexpr uint8_t PIN_TOUCH_CS  = D2;
constexpr uint8_t PIN_TOUCH_IRQ = D0;

// GPIO15/D8 musi być LOW, a GPIO0/D3 i GPIO2/D4 HIGH podczas startu.
// SD i podświetlenie są na tym etapie nieużywane.
constexpr uint8_t PIN_SD_CS = D4;
constexpr uint8_t PIN_BACKLIGHT = D3;
