#include "settings.h"

#include "web_log.h"

#include <ArduinoJson.h>
#include <SD.h>

static const char* SETTINGS_PATH = "/settings.json";
static const char* TMP_PATH      = "/settings.tmp";

AudioSettings g_settings;

static int clampInt(int v, int lo, int hi)
{
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static float clampFloat(float v, float lo, float hi)
{
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static void sanitize(AudioSettings& s)
{
  // Volume now allows up to 200% (2.0) for boost.
  s.volume = clampFloat(s.volume, 0.0f, 2.0f);
  // Software gain in dB (0 to +12 dB).
  s.softGain    = clampFloat(s.softGain, 0.0f, 12.0f);
  s.sampleRate  = clampInt(s.sampleRate, 8000, 48000);
  s.inBufBytes  = clampInt(s.inBufBytes, 512, 8192);
  s.dmaBufCount = clampInt(s.dmaBufCount, 4, 16);
  s.dmaBufLen   = clampInt(s.dmaBufLen, 128, 1024);

  s.eq.band60Hz  = clampFloat(s.eq.band60Hz, -12.0f, 12.0f);
  s.eq.band250Hz = clampFloat(s.eq.band250Hz, -12.0f, 12.0f);
  s.eq.band1kHz  = clampFloat(s.eq.band1kHz, -12.0f, 12.0f);
  s.eq.band4kHz  = clampFloat(s.eq.band4kHz, -12.0f, 12.0f);
  s.eq.band12kHz = clampFloat(s.eq.band12kHz, -12.0f, 12.0f);

  if (s.currentFile.length() == 0) {
    s.currentFile = "/test.wav";
  }
}

void settingsSetDefaults(AudioSettings& s)
{
  s.volume     = 1.0f; // 100% by default (can go up to 200%).
  s.softGain   = 0.0f; // No additional gain by default.
  s.sampleRate = 44100;
  // Larger buffers for smoother playback:
  // - inBufBytes: 8KB for SD read chunks (was 4KB)
  // - dmaBufCount: 12 DMA buffers (was 8) for more headroom
  // - dmaBufLen: 512 samples per buffer (was 256)
  // Total DMA buffer: 12 * 512 * 4 = 24KB (~136ms at 44100Hz stereo)
  s.inBufBytes         = 8192;
  s.dmaBufCount        = 12;
  s.dmaBufLen          = 512;
  s.currentFile        = "/test.wav";
  s.eqEnabled          = false;
  s.eq.band60Hz        = 0.0f;
  s.eq.band250Hz       = 0.0f;
  s.eq.band1kHz        = 0.0f;
  s.eq.band4kHz        = 0.0f;
  s.eq.band12kHz       = 0.0f;
  s.autoTuneEnabled    = true;
  s.resamplingEnabled  = true;
  s.softLimiterEnabled = true; // Enable limiter by default to prevent clipping.
  s.timezone           = "Europe/Moscow";
  s.timezoneOffset     = 10800; // UTC+3 (Moscow).
}

bool settingsSaveToSD()
{
  sanitize(g_settings);

  JsonDocument doc;
  doc["volume"]             = g_settings.volume;
  doc["softGain"]           = g_settings.softGain;
  doc["sampleRate"]         = g_settings.sampleRate;
  doc["inBufBytes"]         = g_settings.inBufBytes;
  doc["dmaBufCount"]        = g_settings.dmaBufCount;
  doc["dmaBufLen"]          = g_settings.dmaBufLen;
  doc["currentFile"]        = g_settings.currentFile;
  doc["eqEnabled"]          = g_settings.eqEnabled;
  doc["autoTuneEnabled"]    = g_settings.autoTuneEnabled;
  doc["resamplingEnabled"]  = g_settings.resamplingEnabled;
  doc["softLimiterEnabled"] = g_settings.softLimiterEnabled;
  doc["timezone"]           = g_settings.timezone;
  doc["timezoneOffset"]     = g_settings.timezoneOffset;

  JsonObject eq   = doc["eq"].to<JsonObject>();
  eq["band60Hz"]  = g_settings.eq.band60Hz;
  eq["band250Hz"] = g_settings.eq.band250Hz;
  eq["band1kHz"]  = g_settings.eq.band1kHz;
  eq["band4kHz"]  = g_settings.eq.band4kHz;
  eq["band12kHz"] = g_settings.eq.band12kHz;

  if (SD.exists(TMP_PATH))
    SD.remove(TMP_PATH);

  File f = SD.open(TMP_PATH, FILE_WRITE);
  if (!f) {
    WebLog.println("[SET] ❌ Cannot open settings.tmp for write");
    return false;
  }

  if (serializeJsonPretty(doc, f) == 0) {
    WebLog.println("[SET] ❌ Failed to write JSON");
    f.close();
    SD.remove(TMP_PATH);
    return false;
  }

  f.flush();
  f.close();

  if (SD.exists(SETTINGS_PATH))
    SD.remove(SETTINGS_PATH);

  if (!SD.rename(TMP_PATH, SETTINGS_PATH)) {
    WebLog.println("[SET] ❌ Rename settings.tmp -> settings.json failed");
    return false;
  }

  WebLog.println("[SET] ✅ Saved /settings.json");
  return true;
}

bool settingsLoadFromSD()
{
  if (!SD.exists(SETTINGS_PATH)) {
    WebLog.println("[SET] ⚠️ settings.json not found, creating defaults...");
    settingsSetDefaults(g_settings);
    return settingsSaveToSD();
  }

  File f = SD.open(SETTINGS_PATH, FILE_READ);
  if (!f) {
    WebLog.println("[SET] ❌ Cannot open settings.json for read");
    return false;
  }

  JsonDocument         doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    WebLog.print("[SET] ❌ JSON parse error: ");
    WebLog.println(err.c_str());
    WebLog.println("[SET] ⚠️ Recreating defaults...");
    settingsSetDefaults(g_settings);
    return settingsSaveToSD();
  }

  g_settings.volume             = doc["volume"] | 1.0f;
  g_settings.softGain           = doc["softGain"] | 0.0f;
  g_settings.sampleRate         = doc["sampleRate"] | 44100;
  g_settings.inBufBytes         = doc["inBufBytes"] | 8192;
  g_settings.dmaBufCount        = doc["dmaBufCount"] | 12;
  g_settings.dmaBufLen          = doc["dmaBufLen"] | 512;
  g_settings.currentFile        = doc["currentFile"] | "/test.wav";
  g_settings.eqEnabled          = doc["eqEnabled"] | false;
  g_settings.autoTuneEnabled    = doc["autoTuneEnabled"] | true;
  g_settings.resamplingEnabled  = doc["resamplingEnabled"] | true;
  g_settings.softLimiterEnabled = doc["softLimiterEnabled"] | true;
  g_settings.timezone           = doc["timezone"] | "Europe/Moscow";
  g_settings.timezoneOffset     = doc["timezoneOffset"] | 10800;

  JsonObject eq = doc["eq"];
  if (!eq.isNull()) {
    g_settings.eq.band60Hz  = eq["band60Hz"] | 0.0f;
    g_settings.eq.band250Hz = eq["band250Hz"] | 0.0f;
    g_settings.eq.band1kHz  = eq["band1kHz"] | 0.0f;
    g_settings.eq.band4kHz  = eq["band4kHz"] | 0.0f;
    g_settings.eq.band12kHz = eq["band12kHz"] | 0.0f;
  } else {
    g_settings.eq.band60Hz  = 0.0f;
    g_settings.eq.band250Hz = 0.0f;
    g_settings.eq.band1kHz  = 0.0f;
    g_settings.eq.band4kHz  = 0.0f;
    g_settings.eq.band12kHz = 0.0f;
  }

  sanitize(g_settings);

  WebLog.println("[SET] ✅ Loaded /settings.json");
  WebLog.print("[SET] volume=");
  WebLog.println(g_settings.volume, 3);
  WebLog.print("[SET] sampleRate=");
  WebLog.println(g_settings.sampleRate);
  WebLog.print("[SET] currentFile=");
  WebLog.println(g_settings.currentFile);

  return true;
}
