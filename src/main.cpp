#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "esp_camera.h"

// =============================================================================
// XIAO ESP32S3 Sense Camera Pin Definitions
// =============================================================================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// =============================================================================
// Configuration
// =============================================================================
#define AP_SSID "PawMe-Camera"
#define AP_PASSWORD "pawme123"
#define DNS_PORT 53
#define HTTP_PORT 80

// =============================================================================
// Global Objects
// =============================================================================
DNSServer dnsServer;
AsyncWebServer server(HTTP_PORT);
Preferences preferences;

bool wifiConnected = false;
String savedSSID = "";
String savedPassword = "";

// =============================================================================
// HTML Pages
// =============================================================================
const char CAPTIVE_PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PawMe Camera Setup</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            max-width: 400px;
            width: 100%;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        h1 {
            color: #333;
            text-align: center;
            margin-bottom: 10px;
            font-size: 28px;
        }
        .subtitle {
            color: #666;
            text-align: center;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            color: #555;
            margin-bottom: 8px;
            font-weight: 500;
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 14px;
            border: 2px solid #e0e0e0;
            border-radius: 10px;
            font-size: 16px;
            transition: border-color 0.3s;
        }
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        button {
            width: 100%;
            padding: 16px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 10px;
            font-size: 18px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 30px rgba(102, 126, 234, 0.4);
        }
        .camera-link {
            display: block;
            text-align: center;
            margin-top: 20px;
            color: #667eea;
            text-decoration: none;
            font-weight: 500;
        }
        .status {
            text-align: center;
            padding: 10px;
            border-radius: 8px;
            margin-bottom: 20px;
            display: none;
        }
        .status.success { background: #d4edda; color: #155724; display: block; }
        .status.error { background: #f8d7da; color: #721c24; display: block; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🐾 PawMe Camera</h1>
        <p class="subtitle">Configure WiFi or view camera stream</p>
        
        <div id="status" class="status"></div>
        
        <form action="/save" method="POST">
            <div class="form-group">
                <label for="ssid">WiFi Network Name</label>
                <input type="text" id="ssid" name="ssid" placeholder="Enter WiFi SSID" required>
            </div>
            <div class="form-group">
                <label for="password">WiFi Password</label>
                <input type="password" id="password" name="password" placeholder="Enter WiFi password">
            </div>
            <button type="submit">Save & Connect</button>
        </form>
        
        <a href="/stream" class="camera-link">📷 View Camera Stream</a>
    </div>
</body>
</html>
)rawliteral";

const char STREAM_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PawMe Camera Stream</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1a1a2e;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
        }
        h1 {
            color: white;
            margin-bottom: 20px;
            font-size: 24px;
        }
        .stream-container {
            background: #16213e;
            border-radius: 16px;
            padding: 10px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.5);
            max-width: 100%;
        }
        img {
            display: block;
            max-width: 100%;
            border-radius: 10px;
        }
        .controls {
            margin-top: 20px;
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            justify-content: center;
        }
        .btn {
            padding: 12px 24px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            cursor: pointer;
            text-decoration: none;
            transition: transform 0.2s;
        }
        .btn:hover {
            transform: translateY(-2px);
        }
        .info {
            color: #888;
            margin-top: 20px;
            font-size: 12px;
        }
    </style>
</head>
<body>
    <h1>🐾 PawMe Camera Stream</h1>
    <div class="stream-container">
        <img id="stream" src="/mjpeg" alt="Camera Stream">
    </div>
    <div class="controls">
        <a href="/" class="btn">⚙️ Settings</a>
        <button class="btn" onclick="location.reload()">🔄 Refresh</button>
    </div>
    <p class="info">Stream URL: <code>/mjpeg</code></p>
</body>
</html>
)rawliteral";

const char SUCCESS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Saved</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            text-align: center;
            max-width: 400px;
        }
        h1 { color: #28a745; margin-bottom: 20px; }
        p { color: #666; margin-bottom: 20px; }
        a { color: #667eea; }
    </style>
</head>
<body>
    <div class="container">
        <h1>✅ WiFi Saved!</h1>
        <p>The device will attempt to connect to your WiFi network.</p>
        <p>If connection fails, the Access Point will remain active.</p>
        <p><a href="/">Back to Settings</a> | <a href="/stream">View Stream</a></p>
    </div>
</body>
</html>
)rawliteral";

// =============================================================================
// Camera Functions
// =============================================================================
bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;  // 640x480
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    // Check for PSRAM
    if (psramFound()) {
        config.jpeg_quality = 10;
        config.fb_count = 2;
        config.grab_mode = CAMERA_GRAB_LATEST;
        Serial.println("PSRAM found, using higher quality settings");
    } else {
        config.frame_size = FRAMESIZE_QVGA;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.fb_count = 1;
        Serial.println("No PSRAM, using lower quality settings");
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);
        s->set_exposure_ctrl(s, 1);
        s->set_aec2(s, 0);
        s->set_gain_ctrl(s, 1);
        s->set_agc_gain(s, 0);
        s->set_gainceiling(s, (gainceiling_t)0);
        s->set_bpc(s, 0);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 1);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
        s->set_dcw(s, 1);
    }

    Serial.println("Camera initialized successfully");
    return true;
}

// =============================================================================
// MJPEG Streaming Handler
// =============================================================================
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

class MjpegStreamResponse : public AsyncAbstractResponse {
private:
    camera_fb_t *fb;
    bool firstFrame;
    
public:
    MjpegStreamResponse() {
        fb = nullptr;
        firstFrame = true;
        _code = 200;
        _contentType = STREAM_CONTENT_TYPE;
        _sendContentLength = false;
    }
    
    ~MjpegStreamResponse() {
        if (fb) {
            esp_camera_fb_return(fb);
        }
    }
    
    bool _sourceValid() const override { return true; }
    
    size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {
        size_t len = 0;
        
        if (fb) {
            esp_camera_fb_return(fb);
            fb = nullptr;
        }
        
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            return 0;
        }
        
        if (!firstFrame) {
            size_t boundaryLen = strlen(STREAM_BOUNDARY);
            if (maxLen < boundaryLen) return 0;
            memcpy(buf, STREAM_BOUNDARY, boundaryLen);
            len += boundaryLen;
        }
        firstFrame = false;
        
        char partBuf[64];
        size_t partLen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, fb->len);
        
        if (len + partLen + fb->len > maxLen) {
            // Buffer too small, just send what we can
            return len;
        }
        
        memcpy(buf + len, partBuf, partLen);
        len += partLen;
        
        memcpy(buf + len, fb->buf, fb->len);
        len += fb->len;
        
        return len;
    }
};

void handleMjpegStream(AsyncWebServerRequest *request) {
    Serial.println("Starting MJPEG stream");
    AsyncWebServerResponse *response = request->beginChunkedResponse("multipart/x-mixed-replace;boundary=" PART_BOUNDARY,
        [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                Serial.println("Camera capture failed");
                return 0;
            }
            
            size_t len = 0;
            
            // Add boundary (except for first frame)
            if (index > 0) {
                size_t boundaryLen = strlen(STREAM_BOUNDARY);
                if (boundaryLen <= maxLen) {
                    memcpy(buffer, STREAM_BOUNDARY, boundaryLen);
                    len = boundaryLen;
                }
            }
            
            // Add content type header
            char partBuf[64];
            size_t partLen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, fb->len);
            
            if (len + partLen + fb->len <= maxLen) {
                memcpy(buffer + len, partBuf, partLen);
                len += partLen;
                memcpy(buffer + len, fb->buf, fb->len);
                len += fb->len;
            }
            
            esp_camera_fb_return(fb);
            return len;
        }
    );
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

// Single frame capture
void handleCapture(AsyncWebServerRequest *request) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        request->send(500, "text/plain", "Camera capture failed");
        return;
    }
    
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/jpeg", fb->buf, fb->len);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
    esp_camera_fb_return(fb);
}

// =============================================================================
// WiFi Functions
// =============================================================================
void loadWiFiCredentials() {
    preferences.begin("pawme", true);
    savedSSID = preferences.getString("ssid", "");
    savedPassword = preferences.getString("password", "");
    preferences.end();
    
    if (savedSSID.length() > 0) {
        Serial.printf("Loaded saved WiFi: %s\n", savedSSID.c_str());
    }
}

void saveWiFiCredentials(const String& ssid, const String& password) {
    preferences.begin("pawme", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    
    savedSSID = ssid;
    savedPassword = password;
    Serial.printf("Saved WiFi credentials for: %s\n", ssid.c_str());
}

bool connectToWiFi() {
    if (savedSSID.length() == 0) {
        return false;
    }
    
    Serial.printf("Attempting to connect to WiFi: %s\n", savedSSID.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected to WiFi! IP: %s\n", WiFi.localIP().toString().c_str());
        wifiConnected = true;
        return true;
    }
    
    Serial.println("\nFailed to connect to WiFi");
    return false;
}

void startAccessPoint() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("Access Point started: %s\n", AP_SSID);
    Serial.printf("AP IP address: %s\n", apIP.toString().c_str());
    Serial.printf("Password: %s\n", AP_PASSWORD);
    
    // Start DNS server for captive portal
    dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("DNS server started for captive portal");
}

// =============================================================================
// Web Server Setup
// =============================================================================
void setupWebServer() {
    // Captive portal detection endpoints
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "success");
    });
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Microsoft NCSI");
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    
    // Main pages
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", CAPTIVE_PORTAL_HTML);
    });
    
    server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", STREAM_HTML);
    });
    
    // WiFi configuration
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
        String ssid = "";
        String password = "";
        
        if (request->hasParam("ssid", true)) {
            ssid = request->getParam("ssid", true)->value();
        }
        if (request->hasParam("password", true)) {
            password = request->getParam("password", true)->value();
        }
        
        if (ssid.length() > 0) {
            saveWiFiCredentials(ssid, password);
            request->send_P(200, "text/html", SUCCESS_HTML);
            
            // Try to connect in background
            delay(1000);
            connectToWiFi();
        } else {
            request->redirect("/");
        }
    });
    
    // Camera endpoints
    server.on("/mjpeg", HTTP_GET, handleMjpegStream);
    server.on("/capture", HTTP_GET, handleCapture);
    
    // Status endpoint
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"wifi_connected\":" + String(wifiConnected ? "true" : "false") + ",";
        json += "\"ap_ssid\":\"" + String(AP_SSID) + "\",";
        json += "\"saved_ssid\":\"" + savedSSID + "\",";
        if (wifiConnected) {
            json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
        } else {
            json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\"";
        }
        json += "}";
        request->send(200, "application/json", json);
    });
    
    // Handle all other requests (captive portal redirect)
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    
    server.begin();
    Serial.println("Web server started on port 80");
}

// =============================================================================
// Setup & Loop
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=================================");
    Serial.println("   PawMe Camera Firmware v1.0");
    Serial.println("   XIAO ESP32S3 Sense");
    Serial.println("=================================\n");
    
    // Initialize camera
    if (!initCamera()) {
        Serial.println("ERROR: Camera initialization failed!");
        Serial.println("Please check camera connection and restart.");
        while (1) {
            delay(1000);
        }
    }
    
    // Load saved WiFi credentials
    loadWiFiCredentials();
    
    // Try to connect to saved WiFi, otherwise start AP
    if (!connectToWiFi()) {
        startAccessPoint();
    }
    
    // Setup web server
    setupWebServer();
    
    Serial.println("\n=================================");
    Serial.println("   Setup Complete!");
    Serial.println("=================================");
    if (wifiConnected) {
        Serial.printf("Connected to: %s\n", savedSSID.c_str());
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("Connect to WiFi: %s\n", AP_SSID);
        Serial.printf("Password: %s\n", AP_PASSWORD);
        Serial.printf("Then open: http://%s\n", WiFi.softAPIP().toString().c_str());
    }
    Serial.println("=================================\n");
}

void loop() {
    // Process DNS requests for captive portal
    if (!wifiConnected) {
        dnsServer.processNextRequest();
    }
    
    delay(1);
}
