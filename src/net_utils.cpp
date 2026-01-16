#include "net_utils.h"

#include "app_config.h"
#include "web_log.h"

#include <WiFi.h>

// Cache for Google connectivity check to avoid spamming.
static bool                g_lastGoogleStatus       = false;
static unsigned long       g_lastGoogleCheckTime    = 0;
static const unsigned long GOOGLE_CHECK_INTERVAL_MS = 30000; // Check every 30 seconds.

void wifiConnectAndLog()
{
  WebLog.println("[WIFI] Starting Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    WebLog.print(".");
    if (millis() - start > 15000) {
      WebLog.println();
      WebLog.println("[WIFI] ❌ Failed to connect (timeout)");
      return;
    }
  }
  WebLog.println();

  WebLog.println("==================================");
  WebLog.println("[WIFI] Connected!");
  WebLog.print("[WIFI] SSID: ");
  WebLog.println(WiFi.SSID());
  WebLog.print("[WIFI] IP: ");
  WebLog.println(WiFi.localIP());
  WebLog.print("[WIFI] Gateway: ");
  WebLog.println(WiFi.gatewayIP());
  WebLog.print("[WIFI] DNS: ");
  WebLog.println(WiFi.dnsIP());
  WebLog.print("[WIFI] RSSI: ");
  WebLog.print(WiFi.RSSI());
  WebLog.println(" dBm");
  WebLog.println("==================================");
}

bool isWiFiOk()
{
  return WiFi.status() == WL_CONNECTED;
}

String ipToString()
{
  return WiFi.localIP().toString();
}

bool resolveGoogleDNS()
{
  IPAddress ip;
  bool      ok = WiFi.hostByName("google.com", ip);
  WebLog.print("[DNS] google.com -> ");
  WebLog.println(ok ? ip.toString() : "FAIL");
  return ok;
}

bool checkGoogleTCP()
{
  // Use cached result if checked recently to avoid log spam.
  unsigned long now = millis();
  if (now - g_lastGoogleCheckTime < GOOGLE_CHECK_INTERVAL_MS) {
    return g_lastGoogleStatus;
  }

  WiFiClient     client;
  const char*    host = "google.com";
  const uint16_t port = 80;

  // Silent check - no logging to avoid spam.
  bool ok = client.connect(host, port, 2000); // 2 second timeout.
  client.stop();

  g_lastGoogleStatus    = ok;
  g_lastGoogleCheckTime = now;

  return ok;
}

bool checkGoogleTCPWithLog()
{
  WiFiClient     client;
  const char*    host = "google.com";
  const uint16_t port = 80;

  WebLog.print("[NET] Checking TCP connect to ");
  WebLog.print(host);
  WebLog.print(":");
  WebLog.println(port);

  bool ok = client.connect(host, port, 3000);
  client.stop();

  if (ok) {
    WebLog.println("[NET] ✅ google.com reachable (TCP connect OK)");
  } else {
    WebLog.println("[NET] ❌ google.com NOT reachable (TCP connect failed)");
  }

  g_lastGoogleStatus    = ok;
  g_lastGoogleCheckTime = millis();

  return ok;
}
