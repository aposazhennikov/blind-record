#pragma once
#include <Arduino.h>

// MP3 Player module using ESP8266Audio library.
// Provides MP3 decoding and I2S output.

// Initialize MP3 player.
void mp3Init();

// Start playing MP3 file.
// Returns true if playback started successfully.
bool mp3StartFile(const String& path);

// Stop MP3 playback (sets flag and cleans up).
void mp3Stop();

// Internal cleanup - releases all resources.
void mp3Cleanup();

// Must be called in loop() to pump audio data.
// Returns true if still playing.
bool mp3Loop();

// Check if MP3 is currently playing.
bool mp3IsPlaying();

// Check if stop was requested.
bool mp3IsStopRequested();

// Get MP3 info (after starting playback).
uint32_t mp3GetSampleRate();
uint16_t mp3GetChannels();
uint16_t mp3GetBitsPerSample();

// Get estimated duration in milliseconds (approximate for MP3).
uint32_t mp3GetDurationMs();

// Get current position in milliseconds.
uint32_t mp3GetPositionMs();

// Set volume (0.0 - 1.0). Automatically applies headroom for MP3.
void mp3SetVolume(float volume);
