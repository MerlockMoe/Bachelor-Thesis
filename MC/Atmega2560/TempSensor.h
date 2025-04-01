// TempSensor.h
#ifndef TEMPERATURESENSOR_H
#define TEMPERATURESENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

class TempSensor {
private:
    int pin;
    OneWire oneWire;
    DallasTemperature sensors;
    bool initialized;
    float calibrationOffset;
    int eepromAddress;
public:
    TempSensor(int pin);
    bool begin(int addr);
    float readTemperature();
    float getRawTemperature();
    bool isConnected();
    void setOffset(float offset);
};

#endif