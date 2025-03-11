#include <Wire.h>
#include <EEPROM.h>
#include "TempSensor.h"
#include "ECSensor.h"
#include <avr/wdt.h> // Watchdog-Timer

// Pin-Deklarationen
#define EC_SENSOR_PIN_1 A0
#define EC_SENSOR_PIN_2 A1
#define EC_SENSOR_PIN_3 A2
#define EC_SENSOR_PIN_4 A3
#define TEMP_SENSOR_PIN_1 7
#define TEMP_SENSOR_PIN_2 8
#define TEMP_SENSOR_PIN_3 9
#define TEMP_SENSOR_PIN_4 10
#define WATER_LEVEL_LOW_PIN_1 3
#define WATER_LEVEL_LOW_PIN_2 4
#define WATER_LEVEL_LOW_PIN_3 5
#define WATER_LEVEL_LOW_PIN_4 6

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

// Systemstatus
bool systemOperational = true;

void setup() {
    wdt_enable(WDTO_8S);

    pinMode(WATER_LEVEL_LOW_PIN_1, INPUT_PULLUP);
    pinMode(WATER_LEVEL_LOW_PIN_2, INPUT_PULLUP);
    pinMode(WATER_LEVEL_LOW_PIN_3, INPUT_PULLUP);
    pinMode(WATER_LEVEL_LOW_PIN_4, INPUT_PULLUP);

    Serial.begin(SERIAL_BAUD_RATE);

    systemOperational &= initializeTempSensors();
    systemOperational &= initializeECSensors();
}

bool initializeTempSensors() {
    return v1TempSensor.begin() && v2TempSensor.begin() && v3TempSensor.begin() && v4TempSensor.begin();
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
    if (millis() - lastSensorSend > SENSOR_UPDATE_INTERVAL) {
        lastSensorSend = millis();
        sendSensorData();
    }

    checkForCalibration();
}

void checkForCalibration() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.startsWith("v1eccal")) {
            v1ECSensor.calibrate(10); // EEPROM-Adresse für v1
        } else if (command.startsWith("v2eccal")) {
            v2ECSensor.calibrate(20); // EEPROM-Adresse für v2
        } else if (command.startsWith("v3eccal")) {
            v3ECSensor.calibrate(30); // EEPROM-Adresse für v3
        } else if (command.startsWith("v4eccal")) {
            v4ECSensor.calibrate(40); // EEPROM-Adresse für v4
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
