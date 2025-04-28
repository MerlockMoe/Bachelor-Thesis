// Modell-Auswahl und Pin-Definitionen
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"

// --- WLAN- und MQTT-Zugangsdaten ---
const char* ssid         = "TP-Link_9FA0";
const char* password     = "24269339";
const IPAddress mqttBroker(192, 168, 0, 120);
const uint16_t mqttPort  = 1883;
const char* mqttUser     = "joe";
const char* mqttPassword = "1337";
const char* mqttTopic    = "esp32/cmd";

// --- TCP-Server auf Ubuntu ---
const IPAddress tcpServer(192, 168, 0, 120);
const uint16_t tcpPort   = 6000;

// Netzwerk- und MQTT-Clients
WiFiClient     netClient;
PubSubClient   mqtt(netClient);

// Flag zum Triggern der Bildübertragung
volatile bool sendTCP = false;

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
    config.frame_size   = FRAMESIZE_QVGA;  // 320×240 reduziert Heap-Bedarf
    config.jpeg_quality = 20;              // stärkere Kompression
    config.fb_count     = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Kamera-Init FEHLGESCHLAGEN: 0x%x\n", err);
        while (true) { yield(); }
    }
}

// MQTT-Callback: nur Flag setzen, kein HTTP/TCP hier
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, mqttTopic) != 0) return;
    if (length == 7 && memcmp(payload, "capture", 7) == 0) {
        sendTCP = true;
    }
}

// MQTT-Reconnect mit yield() zur Task-Freigabe
void reconnectMQTT() {
    while (!mqtt.connected()) {
        yield();
        Serial.print("Verbinde MQTT… ");
        if (mqtt.connect("esp32cam", mqttUser, mqttPassword)) {
            Serial.println("OK");
            mqtt.subscribe(mqttTopic);
        } else {
            Serial.printf("fehlgeschlagen, rc=%d\n", mqtt.state());
            delay(2000);
        }
    }
}

// Bild aufnehmen und per TCP senden
void doCaptureAndUpload() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Fehler: Kein Frame");
        return;
    }

    if (netClient.connect(tcpServer, tcpPort)) {
        // Länge als 4-Byte (Little Endian) senden
        uint32_t len = fb->len;
        netClient.write(reinterpret_cast<uint8_t*>(&len), sizeof(len));
        // JPEG-Daten senden
        netClient.write(fb->buf, fb->len);
        netClient.stop();
        Serial.printf("Bild gesendet (%u Bytes)\n", len);
    } else {
        Serial.println("TCP-Verbindung fehlgeschlagen");
    }

    esp_camera_fb_return(fb);
}

void setup() {
    Serial.begin(115200);
    setupCamera();

    // WLAN verbinden
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        yield();
    }
    Serial.println("WLAN verbunden");

    // MQTT initialisieren
    mqtt.setServer(mqttBroker, mqttPort);
    mqtt.setCallback(mqttCallback);
    reconnectMQTT();
}

void loop() {
    if (!mqtt.connected()) {
        reconnectMQTT();
    }
    mqtt.loop();

    if (sendTCP) {
        sendTCP = false;
        doCaptureAndUpload();
    }

    // Kurze Pause für WiFi-Task
    delay(10);
}
