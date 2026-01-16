#pragma once
#include <Arduino.h>

// Start playback with current file from settings.
void audioStart();

// Start playback of specific file (WAV or MP3).
// Also saves the file as default in settings.json.
void audioStartFile(const String& path);

// Stop playback.
void audioStop();

// Restart playback (stop + start).
void audioRestart();

// Check if audio is currently playing.
bool audioIsRunning();

// Check if file format is supported (WAV or MP3).
bool audioIsSupportedFormat(const String& path);
