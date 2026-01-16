#include "ntp_time.h"

#include "settings.h"

#include <WiFi.h>
#include <time.h>

// NTP servers.
static const char* NTP_SERVER1 = "pool.ntp.org";
static const char* NTP_SERVER2 = "time.google.com";
static const char* NTP_SERVER3 = "time.cloudflare.com";

static bool g_ntpSynced      = false;
static int  g_timezoneOffset = 10800; // Default: UTC+3 (Moscow).

void ntpInit()
{
  // Get timezone offset from settings.
  g_timezoneOffset = g_settings.timezoneOffset;

  // Configure time with timezone offset.
  // This starts async NTP sync in background.
  configTime(g_timezoneOffset, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

  Serial.print("[NTP] Syncing time (UTC");
  if (g_timezoneOffset >= 0)
    Serial.print("+");
  Serial.print(g_timezoneOffset / 3600);
  Serial.println(")...");

  // Quick check if time is already available (max 3 seconds total).
  struct tm timeinfo;
  for (int i = 0; i < 6; i++) {
    if (getLocalTime(&timeinfo, 100)) { // 100ms timeout per call.
      g_ntpSynced = true;
      Serial.print("[NTP] ✅ Time synced: ");
      Serial.println(ntpGetDateTimeStr());
      return;
    }
    delay(400); // Total: 6 * 500ms = 3 sec max.
  }

  // Not synced yet, but NTP will continue in background.
  Serial.println("[NTP] ⏳ Sync pending (will complete in background)");
}

void ntpSetTimezoneOffset(int offsetSeconds)
{
  g_timezoneOffset = offsetSeconds;
  configTime(g_timezoneOffset, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

  Serial.print("[NTP] Timezone updated: UTC");
  if (g_timezoneOffset >= 0)
    Serial.print("+");
  Serial.println(g_timezoneOffset / 3600);
}

bool ntpIsSynced()
{
  // Check if we got time sync.
  if (!g_ntpSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) { // Very short timeout.
      g_ntpSynced = true;
    }
  }
  return g_ntpSynced;
}

String ntpGetTimeStr()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) { // 10ms timeout.
    // Fallback to uptime.
    unsigned long ms  = millis();
    unsigned long sec = ms / 1000;
    unsigned long min = sec / 60;
    unsigned long hr  = min / 60;
    min               = min % 60;
    sec               = sec % 60;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hr, min, sec);
    return String(buf);
  }

  g_ntpSynced = true; // Mark as synced if we got time.
  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

String ntpGetDateTimeStr()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    return ntpGetTimeStr();
  }

  g_ntpSynced = true;
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

time_t ntpGetTime()
{
  time_t now;
  time(&now);
  return now;
}

String ntpGetLogTimestamp()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) { // 10ms timeout - non-blocking.
    // Fallback to uptime format [MM:SS.mmm].
    unsigned long ms  = millis();
    unsigned long sec = ms / 1000;
    unsigned long min = sec / 60;
    sec               = sec % 60;
    ms                = ms % 1000;

    char buf[16];
    snprintf(buf, sizeof(buf), "[%02lu:%02lu.%03lu]", min, sec, ms);
    return String(buf);
  }

  g_ntpSynced = true;
  // Real time format [HH:MM:SS].
  char buf[16];
  strftime(buf, sizeof(buf), "[%H:%M:%S]", &timeinfo);
  return String(buf);
}
