#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#include "src/core/wifiManager.h"
#include "src/core/deviceState.h"
#include "src/hardware/cameraManager.h"

WebServer server(80);
Preferences prefs;

/* =========================
   STEP 1 : WIFI SETUP (AP)
   ========================= */

void handleRoot() {
  server.send(200, "text/html",
    "<h2>Pawme WiFi Setup</h2>"
    "<form method='POST' action='/wifi'>"
    "SSID:<br><input name='ssid'><br>"
    "Password:<br><input name='pass' type='password'><br><br>"
    "<button>Save</button>"
    "</form>"
  );
}

void handleWifiSave() {
  if (!server.hasArg("ssid") || !server.hasArg("pass")) {
    server.send(400, "text/plain", "Missing params");
    return;
  }

  prefs.begin("wifi", false);
  prefs.putString("ssid", server.arg("ssid"));
  prefs.putString("pass", server.arg("pass"));
  prefs.end();

  server.send(200, "text/plain", "Saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

/* =========================
   STEP 2 : OTA (STA ONLY)
   ========================= */

void setupOTA() {
  ArduinoOTA.setHostname("pawme");
  ArduinoOTA.begin();
}

/* =========================
   MAIN
   ========================= */

void setup() {
  Serial.begin(115200);
  delay(1000);

  wifiInit();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/wifi", HTTP_POST, handleWifiSave);
  server.begin();
}

void loop() {
  wifiLoop();

  static bool otaStarted = false;
  static bool mdnsStarted = false;
  static bool cameraStarted = false;

  /* ===== OTA ===== */
  if (deviceState == WIFI_CONNECTED && !otaStarted) {
    setupOTA();
    otaStarted = true;
  }
  if (otaStarted) {
    ArduinoOTA.handle();
  }

  /* ===== mDNS ===== */
  if (deviceState == WIFI_CONNECTED && !mdnsStarted) {
    MDNS.begin("pawme");   // pawme.local
    mdnsStarted = true;
  }

  /* ===== CAMERA (START ONCE, STA ONLY) ===== */
  if (deviceState == WIFI_CONNECTED && !cameraStarted) {
    cameraRegisterStream();  // starts HTTP server on port 81
    cameraStarted = true;
  }

  server.handleClient();
}
