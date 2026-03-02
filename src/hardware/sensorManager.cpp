#include "sensorManager.h"
#include "../core/deviceState.h"
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensors(&oneWire);
SensorData currentSensors;

void sensorsInit() {
    pinMode(TEMP_PIN, INPUT); // Required for some S3 variants to stabilize OneWire
    tempSensors.begin();
    
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    
    Serial.println("[SENSORS] Initialized (Temp & Distance)");
}

void updateSensors() {
    // 1. Temperature: Use a non-blocking check
    tempSensors.setWaitForConversion(false); // Don't block the CPU
    tempSensors.requestTemperatures();
    
    // 2. Distance: Add a small delay for stability
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    // Increase timeout to 20ms for better stability
    long duration = pulseIn(ECHO_PIN, HIGH, 20000); 
    
    if (duration > 0) {
        currentSensors.distance = duration * 0.034 / 2;
    }

    // Now read the temp from the previous request
    float t = tempSensors.getTempCByIndex(0);
    if (t != -127.00 && t != 85.00 && t != 0.00) {
        currentSensors.temperature = t;
    }
}