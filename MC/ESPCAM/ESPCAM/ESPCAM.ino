// Neutral-professionelle Kommentare im Code
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"

// WLAN- und MQTT-Credentials
const char* ssid         = "TP-Link_9FA0";
const char* password     = "24269339";
const IPAddress mqttBroker(192, 168, 0, 120);
const uint16_t mqttPort  = 1883;
const char* mqttUser     = "joe";
const char* mqttPassword = "1337";
const char* cmdTopic     = "esp32/cmd";

// Bild-Chunk-Topics
const char* startTopic   = "esp32/image/start";
const char* dataTopic    = "esp32/image/data";
const char* endTopic     = "esp32/image/end";

// Globale Client-Instanzen
WiFiClient     netClient;
PubSubClient   mqtt(netClient);

// Flag zum Auslösen der Aufnahme
volatile bool captureRequested = false;

// Kamera initialisieren
void setupCamera() {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_QVGA;  // 320×240 für geringen Speicherbedarf
    config.jpeg_quality = 20;              // stärkere Kompression
    config.fb_count     = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Kamera-Init FEHLGESCHLAGEN: 0x%x\n", err);
        while (true) { yield(); }
    }
}

// Callback: nur das Flag setzen, keine Bildübertragung hier
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, cmdTopic) == 0
        && length == 7
        && memcmp(payload, "capture", 7) == 0) {
        captureRequested = true;
    }
}

// MQTT-Reconnect mit yield() zur Task-Freigabe
void reconnectMQTT() {
    while (!mqtt.connected()) {
        yield();
        Serial.print("Verbinde MQTT… ");
        if (mqtt.connect("esp32cam", mqttUser, mqttPassword)) {
            Serial.println("OK");
            mqtt.subscribe(cmdTopic);
        } else {
            Serial.printf("fehlgeschlagen, rc=%d\n", mqtt.state());
            delay(2000);
        }
    }
}

// Aufnahme und in Chunks per MQTT senden
void publishImageInChunks() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Fehler: Kein Frame");
        return;
    }

    size_t len = fb->len;

    // Start-Nachricht mit Länge
    char lenStr[16];
    snprintf(lenStr, sizeof(lenStr), "%u", (unsigned)len);
    mqtt.publish(startTopic, lenStr);

    // Nutzdaten in CHUNK_SIZE-Blöcken
    const size_t CHUNK_SIZE = 1024;
    size_t sent = 0;
    while (sent < len) {
        size_t sz = min(CHUNK_SIZE, len - sent);
        mqtt.publish(dataTopic, fb->buf + sent, sz);
        sent += sz;
        delay(1);  // yield für WiFi-Task
    }

    // End-Nachricht
    mqtt.publish(endTopic, "");

    esp_camera_fb_return(fb);
}

void setup() {
    Serial.begin(115200);
    setupCamera();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        yield();
    }
    Serial.println("WLAN verbunden");

    mqtt.setServer(mqttBroker, mqttPort);
    mqtt.setCallback(mqttCallback);
    reconnectMQTT();
}

void loop() {
    if (!mqtt.connected()) {
        reconnectMQTT();
    }
    mqtt.loop();

    if (captureRequested) {
        captureRequested = false;
        publishImageInChunks();
    }

    delay(10);  // Freigabe für System-Tasks
}
