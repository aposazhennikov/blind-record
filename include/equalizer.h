#pragma once
#include <Arduino.h>

// 5-band Equalizer module.
// Bands: Sub-bass (60Hz), Bass (250Hz), Mid (1kHz), Presence (4kHz), Brilliance (12kHz).

// EQ band gains in dB (-12 to +12).
struct EqSettings {
  float band60Hz;  // Sub-bass.
  float band250Hz; // Bass.
  float band1kHz;  // Mid.
  float band4kHz;  // Presence.
  float band12kHz; // Brilliance.
  bool  enabled;   // EQ on/off.
};

// Global EQ settings.
extern EqSettings g_eqSettings;

// Initialize EQ with default (flat) settings.
void eqInit();

// Set EQ defaults (all bands at 0 dB).
void eqSetDefaults(EqSettings& eq);

// Apply EQ to a stereo sample pair (in-place).
// sampleRate is needed for filter calculations.
void eqProcessSample(int16_t& L, int16_t& R, uint32_t sampleRate);

// Apply EQ to a buffer of stereo samples.
void eqProcessBuffer(int16_t* buffer, size_t frames, uint32_t sampleRate);

// Recalculate filter coefficients (call after changing settings).
void eqUpdateCoefficients(uint32_t sampleRate);

// Get/set individual band (index 0-4).
float eqGetBand(int index);
void  eqSetBand(int index, float gainDb);

// Band names for UI.
const char* eqGetBandName(int index);
int         eqGetBandFreq(int index);
