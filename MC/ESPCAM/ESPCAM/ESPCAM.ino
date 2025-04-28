// === ESP32-CAM: Bildaufnahme per MQTT + Bereitstellung via HTTP ===
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"
#include "Arduino.h"
#include <ESPAsyncWebServer.h>
#include <base64.h>  // Arduino-kompatible Base64-Library

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

const char* ssid = "TP-Link_9FA0";
const char* password = "24269339";
const char* mqttServer = "192.168.0.120";
const int mqttPort = 1883;
const char* mqttUser = "joe";
const char* mqttPassword = "1337";

const char* topicTrigger = "cam/CAM01/capture";

WiFiClient espClient;
PubSubClient client(espClient);
AsyncWebServer server(80);

bool imageRequested = false;
camera_fb_t* capturedFrame = nullptr;

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera-Init FEHLGESCHLAGEN: 0x%x\n", err);
    return;
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Verbinde mit MQTT...");
    if (client.connect("CAM-V1", mqttUser, mqttPassword)) {
      Serial.println("verbunden");
      client.subscribe(topicTrigger);
    } else {
      Serial.print("Fehler: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, topicTrigger) == 0) {
    imageRequested = true;
  }
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWLAN verbunden");

  setupCamera();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  server.on("/latest.jpg", HTTP_GET, [](AsyncWebServerRequest *request){
    if (capturedFrame) {
      request->send_P(200, "image/jpeg", (const uint8_t*)capturedFrame->buf, capturedFrame->len);
    } else {
      request->send(404, "text/plain", "Kein Bild verfügbar");
    }
  });
  server.begin();
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  if (imageRequested) {
    imageRequested = false;

    if (capturedFrame) {
      esp_camera_fb_return(capturedFrame);
      capturedFrame = nullptr;
    }

    capturedFrame = esp_camera_fb_get();
    if (!capturedFrame) {
      Serial.println("Fehler: Kein Bild erhalten");
    } else {
      Serial.println("Bild aufgenommen und bereitgestellt");
    }
  }
}
