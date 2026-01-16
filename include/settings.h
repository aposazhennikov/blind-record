#pragma once
#include <Arduino.h>

// EQ settings (5 bands).
struct EqBands {
  float band60Hz;  // Sub-bass (-12..+12 dB).
  float band250Hz; // Bass.
  float band1kHz;  // Mid.
  float band4kHz;  // Presence.
  float band12kHz; // Brilliance.
};

struct AudioSettings {
  float   volume;            // 0.0 .. 1.0
  int     sampleRate;        // 8000..48000
  int     inBufBytes;        // 512..8192
  int     dmaBufCount;       // 4..16
  int     dmaBufLen;         // 128..1024
  String  currentFile;       // Current file to play.
  bool    eqEnabled;         // EQ on/off.
  EqBands eq;                // EQ band gains.
  bool    autoTuneEnabled;   // Auto-tuner on/off.
  bool    resamplingEnabled; // Resampling on/off.
  String  timezone;          // Timezone (e.g., "Europe/Moscow").
  int     timezoneOffset;    // UTC offset in seconds (e.g., 10800 for MSK).
};

extern AudioSettings g_settings;

void settingsSetDefaults(AudioSettings& s);
bool settingsLoadFromSD();
bool settingsSaveToSD();
