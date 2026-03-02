#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#include "src/core/wifiManager.h"
#include "src/core/deviceState.h"
#include "src/hardware/cameraManager.h"
#include "src/hardware/motorManager.h" // ADDED: Motor Header
#include "src/hardware/sensorManager.h"

WebServer server(80);
Preferences prefs;

/* =========================
   STEP 1 : WIFI SETUP (AP)
   ========================= */

void handleRoot() {
  String html = "<h2>Pawme Control Panel</h2>";
  if (WiFi.status() == WL_CONNECTED) {
    html += "<div style='margin-bottom: 20px; font-family:sans-serif;'>";
    
    // ADDED: Sensor Dashboard
    html += "<div style='background:#f1f1f1; padding:10px; border-radius:10px; margin-bottom:10px; border:1px solid #ccc;'>";
    html += "Temp: <b><span id='temp'>--</span>°C</b> | ";
    html += "Distance: <b><span id='dist'>--</span>cm</b>";
    html += "</div>";

    html += "<h3>Live Feed</h3>";
    html += "<img src='http://pawme.local:81/stream' style='width:320px;'>";
    
    html += "<div style='margin-top: 15px;'>";
    html += "<button onmousedown=\"fetch('/move?dir=forward')\" ontouchstart=\"fetch('/move?dir=forward')\" onmouseup=\"fetch('/move?dir=stop')\" ontouchend=\"fetch('/move?dir=stop')\">Forward</button><br><br>";
    html += "<button onmousedown=\"fetch('/move?dir=left')\" ontouchstart=\"fetch('/move?dir=left')\" onmouseup=\"fetch('/move?dir=stop')\" ontouchend=\"fetch('/move?dir=stop')\">Left</button> ";
    html += "<button onmousedown=\"fetch('/move?dir=right')\" ontouchstart=\"fetch('/move?dir=right')\" onmouseup=\"fetch('/move?dir=stop')\" ontouchend=\"fetch('/move?dir=stop')\">Right</button><br><br>";
    html += "<button onmousedown=\"fetch('/move?dir=backward')\" ontouchstart=\"fetch('/move?dir=backward')\" onmouseup=\"fetch('/move?dir=stop')\" ontouchend=\"fetch('/move?dir=stop')\">Backward</button>";
    html += "</div>";

    // ADDED: Auto-Refresh Script
    html += "<script>";
    html += "setInterval(function(){";
    html += "  fetch('/status').then(r=>r.json()).then(d=>{";
    html += "    document.getElementById('temp').innerHTML=d.temp;";
    html += "    document.getElementById('dist').innerHTML=d.dist;";
    html += "  });";
    html += "}, 2000);";
    html += "</script>";

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

// ADDED: Movement Web Handler
void handleMove() {
  String dir = server.arg("dir");
  if (dir == "forward") moveForward();
  else if (dir == "backward") moveBackward();
  else if (dir == "left") turnLeft();
  else if (dir == "right") turnRight();
  else stopMotors();
  server.send(200, "text/plain", "OK");
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

  motorsInit(); 
  sensorsInit(); // <--- ADD THIS
  wifiInit();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus); // <--- ADD THIS
  server.on("/wifi", HTTP_POST, handleWifiSave);
  server.on("/move", HTTP_GET, handleMove); 
  server.begin();
}

void loop() {
  wifiLoop();

  static unsigned long lastSensorUpdate = 0;
  if (millis() - lastSensorUpdate > 2000) {
    updateSensors(); 
    lastSensorUpdate = millis();
  }

  static bool otaStarted = false;
  static bool mdnsStarted = false;
  static bool cameraStarted = false;

  /* ===== OTA ===== */
  if (deviceState == WIFI_CONNECTED && !otaStarted) {
    setupOTA();
    otaStarted = true;
    
    // ADDED: Turn off Setup AP once home WiFi is active
    WiFi.softAPdisconnect(true); 
    WiFi.mode(WIFI_STA);
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
  
  if(mdnsStarted) {
    // Optional: some ESP32 cores require explicit MDNS update calls
    // MDNS.update(); 
  }
}
void handleStatus() {
  String json = "{";
  json += "\"temp\":" + String(currentSensors.temperature, 1) + ",";
  json += "\"dist\":" + String(currentSensors.distance);
  json += "}";
  server.send(200, "application/json", json);
}
