#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <map>

// WiFi und MQTT-Konfiguration
const char* ssid = "TP-Link_9FA0";
const char* password = "24269339";
const char* mqttServer = "192.168.0.120";
const int mqttPort = 1883;
const char* mqttUser = "joe";
const char* mqttPassword = "1337";
const char* deviceID = "HW-888";

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
    Serial.begin(9600);
    setupWiFi();
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
    connectToMQTT();
}

void loop() {
    if (!client.connected()) {
        connectToMQTT();
    }
    if (WiFi.status() != WL_CONNECTED) {
        setupWiFi();
    }
    client.loop();

    if (Serial.available()) {
        String sensorData = Serial.readStringUntil('\n');
        publishData(sensorData);
    }
}

void setupWiFi() {
    delay(10);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
}

void connectToMQTT() {
    while (!client.connected()) {
        if (client.connect("ESP8266Client", mqttUser, mqttPassword)) {
            client.subscribe((String(deviceID) + "/#").c_str());
            client.publish((String(deviceID) + "/status").c_str(), "connected");
        } else {
            delay(2000);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    String topicStr = String(topic);

    if (topicStr.startsWith(deviceID + String("/"))) {
        String command = message;
        Serial.println(command);
    }
}

void publishData(String sensorData) {
    static const std::map<String, String> topicMap = {
        {"v1ec", "V1/EC"},
        {"v2ec", "V2/EC"},
        {"v3ec", "V3/EC"},
        {"v4ec", "V4/EC"},
        {"v1temp", "V1/temp"},
        {"v2temp", "V2/temp"},
        {"v3temp", "V3/temp"},
        {"v4temp", "V4/temp"},
        {"v1waterlow", "V1/waterLow"},
        {"v2waterlow", "V2/waterLow"},
        {"v3waterlow", "V3/waterLow"},
        {"v4waterlow", "V4/waterLow"}
    };

    for (const auto& pair : topicMap) {
        if (sensorData.startsWith(pair.first)) {
            String value = sensorData.substring(pair.first.length());
            String topic = pair.second;
            client.publish(topic.c_str(), value.c_str());
            return;
        }
    }
}
