# PawMe Robot - Developer Guide

Complete guide for developers to connect to the PawMe Robot board, configure WiFi, and access the video feed.

## Table of Contents

1. [Hardware Requirements](#hardware-requirements)
2. [Quick Start](#quick-start)
3. [Connection Methods](#connection-methods)
4. [REST API Reference](#rest-api-reference)
5. [BLE API Reference](#ble-api-reference)
6. [Companion App Integration](#companion-app-integration)
7. [Troubleshooting](#troubleshooting)

---

## Hardware Requirements

- **Board**: Seeed Studio XIAO ESP32S3 Sense (with camera module)
- **Camera**: OV2640 (included with Sense variant)
- **Power**: USB-C or 3.3V supply

---

## Quick Start

### Step 1: Power On the Board

Connect the XIAO ESP32S3 via USB-C. The board will:
1. Initialize the camera
2. Start WiFi Access Point
3. Start BLE advertising
4. Begin web server

### Step 2: Connect to the Board

**Option A: WiFi Access Point**
```
SSID: PawMe-Robot-XXXX  (XXXX = last 4 digits of MAC address)
Password: None (Development Mode) / pawme123 (Production)
```

**Option B: Bluetooth (BLE)**
```
Device Name: PawMe-Robot-XXXX
```

### Step 3: Open the Captive Portal

After connecting to WiFi AP, open a browser:
```
http://192.168.4.1
```

The captive portal should open automatically on most devices.

### Step 4: Configure WiFi (Optional)

1. Select your home WiFi network from the list
2. Enter the password
3. Click "Connect"
4. The board will connect to your WiFi while keeping the AP active

### Step 5: View Camera Feed

- **Web Browser**: `http://192.168.4.1/stream` (AP) or `http://<device-ip>/stream` (WiFi)
- **Direct MJPEG**: `http://<device-ip>/mjpeg`
- **Single Frame**: `http://<device-ip>/capture`

---

## Connection Methods

### Method 1: WiFi Access Point (Recommended for Setup)

The board always creates an Access Point for initial configuration:

| Setting | Value |
|---------|-------|
| SSID | `PawMe-Robot-XXXX` (XXXX = MAC suffix) |
| Password | None (Dev Mode) / `pawme123` (Production) |
| IP Address | `192.168.4.1` |
| Web Portal | `http://192.168.4.1` |

### Method 2: Station Mode (After WiFi Config)

Once configured, the board connects to your WiFi network:
- Check the device IP via the captive portal or serial monitor
- Access via `http://<assigned-ip>`

### Method 3: Bluetooth Low Energy (BLE)

For companion app integration:

| Setting | Value |
|---------|-------|
| Device Name | `PawMe-Robot-XXXX` |
| Service UUID | `4fafc201-1fb5-459e-8fcc-c5c9c331914b` |

---

## REST API Reference

All endpoints support CORS for cross-origin requests.

### WiFi APIs

#### GET `/api/wifi/scan`
Scan for available WiFi networks.

**Response:**
```json
[
  {
    "ssid": "MyNetwork",
    "rssi": -45,
    "secure": true,
    "channel": 6
  }
]
```

#### GET `/api/wifi/list`
Get cached WiFi scan results (no new scan).

#### POST `/api/wifi/connect`
Connect to a WiFi network.

**Request Body:**
```json
{
  "ssid": "MyNetwork",
  "password": "mypassword"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Credentials saved, connecting..."
}
```

#### POST `/api/wifi/clear`
Clear saved WiFi credentials.

**Response:**
```json
{
  "success": true,
  "message": "WiFi credentials cleared"
}
```

### Status APIs

#### GET `/api/status`
Get device status.

**Response:**
```json
{
  "wifi_connected": true,
  "ap_ssid": "PawMe-Camera",
  "ap_password": "pawme123",
  "saved_ssid": "MyNetwork",
  "ip": "192.168.1.100",
  "rssi": -45,
  "ble_connected": false,
  "ble_name": "PawMe-Camera",
  "free_heap": 245760,
  "psram_free": 4000000
}
```

#### GET `/api/console`
Get device console log.

**Response:**
```json
{
  "log": "PawMe Camera Firmware v2.0\nCamera initialized...\n"
}
```

#### POST `/api/reboot`
Reboot the device.

### Camera APIs

#### GET `/api/stream` or `/mjpeg`
MJPEG video stream (multipart/x-mixed-replace).

**Usage in HTML:**
```html
<img src="http://192.168.4.1/mjpeg" />
```

**Usage in JavaScript:**
```javascript
const img = document.getElementById('stream');
img.src = 'http://192.168.4.1/api/stream';
```

#### GET `/api/capture` or `/capture`
Single JPEG frame capture.

**Response:** `image/jpeg`

---

## BLE API Reference

### Service UUID
```
4fafc201-1fb5-459e-8fcc-c5c9c331914b
```

### Characteristics

#### WiFi List (Read)
- **UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- **Properties**: Read
- **Format**: JSON array of networks

```json
[{"ssid":"Network1","rssi":-45,"secure":true}]
```

#### WiFi Config (Write)
- **UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a9`
- **Properties**: Write
- **Format**: JSON object

```json
{"ssid":"MyNetwork","password":"mypassword"}
```

#### Status (Read/Notify)
- **UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26aa`
- **Properties**: Read, Notify
- **Format**: JSON status

```json
{"status":"connected","ip":"192.168.1.100"}
```

**Status Values:**
- `ready` - Device ready
- `connecting` - Attempting WiFi connection
- `connected` - Successfully connected
- `failed` - Connection failed

---

## Companion App Integration

### iOS/Android App Flow

1. **Scan for BLE devices** with name `PawMe-Camera`
2. **Connect to BLE service** UUID `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
3. **Read WiFi list** from characteristic `beb5483e-36e1-4688-b7f5-ea07361b26a8`
4. **Subscribe to status** notifications on `beb5483e-36e1-4688-b7f5-ea07361b26aa`
5. **Write WiFi credentials** to `beb5483e-36e1-4688-b7f5-ea07361b26a9`
6. **Wait for status notification** with `connected` status and IP address
7. **Connect to video stream** via HTTP at the received IP

### Example: React Native BLE Integration

```javascript
import { BleManager } from 'react-native-ble-plx';

const SERVICE_UUID = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
const WIFI_LIST_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26a8';
const WIFI_CONFIG_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26a9';
const STATUS_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26aa';

// Scan for device
manager.startDeviceScan(null, null, (error, device) => {
  if (device?.name === 'PawMe-Camera') {
    manager.stopDeviceScan();
    connectToDevice(device);
  }
});

// Connect and configure
async function connectToDevice(device) {
  await device.connect();
  await device.discoverAllServicesAndCharacteristics();
  
  // Read WiFi list
  const wifiListChar = await device.readCharacteristicForService(
    SERVICE_UUID, WIFI_LIST_UUID
  );
  const networks = JSON.parse(atob(wifiListChar.value));
  
  // Subscribe to status
  device.monitorCharacteristicForService(
    SERVICE_UUID, STATUS_UUID,
    (error, char) => {
      const status = JSON.parse(atob(char.value));
      if (status.status === 'connected') {
        // Connect to video stream
        const streamUrl = `http://${status.ip}/api/stream`;
      }
    }
  );
  
  // Send WiFi credentials
  const config = JSON.stringify({ ssid: 'MyNetwork', password: 'pass123' });
  await device.writeCharacteristicWithResponseForService(
    SERVICE_UUID, WIFI_CONFIG_UUID, btoa(config)
  );
}
```

### Example: Web App Integration (via WiFi AP)

```javascript
// Connect to PawMe-Camera WiFi first, then:

// Scan WiFi networks
const networks = await fetch('http://192.168.4.1/api/wifi/scan')
  .then(r => r.json());

// Configure WiFi
await fetch('http://192.168.4.1/api/wifi/connect', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ ssid: 'MyNetwork', password: 'pass123' })
});

// Check status
const status = await fetch('http://192.168.4.1/api/status')
  .then(r => r.json());

// Display video stream
document.getElementById('video').src = 'http://192.168.4.1/api/stream';
```

---

## Troubleshooting

### Camera Not Working
- Ensure camera module is properly connected
- Check serial monitor for initialization errors
- Try power cycling the board

### Cannot Connect to AP
- Ensure you're within range
- Check that AP password is correct (`pawme123`)
- Try forgetting the network and reconnecting

### WiFi Connection Fails
- Verify SSID and password are correct
- Ensure the network is 2.4GHz (5GHz not supported)
- Check signal strength (RSSI should be > -80 dBm)

### BLE Not Discoverable
- Ensure no other device is connected via BLE
- Power cycle the board
- Check that BLE is enabled on your phone

### Video Stream Laggy
- Reduce distance to the board
- Ensure good WiFi signal
- Try connecting directly to AP instead of through router

### Serial Monitor Output
Connect via USB and open serial monitor at 115200 baud to see debug output:
```bash
pio device monitor
```

---

## Configuration Constants

Edit `src/main.cpp` to customize:

```cpp
#define DEV_MODE true              // Set false for production (enables password)
#define AP_SSID_PREFIX "PawMe-Robot-" // Access Point name prefix
#define AP_PASSWORD "pawme123"       // Access Point password (production only)
#define BLE_DEVICE_PREFIX "PawMe-Robot-" // Bluetooth name prefix
#define HTTP_PORT 80                 // Web server port
```

---

## Building & Flashing

```bash
# Build
pio run

# Upload
pio run -t upload

# Monitor serial output
pio device monitor
```

---

## License

MIT License - See LICENSE file for details.
