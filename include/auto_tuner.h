#pragma once
#include <Arduino.h>

// Auto-tuner module.
// Automatically adjusts buffer sizes when underruns are detected.

// Statistics for auto-tuning.
struct TunerStats {
  uint32_t underrunCount; // Number of underruns detected.
  uint32_t totalChunks;   // Total chunks processed.
  uint32_t lastTuneTime;  // Last time settings were adjusted.
  bool     tuningEnabled; // Auto-tuning on/off.
};

// Global tuner stats.
extern TunerStats g_tunerStats;

// Initialize auto-tuner.
void tunerInit();

// Report an underrun event.
void tunerReportUnderrun();

// Report a successful chunk (no underrun).
void tunerReportSuccess();

// Check if tuning is needed and apply changes.
// Returns true if settings were changed.
bool tunerCheck();

// Enable/disable auto-tuning.
void tunerSetEnabled(bool enabled);

// Reset statistics.
void tunerResetStats();

// Get stats as JSON.
String tunerGetStatsJson();
