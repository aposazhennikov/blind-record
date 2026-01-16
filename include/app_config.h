#pragma once
#include <Arduino.h>

// ==================== Wi-Fi ====================
static const char* WIFI_SSID = "Aleksandr";
static const char* WIFI_PASS = "12345678";

// ==================== SD pins ====================
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18
#define SD_CS 5

// ==================== I2S pins ====================
#define I2S_DOUT 21 // DIN MAX98357A
#define I2S_BCLK 26 // BCLK
#define I2S_LRC 25  // LRC

// Aliases for ESP8266Audio library.
#define I2S_DOUT_PIN I2S_DOUT
#define I2S_BCLK_PIN I2S_BCLK
#define I2S_LRC_PIN I2S_LRC
