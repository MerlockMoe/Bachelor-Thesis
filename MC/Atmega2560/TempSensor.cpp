#include "TempSensor.h"
#include <Arduino.h>

TempSensor::TempSensor(int pin) : pin(pin), oneWire(pin), sensors(&oneWire), initialized(false) {}

bool TempSensor::begin() {
    sensors.begin();
    initialized = true;
    return initialized;
}

float TempSensor::readTemperature() {
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    return (temp == DEVICE_DISCONNECTED_C) ? -127.0 : temp; // Fehlerwert, falls kein Sensor verbunden
}

bool TempSensor::isConnected() {
    return initialized;
}
