#include "wifiManager.h"
#include "deviceState.h"

#include <WiFi.h>
#include <Preferences.h>

DeviceState deviceState = WIFI_CONNECTING;

static Preferences prefs;
static unsigned long connectStart = 0;

static const char* AP_SSID = "PAWME-SETUP";
static const char* AP_PASS = "pawme123"; // temp, change later

void startSetupAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  deviceState = WIFI_SETUP_MODE;

  Serial.println("[WIFI] Setup mode");
  Serial.print("[WIFI] AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void wifiInit() {
  prefs.begin("wifi", false);

  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");

  if (ssid.length() == 0) {
    startSetupAP();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  connectStart = millis();
  deviceState = WIFI_CONNECTING;

  Serial.println("[WIFI] Connecting...");
}

void wifiLoop() {
  if (deviceState == WIFI_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      deviceState = WIFI_CONNECTED;
      Serial.println("[WIFI] Connected");
      Serial.print("[WIFI] IP: ");
      Serial.println(WiFi.localIP());
    }

    if (millis() - connectStart > 15000) {
      Serial.println("[WIFI] Failed, entering setup mode");
      WiFi.disconnect(true);
      startSetupAP();
    }
  }
}


bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String wifiIP() {
  return WiFi.localIP().toString();
}
