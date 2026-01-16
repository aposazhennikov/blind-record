#pragma once
#include <Arduino.h>

void   wifiConnectAndLog();
bool   isWiFiOk();
String ipToString();

bool checkGoogleTCP();        // Silent cached check (for status polling).
bool checkGoogleTCPWithLog(); // Force check with logging (for boot).
bool resolveGoogleDNS();
