#include <Arduino.h>
#include <WiFi.h>
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
  String html = "<h2>Pawme WiFi Setup</h2>";
  if (WiFi.status() == WL_CONNECTED) {
    html += "<div style='margin-bottom: 20px;'>";
    html += "<h3>Live Feed</h3>";
    // Using the mDNS name in the browser view as well
    html += "<img src='http://pawme.local:81/stream' style='width:320px;'>";
    html += "</div>";
  }
  html += "<form method='POST' action='/wifi'>";
  html += "SSID:<br><input name='ssid'><br>";
  html += "Password:<br><input name='pass' type='password'><br><br>";
  html += "<button>Save</button></form>";
  
  server.send(200, "text/html", html);
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
  
  // Clean shutdown for network stability
  server.client().stop(); 
  delay(2000); 
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

  /* ===== mDNS SERVICE DISCOVERY ===== */
  if (deviceState == WIFI_CONNECTED && !mdnsStarted) {
    // Start mDNS responder for pawme.local
    if (MDNS.begin("pawme")) {
      Serial.println("[mDNS] Started: http://pawme.local");
      
      // Advertise the main web server (Port 80)
      MDNS.addService("http", "tcp", 80);
      
      // Advertise the camera stream specifically (Port 81)
      // The app will look for "_pawme-cam._tcp"
      MDNS.addService("pawme-cam", "tcp", 81);
      
      mdnsStarted = true;
    }
  }

  /* ===== CAMERA (START ONCE, STA ONLY) ===== */
  if (deviceState == WIFI_CONNECTED && !cameraStarted) {
    cameraRegisterStream();  // Starts the secondary server on port 81
    cameraStarted = true;
  }

  server.handleClient();
  
  // Allow mDNS to process background tasks
  if(mdnsStarted) {
    // Optional: some ESP32 cores require explicit MDNS update calls
    // MDNS.update(); 
  }
}
