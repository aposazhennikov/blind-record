#include "sd_browser.h"

#include "web_log.h"

#include <SD.h>

// Current file selected for playback.
static String g_currentFile = "/test.wav";

const String& sdGetCurrentFile()
{
  return g_currentFile;
}

void sdSetCurrentFile(const String& path)
{
  g_currentFile = path;
  WebLog.print("[SD] Current file set to: ");
  WebLog.println(g_currentFile);
}

bool sdIsWavFile(const String& path)
{
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".wav");
}

bool sdIsAudioFile(const String& path)
{
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".wav") || lower.endsWith(".mp3");
}

bool sdFileExists(const String& path)
{
  return SD.exists(path);
}

uint32_t sdGetFileSize(const String& path)
{
  File f = SD.open(path, FILE_READ);
  if (!f)
    return 0;
  uint32_t sz = f.size();
  f.close();
  return sz;
}

String sdListDir(const String& path)
{
  String json = "[";

  // Normalize path.
  String normalizedPath = path;
  if (normalizedPath.length() == 0) {
    normalizedPath = "/";
  }

  WebLog.print("[SD] Opening directory: ");
  WebLog.println(normalizedPath);

  File root = SD.open(normalizedPath);
  if (!root) {
    WebLog.print("[SD] ❌ Cannot open path: ");
    WebLog.println(normalizedPath);
    return "[]";
  }

  if (!root.isDirectory()) {
    WebLog.print("[SD] ❌ Not a directory: ");
    WebLog.println(normalizedPath);
    root.close();
    return "[]";
  }

  int       count    = 0;
  const int maxFiles = 50;

  File entry = root.openNextFile();
  while (entry && count < maxFiles) {
    String name = entry.name();

    // Remove leading path if present (ESP32 SD library quirk).
    int lastSlash = name.lastIndexOf('/');
    if (lastSlash >= 0) {
      name = name.substring(lastSlash + 1);
    }

    // Skip hidden files, system files, and empty names.
    if (name.length() == 0 || name.startsWith(".") || name.startsWith("_")) {
      entry = root.openNextFile();
      continue;
    }

    if (count > 0)
      json += ",";

    json += "{\"name\":\"";
    json += name;
    json += "\",\"size\":";
    json += String(entry.size());
    json += ",\"isDir\":";
    json += entry.isDirectory() ? "true" : "false";
    json += "}";

    count++;
    entry = root.openNextFile();
  }

  root.close();
  json += "]";

  WebLog.print("[SD] Listed ");
  WebLog.print(count);
  WebLog.print(" items in ");
  WebLog.println(normalizedPath);

  return json;
}

bool sdDeleteFile(const String& path)
{
  if (!SD.exists(path)) {
    WebLog.print("[SD] ❌ File not found: ");
    WebLog.println(path);
    return false;
  }

  // Prevent deleting settings.json.
  if (path == "/settings.json") {
    WebLog.println("[SD] ❌ Cannot delete settings.json");
    return false;
  }

  File f     = SD.open(path);
  bool isDir = f.isDirectory();
  f.close();

  bool ok = false;
  if (isDir) {
    ok = SD.rmdir(path);
  } else {
    ok = SD.remove(path);
  }

  if (ok) {
    WebLog.print("[SD] ✅ Deleted: ");
    WebLog.println(path);
  } else {
    WebLog.print("[SD] ❌ Failed to delete: ");
    WebLog.println(path);
  }

  return ok;
}

bool sdRenameFile(const String& oldPath, const String& newPath)
{
  if (!SD.exists(oldPath)) {
    WebLog.print("[SD] ❌ Source not found: ");
    WebLog.println(oldPath);
    return false;
  }

  if (SD.exists(newPath)) {
    WebLog.print("[SD] ❌ Destination already exists: ");
    WebLog.println(newPath);
    return false;
  }

  // Prevent renaming settings.json.
  if (oldPath == "/settings.json") {
    WebLog.println("[SD] ❌ Cannot rename settings.json");
    return false;
  }

  bool ok = SD.rename(oldPath, newPath);

  if (ok) {
    WebLog.print("[SD] ✅ Renamed: ");
    WebLog.print(oldPath);
    WebLog.print(" -> ");
    WebLog.println(newPath);

    // Update current file if it was renamed.
    if (g_currentFile == oldPath) {
      g_currentFile = newPath;
    }
  } else {
    WebLog.print("[SD] ❌ Failed to rename: ");
    WebLog.println(oldPath);
  }

  return ok;
}
