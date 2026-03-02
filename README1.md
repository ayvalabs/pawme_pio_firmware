# 🐾 Pawme  
### WiFi Controlled ESP32-S3 Rover with Live Camera, OTA & Sensor Dashboard

---

## 🚀 Overview

Pawme is a WiFi-controlled robotic rover built using the **XIAO ESP32S3** and **OV3660 camera module**.

It provides:

- 📡 WiFi provisioning (AP → Home Network)
- 🎥 Live MJPEG camera streaming
- 🎮 Web-based motor control
- 🌡 Temperature monitoring
- 📏 Distance sensing
- 🔁 OTA firmware updates
- 🌐 mDNS support (`pawme.local`)

Everything runs directly from the browser and even from app-"Pawme".

---

# 🧠 Hardware

| Component | Model |
|-----------|--------|
| Board | XIAO ESP32S3 |
| Camera | OV3660 |
| Motors | 4 GPIO controlled |
| Sensors | Temperature + Distance |

---

# 🌐 How It Works

## 🟢 Step 1 – Setup Mode (First Boot)

1. Power ON the robot.
2. Connect to the WiFi network created by the robot.
3. Open a browser and go to:

```
http://192.168.4.1
```

4. Enter your home WiFi credentials.
5. Click Save.
6. The robot reboots.

---

## 🔵 Step 2 – Station Mode (Connected to Home WiFi)

After reboot:

- AP turns OFF
- Robot connects to your Home WiFi
- OTA becomes available
- Camera stream starts
- Web dashboard becomes active

Now:

1. Connect your device to the same Home WiFi.
2. Open:

```
http://pawme.local
```

or

```
http://<new-robot-ip-address>
```

---

# 🎥 Live Camera Stream

```
http://pawme.local:81/stream
```

or

```
http://<new-robot-ip>:81/stream
```

Stream type: MJPEG over HTTP

---

# 🎮 Web Dashboard Features

- Live camera preview
- Forward / Backward / Left / Right control
- Real-time temperature
- Real-time distance
- WiFi credential update form

Sensor data auto-refreshes every 2 seconds.

---

# 🔌 API Endpoints

### Root
```
GET /
```

### Save WiFi
```
POST /wifi
```

### Move Motors
```
GET /move?dir=forward
GET /move?dir=backward
GET /move?dir=left
GET /move?dir=right
GET /move?dir=stop
```

### Sensor Status
```
GET /status
```

Example response:

```json
{
  "temp": 28.5,
  "dist": 42
}
```

---

# 🔄 OTA Update

Once connected to home WiFi:

- Hostname: `pawme`

In Arduino IDE:

Tools → Port → pawme (Network Port)

No USB required after first upload.

---

# 📂 Project Structure

```
src/
 ├── core/
 │    ├── wifiManager.h
 │    ├── deviceState.h
 │
 ├── hardware/
 │    ├── cameraManager.h
 │    ├── motorManager.h
 │    ├── sensorManager.h

pawme.ino
```

---

# ⚙️ Setup

1. Install ESP32 board package.
2. Select board: **XIAO ESP32S3**
3. Upload firmware.
4. Power the robot.
5. Configure WiFi.
6. Access dashboard.

---

# 🌟 Future Improvements

- React-based UI
- Obstacle avoidance
- Video recording
- Cloud control
- Voice control

---

# 📜 License

MIT
