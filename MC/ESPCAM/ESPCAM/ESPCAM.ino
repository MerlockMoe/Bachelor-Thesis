#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include "camera_pins.h"
#include "esp_camera.h"

// --- Netzwerk- und MQTT-Zugangsdaten ---
const char* ssid         = "TP-Link_9FA0";
const char* password     = "24269339";
const char* mqttServer   = "192.168.0.120";
const int   mqttPort     = 1883;
const char* mqttUser     = "joe";
const char* mqttPassword = "1337";
const char* mqttTopic    = "esp32/cmd";
const char* postURL      = "http://192.168.0.120:5000/upload";

// ESP-Netzwerk- und Dienst-Clients
WiFiClient     netClient;
PubSubClient   mqtt(netClient);

// Kamera initialisieren (neutral-professionelle Kommentare)
void setupCamera() {
    camera_config_t config;
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
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count     = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Kamera-Init FEHLGESCHLAGEN: 0x%x\n", err);
        while(true) delay(1000);
    }
}

// MQTT-Callback: auf Befehl „capture“ reagie­ren
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    if (msg.equals("capture")) {
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Fehler: Kein Frame");
            return;
        }
        HTTPClient http;
        http.begin(postURL);
        http.addHeader("Content-Type", "image/jpeg");
        int httpCode = http.POST(fb->buf, fb->len);
        if (httpCode == 200) {
            Serial.println("Bild erfolgreich gesendet");
        } else {
            Serial.printf("Upload-Fehler: %d\n", httpCode);
        }
        http.end();
        esp_camera_fb_return(fb);
    }
}

// MQTT-Verbindung herstellen (mit Reconnect-Logik)
void reconnectMQTT() {
    while (!mqtt.connected()) {
        Serial.print("Verbinde MQTT… ");
        if (mqtt.connect("esp32cam", mqttUser, mqttPassword)) {
            Serial.println("verbunden");
            mqtt.subscribe(mqttTopic);
        } else {
            Serial.printf("fehlgeschlagen, rc=%d. Warte 5s\n", mqtt.state());
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    setupCamera();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nWLAN verbunden");

    mqtt.setServer(mqttServer, mqttPort);
    mqtt.setCallback(mqttCallback);
    reconnectMQTT();
}

void loop() {
    if (!mqtt.connected()) {
        reconnectMQTT();
    }
    mqtt.loop();
}
