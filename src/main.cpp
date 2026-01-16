#include "app_config.h"
#include "audio_player.h"
#include "audio_progress.h"
#include "auto_tuner.h"
#include "equalizer.h"
#include "net_utils.h"
#include "ntp_time.h"
#include "settings.h"
#include "web_log.h"
#include "web_panel.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// SD SPI frequencies to try (from fast to slow).
static const uint32_t SD_SPI_SPEEDS[]     = {20000000, 10000000, 4000000};
static const int      SD_SPI_SPEEDS_COUNT = 3;
static uint32_t       g_sdSpiFreq         = 0;

static bool initSD()
{
  WebLog.println("[SD] Initializing...");
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // Try different SPI speeds, starting from fastest.
  for (int i = 0; i < SD_SPI_SPEEDS_COUNT; i++) {
    uint32_t freq = SD_SPI_SPEEDS[i];
    WebLog.print("[SD] Trying ");
    WebLog.print(freq / 1000000);
    WebLog.println(" MHz...");

    if (SD.begin(SD_CS, SPI, freq)) {
      g_sdSpiFreq = freq;

      // Print SD card info.
      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      WebLog.print("[SD] ✅ SD ready, size: ");
      WebLog.print((uint32_t)cardSize);
      WebLog.print(" MB @ ");
      WebLog.print(freq / 1000000);
      WebLog.println(" MHz SPI");

      return true;
    }

    // Failed, try next speed.
    SD.end();
    delay(100);
  }

  WebLog.println("[SD] ❌ SD init failed at all speeds");
  WebLog.println("[SD] Check: card inserted? wiring correct? card formatted FAT32?");
  return false;
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  // Initialize web log first.
  webLogBegin();

  WebLog.println("==================================");
  WebLog.println("ESP32 WAV Player + Web Panel v2.0");
  WebLog.println("==================================");

  // Initialize modules.
  progressInit();
  eqInit();
  tunerInit();

  // 1) Wi-Fi.
  wifiConnectAndLog();
  if (isWiFiOk()) {
    resolveGoogleDNS();
    checkGoogleTCPWithLog(); // Use version with logging for boot only.
    webPanelBegin(audioRestart);
  } else {
    WebLog.println("[BOOT] ⚠️ Wi-Fi not connected, Web panel won't be available.");
  }

  // 2) SD.
  if (!initSD()) {
    WebLog.println("[BOOT] ❌ SD init failed, cannot continue");
    return;
  }

  // 3) settings.json.
  settingsLoadFromSD();

  // 4) Initialize NTP time (after settings loaded for timezone).
  if (isWiFiOk()) {
    ntpInit();
  }

  // Sync EQ settings.
  g_eqSettings.band60Hz  = g_settings.eq.band60Hz;
  g_eqSettings.band250Hz = g_settings.eq.band250Hz;
  g_eqSettings.band1kHz  = g_settings.eq.band1kHz;
  g_eqSettings.band4kHz  = g_settings.eq.band4kHz;
  g_eqSettings.band12kHz = g_settings.eq.band12kHz;
  g_eqSettings.enabled   = g_settings.eqEnabled;

  // Sync auto-tuner.
  tunerSetEnabled(g_settings.autoTuneEnabled);

  // 4) Check if current file exists.
  if (!SD.exists(g_settings.currentFile)) {
    WebLog.print("[SD] ⚠️ Current file not found: ");
    WebLog.println(g_settings.currentFile);

    // Try to find any audio file (.wav or .mp3).
    File root = SD.open("/");
    if (root) {
      File entry = root.openNextFile();
      while (entry) {
        String name  = entry.name();
        String lower = name;
        lower.toLowerCase();
        if (lower.endsWith(".wav") || lower.endsWith(".mp3")) {
          g_settings.currentFile = "/" + name;
          WebLog.print("[SD] ✅ Found audio file: ");
          WebLog.println(g_settings.currentFile);
          break;
        }
        entry = root.openNextFile();
      }
      root.close();
    }
  } else {
    WebLog.print("[SD] ✅ Current file: ");
    WebLog.println(g_settings.currentFile);
  }

  // 5) Start audio playback.
  if (SD.exists(g_settings.currentFile)) {
    audioStart();
  } else {
    WebLog.println("[BOOT] ⚠️ No audio file found, audio not started");
  }

  WebLog.println("[BOOT] ✅ Setup done");
  WebLog.print("[BOOT] Free heap: ");
  WebLog.print(ESP.getFreeHeap());
  WebLog.println(" bytes");
}

void loop()
{
  if (isWiFiOk()) {
    webPanelHandleClient();
  }

  // Small delay to prevent watchdog issues.
  delay(1);
}
