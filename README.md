# PawMe Firmware

ESP32S3 camera firmware for XIAO ESP32S3 Sense with WiFi Access Point, Captive Portal, and MJPEG streaming.

## Features

- **WiFi Access Point**: Creates a WiFi hotspot for direct connection
- **Captive Portal**: Automatically opens configuration page when connecting
- **WiFi Configuration**: Save WiFi credentials to connect to your home network
- **MJPEG Streaming**: Live camera feed viewable in any browser
- **Persistent Storage**: WiFi credentials saved to flash memory

## Hardware

- Seeed Studio XIAO ESP32S3 Sense (with camera module)

## Quick Start

1. **Build and Upload**:
   ```bash
   pio run -t upload
   ```

2. **Connect to the camera**:
   - WiFi SSID: `PawMe-Camera`
   - Password: `pawme123`

3. **Open the captive portal** (should open automatically) or navigate to:
   - http://192.168.4.1

4. **View camera stream**:
   - http://192.168.4.1/stream

## Endpoints

| Endpoint | Description |
|----------|-------------|
| `/` | Configuration page |
| `/stream` | Camera stream viewer |
| `/mjpeg` | Raw MJPEG stream |
| `/capture` | Single JPEG capture |
| `/status` | JSON status info |
| `/save` | POST WiFi credentials |

## Configuration

Edit `src/main.cpp` to change:
- `AP_SSID` - Access point name
- `AP_PASSWORD` - Access point password

## Dependencies

- ESPAsyncWebServer
- AsyncTCP
- esp_camera (built-in)

## License

MIT

## erase flash
pio run -t erase

## build the firmware
pio run

## upload and monitor
pio run -t upload

## upload and monitor
pio device monitor