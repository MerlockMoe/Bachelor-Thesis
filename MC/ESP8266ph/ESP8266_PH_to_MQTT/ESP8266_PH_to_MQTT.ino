#include "DFRobot_PH.h"
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define PH_PIN A0
float voltage, phValue, temperature = 25;
DFRobot_PH ph;

const char* ssid = "TP-Link_9FA0";
const char* password = "24269339";
const char* mqttServer = "192.168.0.120";
const int mqttPort = 1883;
const char* mqttUser = "joe";
const char* mqttPassword = "1337";

WiFiClient espClient;
PubSubClient client(espClient);

char temperaturePayload[10];

void callback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, "V2/temp") == 0) {
        memset(temperaturePayload, 0, sizeof(temperaturePayload));
        memcpy(temperaturePayload, payload, length);
        temperature = atof(temperaturePayload);
        Serial.print("Temperatur empfangen: ");
        Serial.println(temperature);
    }
}

void setup() {
    Serial.begin(115200);
    EEPROM.begin(512);   // EEPROM initialisieren (wichtig für ESP)
    ph.begin();          // pH-Sensor initialisieren
    Serial.println("Lade gespeicherte Kalibrierungswerte...");
    ph.calibration(voltage, temperature, (char*)"load"); // Lade gespeicherte Werte aus dem EEPROM
    Serial.println("Kalibrierung geladen.");

    // Verbindung mit WLAN
    WiFi.begin(ssid, password);
    Serial.print("Verbindung mit WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi verbunden");

    // Verbindung mit MQTT Server
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
    while (!client.connected()) {
        Serial.print("Verbindung mit MQTT Server...");
        if (client.connect("ESP8266Client", mqttUser, mqttPassword)) {
            Serial.println("verbunden");
            client.subscribe("V2/temp");
        } else {
            Serial.print("fehlgeschlagen, rc=");
            Serial.print(client.state());
            Serial.println(" versuche es in 5 Sekunden erneut");
            delay(5000);
        }
    }
}

void loop() {
    static unsigned long timepoint = millis();
    if (millis() - timepoint > 1000U) {  // 1-Sekunden-Intervall
        timepoint = millis();

        // Spannung korrekt berechnen (ESP = 3,3V, Arduino = 5V)
        voltage = analogRead(PH_PIN) / 1024.0 * 3300;  // Falls ESP8266 mit 3.3V ADC
        // voltage = analogRead(PH_PIN) / 1024.0 * 5000;  // Falls Arduino mit 5V ADC

        phValue = ph.readPH(voltage, temperature);  // Berechnung mit empfangener Temperatur

        Serial.print("Spannung: ");
        Serial.print(voltage, 2);
        Serial.print("mV  |  pH-Wert: ");
        Serial.println(phValue, 2);

        // pH-Wert an MQTT Server senden
        char msg[10];
        dtostrf(phValue, 1, 2, msg);
        client.publish("V2/pH", msg);
    }

    ph.calibration(voltage, temperature);  // Führt die Kalibrierung durch (bei Befehl über Serial Monitor)
    client.loop();  // MQTT-Client am Leben halten
}
