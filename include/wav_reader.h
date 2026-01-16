#pragma once
#include <Arduino.h>
#include <SD.h>

struct WavInfo {
  bool     ok            = false;
  uint16_t audioFormat   = 0;
  uint16_t numChannels   = 0;
  uint32_t sampleRate    = 0;
  uint16_t bitsPerSample = 0;
  uint32_t dataOffset    = 0;
  uint32_t dataSize      = 0;
};

WavInfo parseWavHeader(File& f);
