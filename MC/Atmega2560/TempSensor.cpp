// TempSensor.cpp
#include "TempSensor.h"
#include <Arduino.h>
#include <EEPROM.h>

TempSensor::TempSensor(int pin) : pin(pin), oneWire(pin), sensors(&oneWire), initialized(false), calibrationOffset(0.0f), eepromAddress(0) {}

bool TempSensor::begin(int addr) {
    sensors.begin();
    eepromAddress = addr;
    EEPROM.get(eepromAddress, calibrationOffset);
    if (isnan(calibrationOffset)) calibrationOffset = 0.0f;
    initialized = true;
    return initialized;
}

float TempSensor::readTemperature() {
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    return (temp == DEVICE_DISCONNECTED_C) ? -127.0 : temp + calibrationOffset;
}

float TempSensor::getRawTemperature() {
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    return (temp == DEVICE_DISCONNECTED_C) ? -127.0 : temp;
}

bool TempSensor::isConnected() {
    return initialized;
}

void TempSensor::setOffset(float offset) {
    calibrationOffset = offset;
}