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
public:
    TempSensor(int pin);
    bool begin();
    float readTemperature();
    bool isConnected();
};

#endif
