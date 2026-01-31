#pragma once

enum DeviceState {
  WIFI_SETUP_MODE,
  WIFI_CONNECTING,
  WIFI_CONNECTED
};

extern DeviceState deviceState;
