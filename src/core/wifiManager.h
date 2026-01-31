#pragma once
#include <Arduino.h>

void wifiInit();
void wifiLoop();

bool wifiIsConnected();
String wifiIP();
