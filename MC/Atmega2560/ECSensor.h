#ifndef ECSENSOR_H
#define ECSENSOR_H

class ECSensor {
private:
    int pin;
    float offset;
    bool calibrated;
public:
    ECSensor(int analogPin);
    float readEC();
    void calibrate(int eepromAddress);
    bool loadCalibration(int eepromAddress);
    bool isConnected();
};

#endif
