// Modell-Auswahl und Pin-Definitionen
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include "esp_camera.h"

// --- Individuelle Kamera-ID (z.B. "camV1", "camV2" usw.) ---
static const char* CAM_ID         = "camV4";
// Basis für MQTT-Topics
#define TOPIC_BASE "esp32"

// Puffer für vollständige Topic-Namen
static char CMD_TOPIC[32];
static char START_TOPIC[32];
static char DATA_TOPIC[32];
static char END_TOPIC[32];
static char STATUS_TOPIC[32];

// --- WLAN- und MQTT-Zugangsdaten ---
static const char* WIFI_SSID      = "TP-Link_9FA0";
static const char* WIFI_PASSWORD  = "24269339";
static const char* MQTT_HOST      = "192.168.0.120";
static const uint16_t MQTT_PORT   = 1883;
static const char* MQTT_USER      = "joe";
static const char* MQTT_PASSWORD  = "1337";

// Asynchroner MQTT-Client und Reconnect-Timer
AsyncMqttClient    mqttClient;
TimerHandle_t      mqttReconnectTimer;
TimerHandle_t      statusTimer;

// Forward-Deklarationen
void connectToMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties props,
                   size_t len, size_t index, size_t total);
void setupCamera();
void publishImageInChunks();

// Timer-Callback für periodische "ready"
void publishStatus(TimerHandle_t xTimer) {
    mqttClient.publish(STATUS_TOPIC, 0, false, "ready", strlen("ready"));
}

void setup() {
    Serial.begin(115200);

    // Vollständige Topic-Namen zusammensetzen
    snprintf(CMD_TOPIC, sizeof(CMD_TOPIC), "%s/%s/cmd", TOPIC_BASE, CAM_ID);
    snprintf(START_TOPIC, sizeof(START_TOPIC), "%s/%s/start", TOPIC_BASE, CAM_ID);
    snprintf(DATA_TOPIC, sizeof(DATA_TOPIC), "%s/%s/data", TOPIC_BASE, CAM_ID);
    snprintf(END_TOPIC, sizeof(END_TOPIC), "%s/%s/end", TOPIC_BASE, CAM_ID);
    snprintf(STATUS_TOPIC, sizeof(STATUS_TOPIC), "%s/%s/status", TOPIC_BASE, CAM_ID);

    // Kamera initialisieren
    setupCamera();

    // WLAN-Event: nach IP -> MQTT
    WiFi.onEvent(
      [](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
          Serial.println("WLAN verbunden");
          connectToMqtt();
        }
      },
      ARDUINO_EVENT_WIFI_STA_GOT_IP
    );
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Reconnect-Timer anlegen
    mqttReconnectTimer = xTimerCreate(
        "mqttReconnectTimer",
        pdMS_TO_TICKS(2000),
        pdFALSE,
        nullptr,
        [](TimerHandle_t){ connectToMqtt(); }
    );
    // Status-Timer: 1 Stunde Intervall
    statusTimer = xTimerCreate(
        "statusTimer",
        pdMS_TO_TICKS(3600000),
        pdTRUE,
        nullptr,
        publishStatus
    );

    // MQTT-Client konfigurieren
    mqttClient.setClientId(CAM_ID);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCredentials(MQTT_USER, MQTT_PASSWORD);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);
}

void loop() {
    // Leerlauf: AsyncMqttClient und Timer laufen im Hintergrund
}

// Verbindung zum MQTT-Broker herstellen
void connectToMqtt() {
    Serial.printf("Verbinde MQTT mit Client-ID '%s' auf Topic '%s'...\n", CAM_ID, CMD_TOPIC);
    mqttClient.connect();
}

// Bei erfolgreicher MQTT-Verbindung
void onMqttConnect(bool sessionPresent) {
    Serial.printf("MQTT verbunden (Client-ID '%s')\n", CAM_ID);
    // Status "ready" senden
    mqttClient.publish(STATUS_TOPIC, 0, false, "ready", strlen("ready"));
    // Status-Timer starten (first tick nach 1 Stunde)
    xTimerStart(statusTimer, 0);
    // Befehl-Topic abonnieren
    mqttClient.subscribe(CMD_TOPIC, 0);
}

// Bei Trennung vom Broker
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.println("MQTT getrennt – reconnect in 2 Sekunden");
    xTimerStart(mqttReconnectTimer, 0);
    // Status-Timer stoppen
    xTimerStop(statusTimer, 0);
}

// Ankommende MQTT-Nachrichten
void onMqttMessage(char* topic, char* payload,
    AsyncMqttClientMessageProperties props,
    size_t len, size_t index, size_t total)
{
    if (strcmp(topic, CMD_TOPIC) == 0
        && len == 7
        && memcmp(payload, "capture", 7) == 0)
    {
        publishImageInChunks();
    }
}

// Kamera konfigurieren und Einstellungen setzen
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
    config.frame_size   = FRAMESIZE_SVGA;
    config.jpeg_quality = 20;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;

    esp_err_t err = esp_camera_init(&config);
    sensor_t* s = esp_camera_sensor_get();
    Serial.printf("Sensor-PID: 0x%04x\n", s->id.PID);

    // Weißabgleich Office
    s->set_whitebal(s, false);
    s->set_wb_mode(s, 3);
    // Manuelle Belichtung
    s->set_exposure_ctrl(s, false);
    s->set_aec2(s, false);
    s->set_ae_level(s, -1);
    s->set_aec_value(s, 40);
    // Manuelles Gain
    s->set_gain_ctrl(s, false);
    s->set_agc_gain(s, 0);
    // Farb- und Kontrast-Feintuning
    s->set_brightness(s, 0);
    s->set_saturation(s, 0);
    s->set_contrast(s, 0);

    if (err != ESP_OK) {
        Serial.printf("Kamera-Init FEHLGESCHLAGEN: 0x%x\n", err);
        while (true) { delay(1000); }
    }
}

// Bild seriell in Chunks über MQTT senden
void publishImageInChunks() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Fehler: Kein Frame");
        return;
    }
    size_t len = fb->len;
    char lenBuf[16];
    snprintf(lenBuf, sizeof(lenBuf), "%u", (unsigned)len);
    mqttClient.publish(START_TOPIC, 0, false, lenBuf, strlen(lenBuf));
    const size_t CHUNK_SIZE = 1024;
    size_t sent = 0;
    while (sent < len) {
        size_t sz = min(CHUNK_SIZE, len - sent);
        mqttClient.publish(DATA_TOPIC, 0, false,
                           reinterpret_cast<const char*>(fb->buf + sent), sz);
        sent += sz;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    mqttClient.publish(END_TOPIC, 0, false, nullptr, 0);
    esp_camera_fb_return(fb);
}
