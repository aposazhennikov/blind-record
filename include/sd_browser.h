#pragma once
#include <Arduino.h>

// SD File Browser module.
// Provides directory listing, file selection, delete and rename operations.

struct FileInfo {
  String   name;
  uint32_t size;
  bool     isDir;
};

// Get list of files in directory (max 50 entries).
// Returns JSON array string: [{"name":"file.wav","size":12345,"isDir":false}, ...]
String sdListDir(const String& path);

// Delete file or empty directory.
bool sdDeleteFile(const String& path);

// Rename file.
bool sdRenameFile(const String& oldPath, const String& newPath);

// Check if file exists.
bool sdFileExists(const String& path);

// Get file size in bytes.
uint32_t sdGetFileSize(const String& path);

// Get current selected file path for playback.
const String& sdGetCurrentFile();

// Set current file for playback.
void sdSetCurrentFile(const String& path);

// Check if file is valid WAV (by extension).
bool sdIsWavFile(const String& path);

// Check if file is a supported audio format (WAV or MP3).
bool sdIsAudioFile(const String& path);
