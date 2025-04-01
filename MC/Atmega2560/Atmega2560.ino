// maincode
#include <Wire.h>
#include <EEPROM.h>
#include "TempSensor.h"
#include "ECSensor.h"
#include <avr/wdt.h> // Watchdog-Timer
#include "StepperMotor.h"

// Pin-Deklarationen
#define EC_SENSOR_PIN_1 A0
#define EC_SENSOR_PIN_2 A1
#define EC_SENSOR_PIN_3 A2
#define EC_SENSOR_PIN_4 A3

#define TEMP_SENSOR_PIN_1 11
#define TEMP_SENSOR_PIN_2 10
#define TEMP_SENSOR_PIN_3 9
#define TEMP_SENSOR_PIN_4 8

#define WATER_LEVEL_LOW_PIN_1 46
#define WATER_LEVEL_LOW_PIN_2 48
#define WATER_LEVEL_LOW_PIN_3 50
#define WATER_LEVEL_LOW_PIN_4 52

#define VAVLE_PIN_1 4
#define VAVLE_PIN_2 5
#define VAVLE_PIN_3 6
#define VAVLE_PIN_4 7

#define PH_DOWN_ENABLE 16
#define PH_DOWN_DIR 15
#define PH_DOWN_STEP 14

#define PH_UP_ENABLE 22
#define PH_UP_DIR 24
#define PH_UP_STEP 26

#define WATER_ENABLE 31
#define WATER_DIR 33
#define WATER_STEP 35

// Timing-Konstanten
#define SENSOR_UPDATE_INTERVAL 3000
#define SERIAL_BAUD_RATE 9600

// Sensor-Instanzen
TempSensor v1TempSensor(TEMP_SENSOR_PIN_1);
TempSensor v2TempSensor(TEMP_SENSOR_PIN_2);
TempSensor v3TempSensor(TEMP_SENSOR_PIN_3);
TempSensor v4TempSensor(TEMP_SENSOR_PIN_4);
ECSensor v1ECSensor(EC_SENSOR_PIN_1);
ECSensor v2ECSensor(EC_SENSOR_PIN_2);
ECSensor v3ECSensor(EC_SENSOR_PIN_3);
ECSensor v4ECSensor(EC_SENSOR_PIN_4);

// Stepper-Instanzen
StepperMotor stepperPhUp(PH_UP_STEP, PH_UP_DIR, PH_UP_ENABLE);
StepperMotor stepperPhDown(PH_DOWN_STEP, PH_DOWN_DIR, PH_DOWN_ENABLE);
StepperMotor stepperWater(WATER_STEP, WATER_DIR, WATER_ENABLE);

// Systemstatus
bool systemOperational = true;

void setup() {
    wdt_enable(WDTO_8S);

    pinMode(WATER_LEVEL_LOW_PIN_1, INPUT_PULLUP);
    pinMode(WATER_LEVEL_LOW_PIN_2, INPUT_PULLUP);
    pinMode(WATER_LEVEL_LOW_PIN_3, INPUT_PULLUP);
    pinMode(WATER_LEVEL_LOW_PIN_4, INPUT_PULLUP);

    pinMode(VAVLE_PIN_1, OUTPUT);
    pinMode(VAVLE_PIN_2, OUTPUT);
    pinMode(VAVLE_PIN_3, OUTPUT);
    pinMode(VAVLE_PIN_4, OUTPUT);

    digitalWrite(VAVLE_PIN_1, LOW);
    digitalWrite(VAVLE_PIN_2, LOW);
    digitalWrite(VAVLE_PIN_3, LOW);
    digitalWrite(VAVLE_PIN_4, LOW);

    Serial.begin(SERIAL_BAUD_RATE);

    systemOperational &= initializeTempSensors();
    systemOperational &= initializeECSensors();
}

bool initializeTempSensors() {
    return v1TempSensor.begin(50) && v2TempSensor.begin(60) && v3TempSensor.begin(70) && v4TempSensor.begin(80);
}

bool initializeECSensors() {
    return v1ECSensor.loadCalibration(10) &&
           v2ECSensor.loadCalibration(20) &&
           v3ECSensor.loadCalibration(30) &&
           v4ECSensor.loadCalibration(40);
}

void loop() {
    wdt_reset();

    static unsigned long lastSensorSend = 0;
    if (!stepperPhUp.getStepper().isRunning() && !stepperWater.getStepper().isRunning() && !stepperPhDown.getStepper().isRunning()) {
        stepperPhUp.stop();
        stepperWater.stop();
        stepperPhDown.stop();
        if (millis() - lastSensorSend > SENSOR_UPDATE_INTERVAL) {
            lastSensorSend = millis();
            sendSensorData();
        }
    }

    checkForCommand();
    stepperPhUp.run();
    stepperPhDown.run();
    stepperWater.run();
}

void calibrateTemperatureSensor(TempSensor& sensor, int eepromAddr) {
    Serial.println("Bitte Referenztemperatur eingeben (in °C):");
    while (!Serial.available());
    float reference = Serial.parseFloat();
    float measured = sensor.getRawTemperature();
    float offset = reference - measured;
    EEPROM.put(eepromAddr, offset);
    sensor.setOffset(offset);
    Serial.print("Kalibrierung abgeschlossen. Offset gespeichert: ");
    Serial.println(offset);
}

void checkForCommand() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        Serial.print("Eingehender Befehl: ");
        Serial.println(command);

        if (command.equals("phup")) {
            Serial.println("Bewege PH-Up Stepper");
            digitalWrite(PH_UP_ENABLE, LOW); // Stepper aktivieren
            int newPos = stepperPhUp.getStepper().currentPosition() + 2000;
            stepperPhUp.moveToPosition(newPos);
        } else if (command.equals("phdown")) {
            Serial.println("Bewege PH-Down Stepper");
            digitalWrite(PH_DOWN_ENABLE, LOW); // Stepper aktivieren
            int newPos = stepperPhDown.getStepper().currentPosition() + 2000;
            stepperPhDown.moveToPosition(newPos);
        } else if (command.equals("water")) {
            Serial.println("Bewege Water Stepper");
            digitalWrite(WATER_ENABLE, LOW); // Stepper aktivieren
            int newPos = stepperWater.getStepper().currentPosition() + 20000;
            stepperWater.moveToPosition(newPos);    
        } else if (command.startsWith("v1eccal")) {
            Serial.println("Kalibriere v1 EC-Sensor");
            v1ECSensor.calibrate(10);
        } else if (command.startsWith("v2eccal")) {
            Serial.println("Kalibriere v2 EC-Sensor");
            v2ECSensor.calibrate(20);
        } else if (command.startsWith("v3eccal")) {
            Serial.println("Kalibriere v3 EC-Sensor");
            v3ECSensor.calibrate(30);
        } else if (command.startsWith("v4eccal")) {
            Serial.println("Kalibriere v4 EC-Sensor");
            v4ECSensor.calibrate(40);
        } else if (command.equals("v1tempcal")) {
            calibrateTemperatureSensor(v1TempSensor, 50);
        } else if (command.equals("v2tempcal")) {
            calibrateTemperatureSensor(v2TempSensor, 60);
        } else if (command.equals("v3tempcal")) {
            calibrateTemperatureSensor(v3TempSensor, 70);
        } else if (command.equals("v4tempcal")) {
            calibrateTemperatureSensor(v4TempSensor, 80);
        } else if (command.equals("v1valveopen")) {
            digitalWrite(VAVLE_PIN_1, HIGH);
        } else if (command.equals("v1valveclose")) {
            digitalWrite(VAVLE_PIN_1, LOW);
        } else if (command.equals("v2valveopen")) {
            digitalWrite(VAVLE_PIN_2, HIGH);
        } else if (command.equals("v2valveclose")) {
            digitalWrite(VAVLE_PIN_2, LOW);
        } else if (command.equals("v3valveopen")) {
            digitalWrite(VAVLE_PIN_3, HIGH);
        } else if (command.equals("v3valveclose")) {
            digitalWrite(VAVLE_PIN_3, LOW);
        } else if (command.equals("v4valveopen")) {
            digitalWrite(VAVLE_PIN_4, HIGH);
        } else if (command.equals("v4valveclose")) {
            digitalWrite(VAVLE_PIN_4, LOW);
        }
    }
}

void sendSensorData() {
    sendSensorValue(v1ECSensor.readEC(), "v1ec");
    sendSensorValue(v2ECSensor.readEC(), "v2ec");
    sendSensorValue(v3ECSensor.readEC(), "v3ec");
    sendSensorValue(v4ECSensor.readEC(), "v4ec");

    sendSensorValue(v1TempSensor.readTemperature(), "v1temp");
    sendSensorValue(v2TempSensor.readTemperature(), "v2temp");
    sendSensorValue(v3TempSensor.readTemperature(), "v3temp");
    sendSensorValue(v4TempSensor.readTemperature(), "v4temp");

    updateWaterLevel(WATER_LEVEL_LOW_PIN_1, "v1waterlow");
    updateWaterLevel(WATER_LEVEL_LOW_PIN_2, "v2waterlow");
    updateWaterLevel(WATER_LEVEL_LOW_PIN_3, "v3waterlow");
    updateWaterLevel(WATER_LEVEL_LOW_PIN_4, "v4waterlow");
}

void sendSensorValue(float value, const char* label) {
    String data = String(label) + String(value, 2);
    Serial.println(data);
}

void updateWaterLevel(int pin, const char* label) {
    bool state = digitalRead(pin) == LOW;
    String stateStr = state ? "true" : "false";
    Serial.println(String(label) + stateStr);
}