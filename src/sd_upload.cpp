#include "sd_upload.h"

#include "audio_player.h"
#include "web_log.h"

#include <SD.h>

// Upload buffer for faster writes.
// Larger buffer = fewer SD writes = faster upload.
// 32KB is a good balance between speed and memory usage.
static const size_t UPLOAD_BUFFER_SIZE = 32768; // 32KB buffer.
static uint8_t*     g_uploadBuffer     = nullptr;
static size_t       g_uploadBufferPos  = 0;

static File          g_uploadFile;
static String        g_uploadStatus     = "";
static String        g_uploadPath       = "";
static size_t        g_uploadSize       = 0;
static size_t        g_uploadTotalSize  = 0;
static unsigned long g_uploadStartTime  = 0;
static bool          g_uploadInProgress = false;
static bool          g_audioWasPlaying  = false; // Track if we stopped audio.

// Progress logging interval (bytes).
static const size_t LOG_INTERVAL  = 1024 * 1024; // Every 1MB.
static size_t       g_lastLogSize = 0;

String sdUploadGetStatus()
{
  return g_uploadStatus;
}

String sdUploadGetProgressJson()
{
  if (!g_uploadInProgress) {
    return "{\"inProgress\":false}";
  }

  unsigned long elapsed    = millis() - g_uploadStartTime;
  float         speedKBps  = 0;
  unsigned long etaSeconds = 0;

  if (elapsed > 0) {
    speedKBps = (float)g_uploadSize / 1024.0f / ((float)elapsed / 1000.0f);
    if (speedKBps > 0 && g_uploadTotalSize > g_uploadSize) {
      etaSeconds = (unsigned long)((g_uploadTotalSize - g_uploadSize) / 1024.0f / speedKBps);
    }
  }

  int percent = 0;
  if (g_uploadTotalSize > 0) {
    percent = (int)((uint64_t)g_uploadSize * 100 / g_uploadTotalSize);
    if (percent > 100)
      percent = 100;
  }

  String json = "{";
  json += "\"inProgress\":true,";
  json += "\"fileName\":\"" + g_uploadPath + "\",";
  json += "\"uploaded\":" + String(g_uploadSize) + ",";
  json += "\"total\":" + String(g_uploadTotalSize) + ",";
  json += "\"percent\":" + String(percent) + ",";
  json += "\"speedKBps\":" + String(speedKBps, 1) + ",";
  json += "\"etaSeconds\":" + String(etaSeconds);
  json += "}";

  return json;
}

// Flush buffer to SD card.
static void flushUploadBuffer()
{
  if (g_uploadFile && g_uploadBufferPos > 0) {
    g_uploadFile.write(g_uploadBuffer, g_uploadBufferPos);
    g_uploadBufferPos = 0;
  }
}

static void handleUploadPage(WebServer& server)
{
  String html = R"HTML(
<!doctype html>
<html>
<head><meta charset="utf-8"><title>Upload</title></head>
<body style="background:#0b1020;color:#e7e9ee;font-family:sans-serif;padding:20px">
<h2>📤 Загрузка файла на SD</h2>
<form method="POST" action="/upload" enctype="multipart/form-data">
  <input type="file" name="file" accept=".wav,.mp3,.txt,.json" style="margin:10px 0"><br>
  <input type="submit" value="Загрузить" style="padding:10px 20px;cursor:pointer">
</form>
<p id="status"></p>
</body>
</html>
)HTML";
  server.send(200, "text/html", html);
}

static void handleUploadData(WebServer& server)
{
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    g_uploadPath       = filename;
    g_uploadSize       = 0;
    g_uploadTotalSize  = upload.totalSize;
    g_uploadStartTime  = millis();
    g_uploadInProgress = true;
    g_lastLogSize      = 0;
    g_uploadBufferPos  = 0;

    // Stop audio playback to free SD bandwidth.
    g_audioWasPlaying = audioIsRunning();
    if (g_audioWasPlaying) {
      WebLog.println("[UPLOAD] ⏸ Pausing audio for faster upload");
      audioStop();
    }

    // Allocate upload buffer if not already allocated.
    if (!g_uploadBuffer) {
      g_uploadBuffer = (uint8_t*)malloc(UPLOAD_BUFFER_SIZE);
      if (!g_uploadBuffer) {
        WebLog.println("[UPLOAD] ❌ Failed to allocate buffer");
        g_uploadStatus     = "Ошибка: недостаточно памяти";
        g_uploadInProgress = false;
        return;
      }
    }

    WebLog.print("[UPLOAD] Starting: ");
    WebLog.println(g_uploadPath);
    if (g_uploadTotalSize > 0) {
      WebLog.print("[UPLOAD] Total size: ");
      WebLog.print(g_uploadTotalSize / 1024);
      WebLog.println(" KB");
    }

    if (SD.exists(g_uploadPath)) {
      SD.remove(g_uploadPath);
    }

    g_uploadFile = SD.open(g_uploadPath, FILE_WRITE);
    if (!g_uploadFile) {
      WebLog.println("[UPLOAD] ❌ Failed to create file");
      g_uploadStatus     = "Ошибка: не удалось создать файл";
      g_uploadInProgress = false;
    } else {
      g_uploadStatus = "Загрузка...";
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (g_uploadFile && g_uploadBuffer) {
      size_t   bytesToWrite = upload.currentSize;
      uint8_t* srcPtr       = upload.buf;

      while (bytesToWrite > 0) {
        size_t spaceInBuffer = UPLOAD_BUFFER_SIZE - g_uploadBufferPos;
        size_t toCopy        = (bytesToWrite < spaceInBuffer) ? bytesToWrite : spaceInBuffer;

        memcpy(g_uploadBuffer + g_uploadBufferPos, srcPtr, toCopy);
        g_uploadBufferPos += toCopy;
        srcPtr += toCopy;
        bytesToWrite -= toCopy;

        // Flush buffer when full.
        if (g_uploadBufferPos >= UPLOAD_BUFFER_SIZE) {
          flushUploadBuffer();
        }
      }

      g_uploadSize += upload.currentSize;

      if (g_uploadTotalSize == 0 && upload.totalSize > 0) {
        g_uploadTotalSize = upload.totalSize;
      }

      // Log progress at intervals (reduced logging for speed).
      if (g_uploadSize - g_lastLogSize >= LOG_INTERVAL) {
        unsigned long elapsed   = millis() - g_uploadStartTime;
        float         speedKBps = (float)g_uploadSize / 1024.0f / ((float)elapsed / 1000.0f);

        WebLog.print("[UPLOAD] ");
        WebLog.print(g_uploadSize / 1024);
        WebLog.print(" KB");
        if (g_uploadTotalSize > 0) {
          int percent = (int)((uint64_t)g_uploadSize * 100 / g_uploadTotalSize);
          WebLog.print(" (");
          WebLog.print(percent);
          WebLog.print("%)");
        }
        WebLog.print(" @ ");
        WebLog.print(speedKBps, 1);
        WebLog.println(" KB/s");

        g_lastLogSize = g_uploadSize;
      }
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    // Flush remaining data in buffer.
    flushUploadBuffer();

    g_uploadInProgress = false;

    if (g_uploadFile) {
      g_uploadFile.close();

      unsigned long elapsed = millis() - g_uploadStartTime;
      float         speedKBps =
          elapsed > 0 ? (float)g_uploadSize / 1024.0f / ((float)elapsed / 1000.0f) : 0;

      WebLog.print("[UPLOAD] ✅ Complete: ");
      WebLog.print(g_uploadPath);
      WebLog.print(" (");
      WebLog.print(g_uploadSize / 1024);
      WebLog.print(" KB in ");
      WebLog.print(elapsed / 1000);
      WebLog.print("s @ ");
      WebLog.print(speedKBps, 1);
      WebLog.println(" KB/s)");

      g_uploadStatus =
          "✅ Загружено: " + g_uploadPath + " (" + String(g_uploadSize / 1024) + " KB)";
    }

    // Resume audio if it was playing before.
    if (g_audioWasPlaying) {
      WebLog.println("[UPLOAD] ▶ Resuming audio playback");
      audioStart();
      g_audioWasPlaying = false;
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    g_uploadInProgress = false;
    g_uploadBufferPos  = 0;

    if (g_uploadFile) {
      g_uploadFile.close();
      SD.remove(g_uploadPath);
    }
    WebLog.println("[UPLOAD] ❌ Aborted");
    g_uploadStatus = "❌ Загрузка прервана";

    // Resume audio if it was playing before.
    if (g_audioWasPlaying) {
      WebLog.println("[UPLOAD] ▶ Resuming audio playback");
      audioStart();
      g_audioWasPlaying = false;
    }
  }
}

static void handleUploadComplete(WebServer& server)
{
  String json = "{\"status\":\"";
  json += g_uploadStatus;
  json += "\",\"path\":\"";
  json += g_uploadPath;
  json += "\",\"size\":";
  json += String(g_uploadSize);
  json += "}";

  server.send(200, "application/json", json);
}

static void handleUploadProgress(WebServer& server)
{
  server.send(200, "application/json", sdUploadGetProgressJson());
}

void sdUploadBegin(WebServer& server)
{
  server.on("/upload", HTTP_GET, [&server]() { handleUploadPage(server); });

  server.on(
      "/upload", HTTP_POST, [&server]() { handleUploadComplete(server); },
      [&server]() { handleUploadData(server); });

  server.on("/upload-progress", HTTP_GET, [&server]() { handleUploadProgress(server); });

  WebLog.println("[UPLOAD] ✅ Upload handlers registered");
}
