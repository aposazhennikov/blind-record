#pragma once
#include <Arduino.h>

// Audio Progress module.
// Tracks playback position and provides progress info for UI.

struct AudioProgressInfo {
  uint32_t totalBytes;    // Total data size in bytes.
  uint32_t playedBytes;   // Bytes played so far.
  uint32_t totalMs;       // Total duration in milliseconds.
  uint32_t playedMs;      // Played duration in milliseconds.
  uint8_t  percent;       // Progress 0-100%.
  bool     playing;       // Currently playing.
  String   fileName;      // Current file name.
  uint32_t sampleRate;    // Sample rate of current file.
  uint16_t channels;      // Number of channels.
  uint16_t bitsPerSample; // Bits per sample.
};

// Global progress info (updated by audio_player).
extern AudioProgressInfo g_audioProgress;

// Initialize progress tracking.
void progressInit();

// Reset progress (call when starting new file).
void progressReset(const String& fileName, uint32_t totalBytes, uint32_t sampleRate,
                   uint16_t channels, uint16_t bitsPerSample);

// Update progress (call during playback).
void progressUpdate(uint32_t playedBytes);

// Mark playback as stopped.
void progressStop();

// Get progress as JSON string.
String progressGetJson();

// Calculate duration from bytes.
uint32_t progressBytesToMs(uint32_t bytes, uint32_t sampleRate, uint16_t channels,
                           uint16_t bitsPerSample);
