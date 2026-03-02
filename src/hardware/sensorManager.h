#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

// MOVED to high pins to avoid motor pin conflict (1,2,3,4)
#define TEMP_PIN 5      // D3
#define TRIG_PIN 6      // D4
#define ECHO_PIN 7      // D5

void sensorsInit();
void updateSensors();

#endif