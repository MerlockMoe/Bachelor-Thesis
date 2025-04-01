// StepperMotor.cpp
#include "StepperMotor.h"
#include <Arduino.h>

StepperMotor::StepperMotor(int stepPin, int dirPin, int enablePin) 
    : stepper(AccelStepper::DRIVER, stepPin, dirPin), enablePin(enablePin) {
    pinMode(enablePin, OUTPUT);
    digitalWrite(enablePin, HIGH);           // Motor deaktivieren, bis eine Bewegung ausgeführt wird
    stepper.setMaxSpeed(2000.0);     // Set maximum speed (steps per second)
    stepper.setAcceleration(100000.0); // Set acceleration (steps per second squared)
}

void StepperMotor::stop() {
    stepper.setCurrentPosition(0);
    stepper.stop();
    digitalWrite(enablePin, HIGH);  // Motor deaktivieren, wenn nicht in Nutzung
}

void StepperMotor::moveToPosition(int position) {
    digitalWrite(enablePin, LOW);   // Motor aktivieren
    stepper.moveTo(position);
}

void StepperMotor::run() {
    if (stepper.distanceToGo() != 0) {
        stepper.run();
    } else {
        digitalWrite(enablePin, HIGH);  // Motor deaktivieren, wenn das Ziel erreicht ist
    }
}

AccelStepper& StepperMotor::getStepper() {
    return stepper;
}
