#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

SensirionI2cScd4x scd4x;

const char* ssid = "TP-Link_9FA0";
const char* password = "24269339";
const char* mqttServer = "192.168.0.120";
const int mqttPort = 1883;
const char* mqttUser = "joe";
const char* mqttPassword = "1337";

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
    WiFi.begin(ssid, password);
    Serial.print("Verbindung mit WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi verbunden");
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Verbindung mit MQTT Server...");
        if (client.connect("ESP8266_CO2", mqttUser, mqttPassword)) {
            Serial.println("verbunden");
        } else {
            Serial.print("fehlgeschlagen, rc=");
            Serial.print(client.state());
            Serial.println(" versuche es in 5 Sekunden erneut");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin(4, 5); // SDA = GPIO4 (D2), SCL = GPIO5 (D1)

    scd4x.begin(Wire, 0x62);
    scd4x.stopPeriodicMeasurement();
    delay(500);
    scd4x.startPeriodicMeasurement();

    setup_wifi();
    client.setServer(mqttServer, mqttPort);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    static unsigned long timepoint = millis();
    if (millis() - timepoint > 5000U) {
        timepoint = millis();

        uint16_t co2;
        float temperature;
        float humidity;
        uint16_t error;
        bool isDataReady = false;
        char errorMessage[256];

        error = scd4x.getDataReadyStatus(isDataReady);
        if (!error && isDataReady) {
            error = scd4x.readMeasurement(co2, temperature, humidity);
            if (!error) {
                Serial.print("CO2: ");
                Serial.print(co2);
                Serial.print(" ppm | Temp: ");
                Serial.print(temperature);
                Serial.print(" C | Humidity: ");
                Serial.println(humidity);

                char msg[10];
                dtostrf(temperature, 1, 2, msg);
                client.publish("room/temp", msg);

                dtostrf(humidity, 1, 2, msg);
                client.publish("room/humidity", msg);

                itoa(co2, msg, 10);
                client.publish("room/co2", msg);
            }
        }
    }
}
