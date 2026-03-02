#ifndef MOTOR_MANAGER_H
#define MOTOR_MANAGER_H

#include <Arduino.h>
#include "driver/mcpwm.h"

// Updated to match your screenshot settings exactly
#define MOTOR_A_IN1 1  // Left Track Forward
#define MOTOR_A_IN2 2  // Left Track Reverse
#define MOTOR_B_IN1 4  // Right Track Forward
#define MOTOR_B_IN2 3  // Right Track Reverse

void motorsInit();
void moveForward();
void moveBackward();
void turnLeft();
void turnRight();
void stopMotors();

#endif