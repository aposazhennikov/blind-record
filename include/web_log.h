#pragma once
#include <Arduino.h>

// Web Serial Log module.
// Captures Serial output to a ring buffer for web display.

// Initialize the log system (call in setup).
void webLogBegin();

// Add a log message directly.
void webLogAdd(const String& msg);

// Add a line directly (C string).
void webLogAddLine(const char* line);

// Capture raw serial data.
void webLogCaptureSerial(const char* data, size_t len);

// Get all logs as JSON array (clears buffer after read).
String webLogGetJson();

// Get logs without clearing.
String webLogPeekJson();

// Clear log buffer.
void webLogClear();

// Custom print class that duplicates to Serial and log buffer.
class WebLogPrint : public Print
{
public:
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buffer, size_t size) override;
};

// Global instance to use instead of Serial for logged output.
extern WebLogPrint WebLog;

// Macro for easy logging with tag.
#define LOG(tag, msg)                                                                              \
  do {                                                                                             \
    WebLog.print("[");                                                                             \
    WebLog.print(tag);                                                                             \
    WebLog.print("] ");                                                                            \
    WebLog.println(msg);                                                                           \
  } while (0)

#define LOG_F(tag, ...)                                                                            \
  do {                                                                                             \
    WebLog.print("[");                                                                             \
    WebLog.print(tag);                                                                             \
    WebLog.print("] ");                                                                            \
    WebLog.printf(__VA_ARGS__);                                                                    \
    WebLog.println();                                                                              \
  } while (0)
