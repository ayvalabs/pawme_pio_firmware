#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <Preferences.h>
#include "esp_camera.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

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
// Development mode - set to false for production (enables password)
#define DEV_MODE true

#define AP_SSID_PREFIX "PawMe-Robot-"
#define AP_PASSWORD "pawme123"  // Only used when DEV_MODE is false
#define DNS_PORT 53
#define HTTP_PORT 80
#define BLE_DEVICE_PREFIX "PawMe-Robot-"

// Dynamic SSID and BLE name with MAC suffix
String apSSID = "";
String bleName = "";

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define WIFI_LIST_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define WIFI_CONFIG_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define STATUS_UUID         "beb5483e-36e1-4688-b7f5-ea07361b26aa"

// =============================================================================
// Global Objects
// =============================================================================
DNSServer dnsServer;
AsyncWebServer server(HTTP_PORT);
Preferences preferences;

// BLE objects
BLEServer *pServer = nullptr;
BLECharacteristic *pWifiListChar = nullptr;
BLECharacteristic *pWifiConfigChar = nullptr;
BLECharacteristic *pStatusChar = nullptr;
bool bleDeviceConnected = false;
bool oldBleDeviceConnected = false;

bool wifiConnected = false;
String savedSSID = "";
String savedPassword = "";

// Get last 4 hex digits of MAC address
String getMacSuffix() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    return String(suffix);
}

// Forward declaration - initDeviceNames defined after logMessage

// WiFi scan results
String wifiNetworksJson = "[]";
unsigned long lastWifiScan = 0;
#define WIFI_SCAN_INTERVAL 30000

// Console log buffer (circular buffer for web console)
#define CONSOLE_BUFFER_SIZE 4096
char consoleBuffer[CONSOLE_BUFFER_SIZE];
int consoleBufferHead = 0;
int consoleBufferTail = 0;

// Forward declarations
void logMessage(const char* format, ...);
void scanWiFiNetworks();
bool connectToWiFi();

// =============================================================================
// Console Logging (dual output: Serial + Web buffer)
// =============================================================================
void addToConsoleBuffer(const char* msg) {
    int len = strlen(msg);
    for (int i = 0; i < len; i++) {
        consoleBuffer[consoleBufferHead] = msg[i];
        consoleBufferHead = (consoleBufferHead + 1) % CONSOLE_BUFFER_SIZE;
        if (consoleBufferHead == consoleBufferTail) {
            consoleBufferTail = (consoleBufferTail + 1) % CONSOLE_BUFFER_SIZE;
        }
    }
}

void logMessage(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Output to Serial
    Serial.print(buffer);
    
    // Add to web console buffer
    addToConsoleBuffer(buffer);
}

String getConsoleBuffer() {
    String result = "";
    int i = consoleBufferTail;
    while (i != consoleBufferHead) {
        result += consoleBuffer[i];
        i = (i + 1) % CONSOLE_BUFFER_SIZE;
    }
    return result;
}

void initDeviceNames() {
    String macSuffix = getMacSuffix();
    apSSID = String(AP_SSID_PREFIX) + macSuffix;
    bleName = String(BLE_DEVICE_PREFIX) + macSuffix;
    logMessage("Device MAC suffix: %s\n", macSuffix.c_str());
    logMessage("AP SSID: %s\n", apSSID.c_str());
    logMessage("BLE Name: %s\n", bleName.c_str());
}

// =============================================================================
// HTML Pages
// =============================================================================
const char CAPTIVE_PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PawMe Robot</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #f5f5f5;
            min-height: 100vh;
        }
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 16px 24px;
            background: white;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
        .app-icon { height: 40px; width: auto; }
        .logo-text { height: 32px; width: auto; }
        .container {
            max-width: 480px;
            margin: 0 auto;
            padding: 20px;
        }
        .camera-feed {
            width: 100%;
            border-radius: 12px;
            background: #1a1a2e;
            aspect-ratio: 4/3;
            object-fit: cover;
            margin-bottom: 20px;
        }
        .wifi-list {
            background: white;
            border-radius: 12px;
            overflow: hidden;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
        .wifi-header {
            padding: 16px;
            font-weight: 600;
            color: #333;
            border-bottom: 1px solid #eee;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .refresh-btn {
            background: none;
            border: none;
            font-size: 18px;
            cursor: pointer;
            padding: 4px 8px;
        }
        .wifi-item {
            padding: 14px 16px;
            border-bottom: 1px solid #f0f0f0;
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
            transition: background 0.2s;
        }
        .wifi-item:hover { background: #f8f9fa; }
        .wifi-item:last-child { border-bottom: none; }
        .wifi-name { font-weight: 500; color: #333; }
        .wifi-info {
            display: flex;
            align-items: center;
            gap: 8px;
            color: #666;
            font-size: 13px;
        }
        .lock-icon { font-size: 12px; }
        .signal-strong { color: #28a745; }
        .signal-medium { color: #ffc107; }
        .signal-weak { color: #dc3545; }
        
        /* Modal styles */
        .modal-overlay {
            display: none;
            position: fixed;
            top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(0,0,0,0.5);
            z-index: 100;
            align-items: center;
            justify-content: center;
        }
        .modal-overlay.active { display: flex; }
        .modal {
            background: white;
            border-radius: 16px;
            width: 90%;
            max-width: 360px;
            padding: 24px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
        }
        .modal h3 {
            margin-bottom: 8px;
            color: #333;
        }
        .modal-ssid {
            color: #666;
            font-size: 14px;
            margin-bottom: 20px;
        }
        .form-group { margin-bottom: 16px; }
        .form-group label {
            display: block;
            color: #555;
            margin-bottom: 6px;
            font-weight: 500;
            font-size: 14px;
        }
        .form-group input {
            width: 100%;
            padding: 12px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-size: 16px;
        }
        .form-group input:focus {
            outline: none;
            border-color: #667eea;
        }
        .modal-buttons {
            display: flex;
            gap: 12px;
            margin-top: 20px;
        }
        .btn {
            flex: 1;
            padding: 14px;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
        }
        .btn-cancel {
            background: #e0e0e0;
            color: #333;
        }
        .btn-connect {
            background: #667eea;
            color: white;
        }
        .btn:disabled {
            opacity: 0.6;
            cursor: not-allowed;
        }
        .scanning {
            padding: 40px;
            text-align: center;
            color: #666;
        }
    </style>
</head>
<body>
    <div class="header">
        <img class="app-icon" src="/app-icon.png" alt="PawMe" onerror="this.style.display='none'">
        <img class="logo-text" src="/logo-text.png" alt="PawMe Robot" onerror="this.outerHTML='<span style=font-weight:600;color:#333>PawMe Robot</span>'">
    </div>
    
    <div class="container">
        <img class="camera-feed" id="cameraFeed" src="/mjpeg" alt="Camera Feed" onerror="this.src='/capture'">
        
        <div class="wifi-list">
            <div class="wifi-header">
                <span>WiFi Networks</span>
                <button class="refresh-btn" onclick="scanWifi()" title="Refresh">&#x21bb;</button>
            </div>
            <div id="wifiList">
                <div class="scanning">Scanning...</div>
            </div>
        </div>
    </div>
    
    <!-- Password Modal -->
    <div class="modal-overlay" id="modalOverlay">
        <div class="modal">
            <h3>Connect to WiFi</h3>
            <div class="modal-ssid" id="modalSsid"></div>
            <form id="wifiForm" onsubmit="return connectWifi(event)">
                <div class="form-group">
                    <label for="password">Password</label>
                    <input type="password" id="password" name="password" placeholder="Enter WiFi password" autocomplete="off">
                </div>
                <div class="modal-buttons">
                    <button type="button" class="btn btn-cancel" onclick="closeModal()">Cancel</button>
                    <button type="submit" class="btn btn-connect" id="connectBtn">Connect</button>
                </div>
            </form>
        </div>
    </div>
    
    <script>
        let selectedNetwork = null;
        
        function getSignalClass(rssi) {
            if (rssi > -50) return 'signal-strong';
            if (rssi > -70) return 'signal-medium';
            return 'signal-weak';
        }
        
        function getSignalBars(rssi) {
            if (rssi > -50) return '\u2582\u2584\u2586\u2588';
            if (rssi > -60) return '\u2582\u2584\u2586';
            if (rssi > -70) return '\u2582\u2584';
            return '\u2582';
        }
        
        async function scanWifi() {
            document.getElementById('wifiList').innerHTML = '<div class="scanning">Scanning...</div>';
            try {
                const response = await fetch('/api/wifi/scan');
                const networks = await response.json();
                renderWifiList(networks);
            } catch (e) {
                document.getElementById('wifiList').innerHTML = '<div class="scanning" style="color:#dc3545">Scan failed. Tap to retry.</div>';
            }
        }
        
        function renderWifiList(networks) {
            const list = document.getElementById('wifiList');
            if (networks.length === 0) {
                list.innerHTML = '<div class="scanning">No networks found</div>';
                return;
            }
            list.innerHTML = networks.map(n => `
                <div class="wifi-item" onclick="selectWifi('${n.ssid.replace(/'/g, "\\'")}', ${n.secure})">
                    <span class="wifi-name">${n.ssid}</span>
                    <span class="wifi-info">
                        ${n.secure ? '<span class="lock-icon">\u{1F512}</span>' : ''}
                        <span class="${getSignalClass(n.rssi)}">${getSignalBars(n.rssi)}</span>
                    </span>
                </div>
            `).join('');
        }
        
        function selectWifi(ssid, secure) {
            selectedNetwork = { ssid, secure };
            document.getElementById('modalSsid').textContent = ssid;
            document.getElementById('password').value = '';
            document.getElementById('modalOverlay').classList.add('active');
            if (!secure) {
                document.getElementById('password').placeholder = 'No password required';
            } else {
                document.getElementById('password').placeholder = 'Enter WiFi password';
                document.getElementById('password').focus();
            }
        }
        
        function closeModal() {
            document.getElementById('modalOverlay').classList.remove('active');
            selectedNetwork = null;
        }
        
        async function connectWifi(e) {
            e.preventDefault();
            if (!selectedNetwork) return false;
            
            const btn = document.getElementById('connectBtn');
            btn.disabled = true;
            btn.textContent = 'Connecting...';
            
            try {
                const response = await fetch('/api/wifi/connect', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        ssid: selectedNetwork.ssid,
                        password: document.getElementById('password').value
                    })
                });
                const result = await response.json();
                if (result.success) {
                    closeModal();
                    alert('Connecting to ' + selectedNetwork.ssid + '...');
                    setTimeout(() => location.reload(), 3000);
                } else {
                    alert('Failed: ' + result.message);
                }
            } catch (e) {
                alert('Connection error');
            }
            btn.disabled = false;
            btn.textContent = 'Connect';
            return false;
        }
        
        // Close modal on overlay click
        document.getElementById('modalOverlay').addEventListener('click', function(e) {
            if (e.target === this) closeModal();
        });
        
        // Initial scan
        scanWifi();
    </script>
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
        logMessage("Loaded saved WiFi: %s\n", savedSSID.c_str());
    }
}

void saveWiFiCredentials(const String& ssid, const String& password) {
    preferences.begin("pawme", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    
    savedSSID = ssid;
    savedPassword = password;
    logMessage("Saved WiFi credentials for: %s\n", ssid.c_str());
}

void scanWiFiNetworks() {
    logMessage("Scanning WiFi networks...\n");
    
    // Use AP+STA mode to scan while maintaining AP
    WiFi.mode(WIFI_AP_STA);
    
    int n = WiFi.scanNetworks();
    logMessage("Found %d networks\n", n);
    
    JsonDocument doc;
    JsonArray networks = doc.to<JsonArray>();
    
    for (int i = 0; i < n; i++) {
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        network["channel"] = WiFi.channel(i);
    }
    
    serializeJson(doc, wifiNetworksJson);
    lastWifiScan = millis();
    
    WiFi.scanDelete();
}

bool connectToWiFi() {
    if (savedSSID.length() == 0) {
        return false;
    }
    
    logMessage("Attempting to connect to WiFi: %s\n", savedSSID.c_str());
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        logMessage(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        logMessage("\nConnected to WiFi! IP: %s\n", WiFi.localIP().toString().c_str());
        wifiConnected = true;
        
        // Update BLE status characteristic if available
        if (pStatusChar) {
            String status = "{\"wifi\":true,\"ip\":\"" + WiFi.localIP().toString() + "\"}";
            pStatusChar->setValue(status.c_str());
            pStatusChar->notify();
        }
        return true;
    }
    
    logMessage("\nFailed to connect to WiFi\n");
    wifiConnected = false;
    return false;
}

void startAccessPoint() {
    WiFi.mode(WIFI_AP_STA);
    
    #if DEV_MODE
        // Development mode: No password (open network)
        WiFi.softAP(apSSID.c_str());
        logMessage("Access Point started (DEV MODE - NO PASSWORD): %s\n", apSSID.c_str());
    #else
        // Production mode: With password
        WiFi.softAP(apSSID.c_str(), AP_PASSWORD);
        logMessage("Access Point started: %s\n", apSSID.c_str());
        logMessage("Password: %s\n", AP_PASSWORD);
    #endif
    
    IPAddress apIP = WiFi.softAPIP();
    logMessage("AP IP address: %s\n", apIP.toString().c_str());
    
    // Start DNS server for captive portal
    dnsServer.start(DNS_PORT, "*", apIP);
    logMessage("DNS server started for captive portal\n");
}

// =============================================================================
// BLE Callbacks and Setup
// =============================================================================
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        bleDeviceConnected = true;
        logMessage("BLE client connected\n");
    }
    
    void onDisconnect(BLEServer* pServer) {
        bleDeviceConnected = false;
        logMessage("BLE client disconnected\n");
        // Restart advertising
        pServer->startAdvertising();
    }
};

class WiFiConfigCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            logMessage("BLE WiFi config received: %s\n", value.c_str());
            
            // Parse JSON: {"ssid":"xxx","password":"xxx"}
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, value.c_str());
            
            if (!error) {
                String ssid = doc["ssid"] | "";
                String password = doc["password"] | "";
                
                if (ssid.length() > 0) {
                    saveWiFiCredentials(ssid, password);
                    
                    // Notify status
                    if (pStatusChar) {
                        pStatusChar->setValue("{\"status\":\"connecting\"}");
                        pStatusChar->notify();
                    }
                    
                    // Try to connect
                    if (connectToWiFi()) {
                        if (pStatusChar) {
                            String status = "{\"status\":\"connected\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
                            pStatusChar->setValue(status.c_str());
                            pStatusChar->notify();
                        }
                    } else {
                        if (pStatusChar) {
                            pStatusChar->setValue("{\"status\":\"failed\"}");
                            pStatusChar->notify();
                        }
                    }
                }
            }
        }
    }
};

void setupBLE() {
    logMessage("Initializing BLE...\n");
    
    BLEDevice::init(bleName.c_str());
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    // WiFi List Characteristic (Read)
    pWifiListChar = pService->createCharacteristic(
        WIFI_LIST_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    pWifiListChar->setValue("[]");
    
    // WiFi Config Characteristic (Write)
    pWifiConfigChar = pService->createCharacteristic(
        WIFI_CONFIG_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pWifiConfigChar->setCallbacks(new WiFiConfigCallbacks());
    
    // Status Characteristic (Read + Notify)
    pStatusChar = pService->createCharacteristic(
        STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pStatusChar->addDescriptor(new BLE2902());
    pStatusChar->setValue("{\"status\":\"ready\"}");
    
    pService->start();
    
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    logMessage("BLE advertising started as: %s\n", bleName.c_str());
}

// =============================================================================
// Web Server Setup
// =============================================================================
void setupWebServer() {
    // CORS headers for all responses
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
    
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
        request->send(200, "text/html", CAPTIVE_PORTAL_HTML);
    });
    
    server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", STREAM_HTML);
    });
    
    // Legacy WiFi configuration (form POST)
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
            request->send(200, "text/html", SUCCESS_HTML);
            
            // Try to connect in background
            delay(1000);
            connectToWiFi();
        } else {
            request->redirect("/");
        }
    });
    
    // ==========================================================================
    // REST API Endpoints for Companion App
    // ==========================================================================
    
    // API: Get WiFi scan results
    server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        logMessage("API: WiFi scan requested\n");
        scanWiFiNetworks();
        request->send(200, "application/json", wifiNetworksJson);
    });
    
    // API: Get cached WiFi list (without new scan)
    server.on("/api/wifi/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", wifiNetworksJson);
    });
    
    // API: Connect to WiFi (JSON body)
    AsyncCallbackJsonWebHandler *wifiConnectHandler = new AsyncCallbackJsonWebHandler("/api/wifi/connect", 
        [](AsyncWebServerRequest *request, JsonVariant &json) {
            JsonObject jsonObj = json.as<JsonObject>();
            String ssid = jsonObj["ssid"] | "";
            String password = jsonObj["password"] | "";
            
            logMessage("API: WiFi connect request for SSID: %s\n", ssid.c_str());
            
            if (ssid.length() > 0) {
                saveWiFiCredentials(ssid, password);
                
                // Send response before attempting connection
                request->send(200, "application/json", "{\"success\":true,\"message\":\"Credentials saved, connecting...\"}");
                
                // Connect in background (after response sent)
                delay(500);
                connectToWiFi();
            } else {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"SSID is required\"}");
            }
        }
    );
    server.addHandler(wifiConnectHandler);
    
    // API: Get device status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["wifi_connected"] = wifiConnected;
        doc["ap_ssid"] = apSSID;
        #if DEV_MODE
        doc["ap_password"] = "";
        doc["dev_mode"] = true;
        #else
        doc["ap_password"] = AP_PASSWORD;
        doc["dev_mode"] = false;
        #endif
        doc["saved_ssid"] = savedSSID;
        doc["ble_connected"] = bleDeviceConnected;
        doc["ble_name"] = bleName;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["psram_free"] = ESP.getFreePsram();
        
        if (wifiConnected) {
            doc["ip"] = WiFi.localIP().toString();
            doc["rssi"] = WiFi.RSSI();
        } else {
            doc["ip"] = WiFi.softAPIP().toString();
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // API: Get console log
    server.on("/api/console", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["log"] = getConsoleBuffer();
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // API: Reboot device
    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        logMessage("API: Reboot requested\n");
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
        delay(500);
        ESP.restart();
    });
    
    // API: Clear saved WiFi credentials
    server.on("/api/wifi/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        logMessage("API: Clearing WiFi credentials\n");
        preferences.begin("pawme", false);
        preferences.clear();
        preferences.end();
        savedSSID = "";
        savedPassword = "";
        request->send(200, "application/json", "{\"success\":true,\"message\":\"WiFi credentials cleared\"}");
    });
    
    // Camera endpoints
    server.on("/mjpeg", HTTP_GET, handleMjpegStream);
    server.on("/capture", HTTP_GET, handleCapture);
    
    // API: Camera stream (alias for companion app)
    server.on("/api/stream", HTTP_GET, handleMjpegStream);
    server.on("/api/capture", HTTP_GET, handleCapture);
    
    // Legacy status endpoint (for backward compatibility)
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["wifi_connected"] = wifiConnected;
        doc["ap_ssid"] = apSSID;
        doc["saved_ssid"] = savedSSID;
        if (wifiConnected) {
            doc["ip"] = WiFi.localIP().toString();
        } else {
            doc["ip"] = WiFi.softAPIP().toString();
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Handle OPTIONS for CORS preflight
    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->redirect("/");
        }
    });
    
    server.begin();
    logMessage("Web server started on port 80\n");
}

// =============================================================================
// Setup & Loop
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    logMessage("\n\n=================================\n");
    logMessage("   PawMe-Robot Firmware v2.0\n");
    logMessage("   XIAO ESP32S3 Sense\n");
    #if DEV_MODE
    logMessage("   *** DEVELOPMENT MODE ***\n");
    #endif
    logMessage("=================================\n\n");
    
    // Initialize WiFi early to get MAC address
    WiFi.mode(WIFI_AP_STA);
    
    // Initialize device names with MAC suffix
    initDeviceNames();
    
    // Initialize camera
    if (!initCamera()) {
        logMessage("ERROR: Camera initialization failed!\n");
        logMessage("Please check camera connection and restart.\n");
        while (1) {
            delay(1000);
        }
    }
    
    // Load saved WiFi credentials
    loadWiFiCredentials();
    
    // Start Access Point (always on for configuration)
    startAccessPoint();
    
    // Try to connect to saved WiFi if available
    if (savedSSID.length() > 0) {
        connectToWiFi();
    }
    
    // Initial WiFi scan
    scanWiFiNetworks();
    
    // Setup BLE for companion app provisioning
    setupBLE();
    
    // Setup web server
    setupWebServer();
    
    logMessage("\n=================================\n");
    logMessage("   Setup Complete!\n");
    logMessage("=================================\n");
    #if DEV_MODE
    logMessage("Access Point: %s (NO PASSWORD)\n", apSSID.c_str());
    #else
    logMessage("Access Point: %s (pwd: %s)\n", apSSID.c_str(), AP_PASSWORD);
    #endif
    logMessage("AP IP: http://%s\n", WiFi.softAPIP().toString().c_str());
    if (wifiConnected) {
        logMessage("WiFi Connected: %s\n", savedSSID.c_str());
        logMessage("WiFi IP: http://%s\n", WiFi.localIP().toString().c_str());
    }
    logMessage("BLE Name: %s\n", bleName.c_str());
    logMessage("=================================\n\n");
}

void loop() {
    // Process DNS requests for captive portal
    dnsServer.processNextRequest();
    
    // Periodic WiFi scan (every 30 seconds)
    if (millis() - lastWifiScan > WIFI_SCAN_INTERVAL) {
        scanWiFiNetworks();
        
        // Update BLE WiFi list characteristic
        if (pWifiListChar) {
            pWifiListChar->setValue(wifiNetworksJson.c_str());
        }
    }
    
    // Handle BLE connection state changes
    if (!bleDeviceConnected && oldBleDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        logMessage("BLE: Restarted advertising\n");
        oldBleDeviceConnected = bleDeviceConnected;
    }
    if (bleDeviceConnected && !oldBleDeviceConnected) {
        oldBleDeviceConnected = bleDeviceConnected;
    }
    
    delay(10);
}
