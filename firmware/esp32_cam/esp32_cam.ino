// ============================================
// ECOBIN — Nó de Visão (ESP32-CAM)
// ============================================
// Totalmente passivo: Apenas serve a imagem JPEG via HTTP
// quando o Gateway (ativado pelo Arduino) o requisitar.
// ============================================

#include "esp_camera.h"
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>

const char *WIFI_SSID = "WiFi-Tutunaru";
const char *WIFI_PASSWORD = ")RUl0Y1kijUvH2";

const char *MQTT_BROKER = "192.168.1.104";
const int MQTT_PORT = 1883;

// Pinout ESP32-CAM (AI-Thinker)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#define FLASH_LED_PIN 4

WebServer httpServer(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastReconnectAttempt = 0;

void initCamera() {
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_camera_init(&config);
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor) {
    sensor->set_brightness(sensor, 1);
    sensor->set_whitebal(sensor, 1);
    sensor->set_exposure_ctrl(sensor, 1);
  }
}

void handleCapture() {
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(150); // Dar tempo para a câmara ajustar a luz

  camera_fb_t *fb = esp_camera_fb_get();

  digitalWrite(FLASH_LED_PIN, LOW); // Apagar logo a seguir

  if (!fb) {
    httpServer.send(500, "text/plain", "Erro");
    return;
  }

  httpServer.sendHeader("Content-Type", "image/jpeg");
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void reconnectMQTT() {
  if (mqttClient.connect("ecobin-esp32cam", NULL, NULL, "ecobin/cam/status", 1,
                         true, "offline")) {
    mqttClient.publish("ecobin/cam/status", "online", true);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("[CAM] A iniciar...");

  initCamera();
  Serial.println("[CAM] Câmara OK");

  Serial.println("[CAM] A ligar WiFi...");
  connectWiFi();
  Serial.println("[CAM] WiFi OK — IP: " + WiFi.localIP().toString());

  httpServer.on("/capture", HTTP_GET, handleCapture);
  httpServer.begin();
  Serial.println("[CAM] Servidor HTTP ativo");

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  reconnectMQTT();
  Serial.println("[CAM] MQTT OK");
}

void loop() {
  httpServer.handleClient();

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnectMQTT();
    }
  } else {
    mqttClient.loop();
  }
}
