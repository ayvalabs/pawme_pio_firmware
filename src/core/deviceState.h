#pragma once

enum DeviceState {
  WIFI_SETUP_MODE,
  WIFI_CONNECTING,
  WIFI_CONNECTED
};

extern DeviceState deviceState;

// ADDED: Global sensor values
struct SensorData {
  float temperature = 0.0;
  int distance = 0;
};

extern SensorData currentSensors;
