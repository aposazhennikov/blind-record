#include "audio_progress.h"

#include "web_log.h"

AudioProgressInfo g_audioProgress;

void progressInit()
{
  g_audioProgress.totalBytes    = 0;
  g_audioProgress.playedBytes   = 0;
  g_audioProgress.totalMs       = 0;
  g_audioProgress.playedMs      = 0;
  g_audioProgress.percent       = 0;
  g_audioProgress.playing       = false;
  g_audioProgress.fileName      = "";
  g_audioProgress.sampleRate    = 0;
  g_audioProgress.channels      = 0;
  g_audioProgress.bitsPerSample = 0;
}

uint32_t progressBytesToMs(uint32_t bytes, uint32_t sampleRate, uint16_t channels,
                           uint16_t bitsPerSample)
{
  if (sampleRate == 0 || channels == 0 || bitsPerSample == 0)
    return 0;

  uint32_t bytesPerSample = bitsPerSample / 8;
  uint32_t bytesPerSecond = sampleRate * channels * bytesPerSample;
  if (bytesPerSecond == 0)
    return 0;

  return (uint32_t)((uint64_t)bytes * 1000 / bytesPerSecond);
}

void progressReset(const String& fileName, uint32_t totalBytes, uint32_t sampleRate,
                   uint16_t channels, uint16_t bitsPerSample)
{
  g_audioProgress.fileName      = fileName;
  g_audioProgress.totalBytes    = totalBytes;
  g_audioProgress.playedBytes   = 0;
  g_audioProgress.sampleRate    = sampleRate;
  g_audioProgress.channels      = channels;
  g_audioProgress.bitsPerSample = bitsPerSample;
  g_audioProgress.playing       = true;
  g_audioProgress.percent       = 0;

  g_audioProgress.totalMs  = progressBytesToMs(totalBytes, sampleRate, channels, bitsPerSample);
  g_audioProgress.playedMs = 0;

  WebLog.print("[PROGRESS] Started: ");
  WebLog.print(fileName);
  WebLog.print(" (");
  WebLog.print(g_audioProgress.totalMs / 1000);
  WebLog.println(" sec)");
}

void progressUpdate(uint32_t playedBytes)
{
  g_audioProgress.playedBytes = playedBytes;
  g_audioProgress.playedMs =
      progressBytesToMs(playedBytes, g_audioProgress.sampleRate, g_audioProgress.channels,
                        g_audioProgress.bitsPerSample);

  if (g_audioProgress.totalBytes > 0) {
    g_audioProgress.percent = (uint8_t)((uint64_t)playedBytes * 100 / g_audioProgress.totalBytes);
    if (g_audioProgress.percent > 100)
      g_audioProgress.percent = 100;
  } else {
    g_audioProgress.percent = 0;
  }
}

void progressStop()
{
  g_audioProgress.playing = false;
  WebLog.println("[PROGRESS] Stopped");
}

static String formatTime(uint32_t ms)
{
  uint32_t sec = ms / 1000;
  uint32_t min = sec / 60;
  sec          = sec % 60;

  String s = "";
  if (min < 10)
    s += "0";
  s += String(min);
  s += ":";
  if (sec < 10)
    s += "0";
  s += String(sec);
  return s;
}

String progressGetJson()
{
  String json = "{";
  json += "\"playing\":" + String(g_audioProgress.playing ? "true" : "false") + ",";
  json += "\"percent\":" + String(g_audioProgress.percent) + ",";
  json += "\"playedMs\":" + String(g_audioProgress.playedMs) + ",";
  json += "\"totalMs\":" + String(g_audioProgress.totalMs) + ",";
  json += "\"playedTime\":\"" + formatTime(g_audioProgress.playedMs) + "\",";
  json += "\"totalTime\":\"" + formatTime(g_audioProgress.totalMs) + "\",";
  json += "\"fileName\":\"" + g_audioProgress.fileName + "\",";
  json += "\"sampleRate\":" + String(g_audioProgress.sampleRate) + ",";
  json += "\"channels\":" + String(g_audioProgress.channels) + ",";
  json += "\"bitsPerSample\":" + String(g_audioProgress.bitsPerSample);
  json += "}";
  return json;
}
