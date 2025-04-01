// StepperMotor.h
#ifndef STEPPERMOTOR_H
#define STEPPERMOTOR_H

#include <AccelStepper.h>

class StepperMotor {
private:
    AccelStepper stepper;
    int enablePin;

public:
    StepperMotor(int stepPin, int dirPin, int enablePin);
    void stop();                     // Initialize or stop the motor
    void moveToPosition(int position);
    void run();
    AccelStepper& getStepper();
};

#endif
