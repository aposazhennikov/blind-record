#pragma once
#include <Arduino.h>

// NTP Time module.
// Synchronizes time from NTP server and provides formatted timestamps.

// Initialize NTP client and sync time.
// Call after Wi-Fi is connected.
void ntpInit();

// Update timezone offset (in seconds from UTC).
void ntpSetTimezoneOffset(int offsetSeconds);

// Check if time has been synchronized.
bool ntpIsSynced();

// Get current timestamp formatted as "HH:MM:SS".
String ntpGetTimeStr();

// Get current timestamp formatted as "YYYY-MM-DD HH:MM:SS".
String ntpGetDateTimeStr();

// Get current time in seconds since epoch (Unix timestamp).
time_t ntpGetTime();

// Get formatted timestamp for logs: "[HH:MM:SS]"
String ntpGetLogTimestamp();
