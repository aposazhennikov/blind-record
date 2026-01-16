#include "auto_tuner.h"

#include "settings.h"
#include "web_log.h"

TunerStats g_tunerStats;

static const uint32_t TUNE_INTERVAL_MS    = 5000;
static const uint32_t UNDERRUN_THRESHOLD  = 3;
static const uint32_t CHUNKS_BEFORE_CHECK = 100;

void tunerInit()
{
  g_tunerStats.underrunCount = 0;
  g_tunerStats.totalChunks   = 0;
  g_tunerStats.lastTuneTime  = 0;
  g_tunerStats.tuningEnabled = true;

  WebLog.println("[TUNER] ✅ Auto-tuner initialized");
}

void tunerSetEnabled(bool enabled)
{
  g_tunerStats.tuningEnabled = enabled;
  WebLog.print("[TUNER] Auto-tuning ");
  WebLog.println(enabled ? "enabled" : "disabled");
}

void tunerResetStats()
{
  g_tunerStats.underrunCount = 0;
  g_tunerStats.totalChunks   = 0;
}

void tunerReportUnderrun()
{
  g_tunerStats.underrunCount++;
  g_tunerStats.totalChunks++;
}

void tunerReportSuccess()
{
  g_tunerStats.totalChunks++;
}

bool tunerCheck()
{
  if (!g_tunerStats.tuningEnabled)
    return false;
  if (g_tunerStats.totalChunks < CHUNKS_BEFORE_CHECK)
    return false;

  uint32_t now = millis();
  if (now - g_tunerStats.lastTuneTime < TUNE_INTERVAL_MS)
    return false;

  if (g_tunerStats.underrunCount < UNDERRUN_THRESHOLD) {
    tunerResetStats();
    return false;
  }

  WebLog.println("[TUNER] 🔧 Performance issues detected, adjusting buffers...");

  bool changed = false;

  if (g_settings.inBufBytes < 8192) {
    int newVal = g_settings.inBufBytes + 1024;
    if (newVal > 8192)
      newVal = 8192;
    WebLog.print("[TUNER] inBufBytes: ");
    WebLog.print(g_settings.inBufBytes);
    WebLog.print(" -> ");
    WebLog.println(newVal);
    g_settings.inBufBytes = newVal;
    changed               = true;
  }

  if (g_settings.dmaBufCount < 16 && !changed) {
    int newVal = g_settings.dmaBufCount + 2;
    if (newVal > 16)
      newVal = 16;
    WebLog.print("[TUNER] dmaBufCount: ");
    WebLog.print(g_settings.dmaBufCount);
    WebLog.print(" -> ");
    WebLog.println(newVal);
    g_settings.dmaBufCount = newVal;
    changed                = true;
  }

  if (g_settings.dmaBufLen < 1024 && !changed) {
    int newVal = g_settings.dmaBufLen + 128;
    if (newVal > 1024)
      newVal = 1024;
    WebLog.print("[TUNER] dmaBufLen: ");
    WebLog.print(g_settings.dmaBufLen);
    WebLog.print(" -> ");
    WebLog.println(newVal);
    g_settings.dmaBufLen = newVal;
    changed              = true;
  }

  if (changed) {
    if (settingsSaveToSD()) {
      WebLog.println("[TUNER] ✅ New settings saved");
    } else {
      WebLog.println("[TUNER] ❌ Failed to save settings");
    }

    g_tunerStats.lastTuneTime = now;
    tunerResetStats();

    WebLog.println("[TUNER] ⚠️ Restart audio to apply new buffer settings");
  } else {
    WebLog.println("[TUNER] ⚠️ Buffers at maximum, cannot tune further");
  }

  return changed;
}

String tunerGetStatsJson()
{
  String json = "{";
  json += "\"enabled\":" + String(g_tunerStats.tuningEnabled ? "true" : "false") + ",";
  json += "\"underruns\":" + String(g_tunerStats.underrunCount) + ",";
  json += "\"chunks\":" + String(g_tunerStats.totalChunks) + ",";

  float rate = 0;
  if (g_tunerStats.totalChunks > 0) {
    rate = (float)g_tunerStats.underrunCount * 100.0f / (float)g_tunerStats.totalChunks;
  }
  json += "\"underrunRate\":" + String(rate, 2);
  json += "}";
  return json;
}
