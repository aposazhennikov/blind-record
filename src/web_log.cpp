#include "web_log.h"

#include "ntp_time.h"

// Ring buffer for log messages.
static const int LOG_MAX_LINES    = 150;
static const int LOG_MAX_LINE_LEN = 250;

static String g_logBuffer[LOG_MAX_LINES];
static int    g_logHead  = 0; // Next write position.
static int    g_logCount = 0; // Number of valid entries.

// Current line being built.
static String g_currentLine = "";

// Flag to prevent recursion.
static bool g_inWrite = false;

// Global instance.
WebLogPrint WebLog;

// Format timestamp - uses NTP time if available, otherwise uptime.
static String formatTimestamp()
{
  return ntpGetLogTimestamp() + " ";
}

void webLogBegin()
{
  g_logHead     = 0;
  g_logCount    = 0;
  g_currentLine = "";
  g_currentLine.reserve(LOG_MAX_LINE_LEN);
  Serial.println("[LOG] ✅ Web log initialized");
}

void webLogAdd(const String& msg)
{
  if (msg.length() == 0)
    return;

  // Add timestamp prefix.
  String timestampedMsg = formatTimestamp() + msg;

  // Store in ring buffer.
  g_logBuffer[g_logHead] = timestampedMsg;
  g_logHead              = (g_logHead + 1) % LOG_MAX_LINES;
  if (g_logCount < LOG_MAX_LINES) {
    g_logCount++;
  }
}

void webLogClear()
{
  g_logHead  = 0;
  g_logCount = 0;
  for (int i = 0; i < LOG_MAX_LINES; i++) {
    g_logBuffer[i] = "";
  }
}

static String escapeJson(const String& s)
{
  String out;
  out.reserve(s.length() + 20);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (c >= 32 && c < 127) {
        out += c;
      }
      // Skip non-printable characters.
    }
  }
  return out;
}

String webLogGetJson()
{
  String json = webLogPeekJson();
  webLogClear();
  return json;
}

String webLogPeekJson()
{
  String json = "[";

  if (g_logCount == 0) {
    return "[]";
  }

  // Calculate start position.
  int start = (g_logHead - g_logCount + LOG_MAX_LINES) % LOG_MAX_LINES;

  for (int i = 0; i < g_logCount; i++) {
    int idx = (start + i) % LOG_MAX_LINES;
    if (i > 0)
      json += ",";
    json += "\"";
    json += escapeJson(g_logBuffer[idx]);
    json += "\"";
  }

  json += "]";
  return json;
}

// Add line directly (from external capture).
void webLogAddLine(const char* line)
{
  if (line == nullptr || line[0] == '\0')
    return;
  webLogAdd(String(line));
}

// WebLogPrint implementation - duplicates to Serial and log buffer.
size_t WebLogPrint::write(uint8_t c)
{
  // Prevent recursion.
  if (g_inWrite) {
    return Serial.write(c);
  }

  g_inWrite = true;

  // Always write to Serial.
  Serial.write(c);

  // Build line.
  if (c == '\n') {
    // Line complete, add to buffer.
    if (g_currentLine.length() > 0) {
      // Truncate if too long.
      if (g_currentLine.length() > LOG_MAX_LINE_LEN) {
        g_currentLine = g_currentLine.substring(0, LOG_MAX_LINE_LEN) + "...";
      }
      webLogAdd(g_currentLine);
    }
    g_currentLine = "";
  } else if (c != '\r') {
    if (g_currentLine.length() < LOG_MAX_LINE_LEN) {
      g_currentLine += (char)c;
    }
  }

  g_inWrite = false;
  return 1;
}

size_t WebLogPrint::write(const uint8_t* buffer, size_t size)
{
  size_t written = 0;
  for (size_t i = 0; i < size; i++) {
    written += write(buffer[i]);
  }
  return written;
}

// Serial capture - call this to add raw Serial output to log.
void webLogCaptureSerial(const char* data, size_t len)
{
  for (size_t i = 0; i < len; i++) {
    char c = data[i];
    if (c == '\n') {
      if (g_currentLine.length() > 0) {
        if (g_currentLine.length() > LOG_MAX_LINE_LEN) {
          g_currentLine = g_currentLine.substring(0, LOG_MAX_LINE_LEN) + "...";
        }
        webLogAdd(g_currentLine);
      }
      g_currentLine = "";
    } else if (c != '\r') {
      if (g_currentLine.length() < LOG_MAX_LINE_LEN) {
        g_currentLine += c;
      }
    }
  }
}
