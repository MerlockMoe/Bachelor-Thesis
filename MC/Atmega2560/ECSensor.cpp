#include "ECSensor.h"
#include <Arduino.h>
#include <EEPROM.h>

ECSensor::ECSensor(int analogPin) : pin(analogPin), offset(0.0), calibrated(false) {}

float ECSensor::readEC() {
    int rawValue = analogRead(pin);
    float voltage = (rawValue / 1023.0) * 5.0; // Spannung berechnen (0 - 5V)
    float ppmValue = (voltage / 2.3) * 1000; // PPM-Wert berechnen basierend auf max. 2.3V Sensorspannung
    float ecValue = (ppmValue * 2) / 1000; // EC-Wert berechnen
    return ecValue - offset; // Offset subtrahieren, falls kalibriert
}

void ECSensor::calibrate(int eepromAddress) {
    int rawValue = analogRead(pin);
    float voltage = (rawValue / 1023.0) * 5.0; // Spannung berechnen (0 - 5V)
    float ppmValue = (voltage / 2.3) * 1000; // PPM-Wert berechnen
    float ecValue = (ppmValue * 2) / 1000; // EC-Wert berechnen
    offset = ecValue - 1.413; // Kalibrierung mit Referenzwert 1.413 mS/cm
    EEPROM.put(eepromAddress, offset);
    calibrated = true;
}

bool ECSensor::loadCalibration(int eepromAddress) {
    EEPROM.get(eepromAddress, offset);
    calibrated = true;
    return calibrated;
}

bool ECSensor::isConnected() {
    return calibrated;
}
