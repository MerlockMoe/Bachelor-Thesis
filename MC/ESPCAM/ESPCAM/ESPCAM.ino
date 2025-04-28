// Modell-Auswahl und Pin-Definitionen
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#include <WiFi.h>
#include <AsyncMqttClient.h>
#include "esp_camera.h"

// --- WLAN- und MQTT-Zugangsdaten ---
static const char* WIFI_SSID       = "TP-Link_9FA0";
static const char* WIFI_PASSWORD   = "24269339";
static const char* MQTT_HOST       = "192.168.0.120";
static const uint16_t MQTT_PORT    = 1883;
static const char* MQTT_USER       = "joe";
static const char* MQTT_PASSWORD   = "1337";

// --- MQTT-Topics ---
static const char* CMD_TOPIC       = "esp32/cmd";
static const char* START_TOPIC     = "esp32/image/start";
static const char* DATA_TOPIC      = "esp32/image/data";
static const char* END_TOPIC       = "esp32/image/end";

// Instanz des asynchronen MQTT-Clients und Timer für Reconnect
AsyncMqttClient    mqttClient;
TimerHandle_t      mqttReconnectTimer;

// Forward-Deklarationen
void connectToMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties props, size_t len, size_t index, size_t total);
void setupCamera();
void publishImageInChunks();

void setup() {
    Serial.begin(115200);

    // Kamera in QVGA + JPEG starten, um Heap-Bedarf minimal zu halten
    setupCamera();

    // WLAN-Ereignisse: nach IP -> MQTT verbinden
    WiFi.onEvent([](WiFiEvent_t event){
        if (event == SYSTEM_EVENT_STA_GOT_IP) {
            Serial.println("WLAN verbunden");
            connectToMqtt();
        }
    });
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Timer für automatischen MQTT-Reconnect anlegen
    mqttReconnectTimer = xTimerCreate(
        "mqttTimer",
        pdMS_TO_TICKS(2000),
        pdFALSE,
        nullptr,
        [](TimerHandle_t){ connectToMqtt(); }
    );

    // MQTT-Callbacks registrieren
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);

    // Broker-Adressen und Auth setzen
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCredentials(MQTT_USER, MQTT_PASSWORD);
}

void loop() {
    // Intentional empty: AsyncMqttClient verarbeitet alles im Hintergrund
}

// Baut Verbindung zum Broker auf
void connectToMqtt() {
    Serial.println("Verbinde MQTT…");
    mqttClient.connect();
}

// Wird aufgerufen, wenn Verbindung steht
void onMqttConnect(bool sessionPresent) {
    Serial.println("MQTT verbunden");
    mqttClient.subscribe(CMD_TOPIC, 0);
}

// Wird aufgerufen, wenn getrennt wird
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.println("MQTT getrennt – reconnect in 2 Sekunden");
    xTimerStart(mqttReconnectTimer, 0);
}

// Empfangene MQTT-Nachrichten verarbeiten
void onMqttMessage(char* topic, char* payload,
    AsyncMqttClientMessageProperties props,
    size_t len, size_t index, size_t total)
{
    // Nur auf “capture”-Befehl reagieren
    if (strcmp(topic, CMD_TOPIC) == 0
        && len == 7
        && memcmp(payload, "capture", 7) == 0)
    {
        publishImageInChunks();
    }
}

// Kamera gemäß camera_pins.h initialisieren
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
    config.jpeg_quality = 20;              // höhere Kompression
    config.fb_count     = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Kamera-Init FEHLGESCHLAGEN: 0x%x\n", err);
        while (true) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
}

// JPEG in Chunks über MQTT versenden
void publishImageInChunks() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Fehler: Kein Frame");
        return;
    }

    size_t len = fb->len;
    char lenBuf[16];
    snprintf(lenBuf, sizeof(lenBuf), "%u", (unsigned)len);

    // Start-Nachricht mit Gesamtlänge
    mqttClient.publish(START_TOPIC, 0, false, lenBuf, strlen(lenBuf));

    // Nutzdaten in 1 KB-Blöcken
    const size_t CHUNK_SIZE = 1024;
    size_t sent = 0;
    while (sent < len) {
        size_t sz = min(CHUNK_SIZE, len - sent);
        mqttClient.publish(DATA_TOPIC, 0, false, fb->buf + sent, sz);
        sent += sz;
        vTaskDelay(pdMS_TO_TICKS(1));  // kurz pausieren für WiFi-Task
    }

    // Ende-Meldung
    mqttClient.publish(END_TOPIC, 0, false, nullptr, 0);
    esp_camera_fb_return(fb);
}
