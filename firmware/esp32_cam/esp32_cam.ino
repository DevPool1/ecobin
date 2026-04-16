// ============================================
// ECOBIN — Nó de Visão (ESP32-CAM)
// ============================================
// Captura frames JPEG do resíduo na bandeja
// e serve via endpoint HTTP para o Raspberry Pi.
// Comunica estado via MQTT.
//
// Hardware: ESP32-CAM (AI-Thinker) com OV2640
// Comunicação: HTTP server (imagens) + MQTT (controlo)
// ============================================

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

// ── Configuração WiFi ───────────────────────
// Para apresentação: usar hotspot do telemóvel
const char* WIFI_SSID     = "ECOBIN_Hotspot";
const char* WIFI_PASSWORD  = "ecobin2026";

// ── Configuração MQTT ───────────────────────
// IP do Raspberry Pi na rede do hotspot
const char* MQTT_BROKER    = "192.168.4.1";  // Ajustar ao IP do RPi
const int   MQTT_PORT      = 1883;
const char* MQTT_CLIENT_ID = "ecobin-esp32cam";

// Tópicos MQTT
const char* TOPIC_CAM_STATUS = "ecobin/cam/status";
const char* TOPIC_CAM_READY  = "ecobin/cam/ready";
const char* TOPIC_SYS_STATUS = "ecobin/system/status";

// ── Pinout ESP32-CAM (AI-Thinker) ───────────
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// LED Flash integrado
#define FLASH_LED_PIN      4

// ── Objetos globais ─────────────────────────
WebServer httpServer(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ── Estado ──────────────────────────────────
bool systemReady = false;
unsigned long lastReconnectAttempt = 0;
unsigned long lastFrameCheck = 0;
const unsigned long FRAME_CHECK_INTERVAL = 2000; // Verificar a cada 2s

// Deteção de mudança na frame (Smart Gate)
uint8_t* previousFrame = NULL;
size_t previousFrameLen = 0;
const float CHANGE_THRESHOLD = 5.0; // Percentagem mínima de mudança

// ── Inicialização da Câmara ─────────────────
bool initCamera() {
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  // Resolução e qualidade JPEG
  // Com PSRAM: maior resolução possível
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;    // 640x480
    config.jpeg_quality = 10;               // 0-63 (menor = melhor)
    config.fb_count     = 2;
    Serial.println("[CAM] PSRAM encontrada — VGA 640x480");
  } else {
    config.frame_size   = FRAMESIZE_QVGA;   // 320x240
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    Serial.println("[CAM] Sem PSRAM — QVGA 320x240");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] ❌ Erro ao iniciar câmara: 0x%x\n", err);
    return false;
  }

  // Ajustar configurações do sensor
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor) {
    sensor->set_brightness(sensor, 1);    // Brilho ligeiramente acima
    sensor->set_contrast(sensor, 1);      // Contraste ligeiramente acima
    sensor->set_saturation(sensor, 0);    // Saturação neutra
    sensor->set_whitebal(sensor, 1);      // White balance automático
    sensor->set_awb_gain(sensor, 1);      // AWB gain
    sensor->set_exposure_ctrl(sensor, 1); // Exposição automática
  }

  Serial.println("[CAM] ✅ Câmara inicializada com sucesso!");
  return true;
}

// ── Conexão WiFi ────────────────────────────
void connectWiFi() {
  Serial.printf("[WiFi] A conectar a '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] ✅ Conectado! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] ❌ Falha na conexão WiFi!");
    ESP.restart();
  }
}

// ── MQTT Callbacks ──────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.printf("[MQTT] 📩 %s → %s\n", topic, message.c_str());

  // Processar comandos recebidos
  if (String(topic) == TOPIC_SYS_STATUS) {
    if (message == "idle") {
      systemReady = true;
    } else {
      systemReady = false;
    }
  }
}

bool mqttConnect() {
  Serial.printf("[MQTT] A conectar ao broker %s:%d...\n", MQTT_BROKER, MQTT_PORT);

  // Last Will: se desconectar, publica "offline"
  if (mqttClient.connect(
        MQTT_CLIENT_ID,
        NULL, NULL,              // username, password
        TOPIC_CAM_STATUS,        // will topic
        1,                       // will QoS
        true,                    // will retain
        "offline"                // will payload
      )) {
    Serial.println("[MQTT] ✅ Conectado ao broker!");

    // Publicar status online
    mqttClient.publish(TOPIC_CAM_STATUS, "online", true);

    // Subscrever tópicos de interesse
    mqttClient.subscribe(TOPIC_SYS_STATUS);

    systemReady = true;
    return true;
  } else {
    Serial.printf("[MQTT] ❌ Falha: rc=%d\n", mqttClient.state());
    return false;
  }
}

// ── HTTP: Endpoint /capture ─────────────────
// O Raspberry Pi chama este endpoint para obter um JPEG
void handleCapture() {
  Serial.println("[HTTP] 📷 Pedido de captura recebido!");

  // Ligar flash LED brevemente para iluminar
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(100);

  camera_fb_t* fb = esp_camera_fb_get();

  digitalWrite(FLASH_LED_PIN, LOW);

  if (!fb) {
    httpServer.send(500, "text/plain", "Erro: falha ao capturar frame");
    Serial.println("[HTTP] ❌ Falha ao capturar frame!");
    return;
  }

  // Enviar imagem JPEG como resposta HTTP
  httpServer.sendHeader("Content-Type", "image/jpeg");
  httpServer.sendHeader("Content-Length", String(fb->len));
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);

  Serial.printf("[HTTP] ✅ Imagem enviada: %d bytes (%.1f KB)\n",
                fb->len, fb->len / 1024.0);

  esp_camera_fb_return(fb);
}

// ── HTTP: Endpoint / (status) ───────────────
void handleRoot() {
  String html = "<!DOCTYPE html><html><body>";
  html += "<h1>ECOBIN - ESP32-CAM</h1>";
  html += "<p>Status: Online</p>";
  html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
  html += "<p><a href='/capture'>Capturar Imagem</a></p>";
  html += "<p><img src='/capture' width='640'></p>";
  html += "</body></html>";
  httpServer.send(200, "text/html", html);
}

// ── Deteção de Mudança (Smart Gate) ─────────
// Compara frames consecutivas para detetar se
// algo novo apareceu na bandeja
float calculateFrameChange(camera_fb_t* fb) {
  if (previousFrame == NULL || previousFrameLen == 0) {
    // Primeira frame — guardar e retornar 0
    if (previousFrame) free(previousFrame);
    previousFrame = (uint8_t*)malloc(fb->len);
    if (previousFrame) {
      memcpy(previousFrame, fb->buf, fb->len);
      previousFrameLen = fb->len;
    }
    return 0.0;
  }

  // Comparar tamanhos das frames JPEG
  // Mudanças significativas na cena alteram o tamanho do JPEG
  float sizeChange = abs((int)fb->len - (int)previousFrameLen);
  float percentChange = (sizeChange / (float)previousFrameLen) * 100.0;

  // Atualizar frame anterior
  if (previousFrame) free(previousFrame);
  previousFrame = (uint8_t*)malloc(fb->len);
  if (previousFrame) {
    memcpy(previousFrame, fb->buf, fb->len);
    previousFrameLen = fb->len;
  }

  return percentChange;
}

// ── Setup ───────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n");
  Serial.println("============================================");
  Serial.println("  ♻️  ECOBIN — Nó de Visão (ESP32-CAM)");
  Serial.println("============================================");

  // Flash LED como output
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  // 1. Inicializar câmara
  if (!initCamera()) {
    Serial.println("[FATAL] Câmara não inicializada. A reiniciar...");
    delay(3000);
    ESP.restart();
  }

  // 2. Conectar WiFi
  connectWiFi();

  // 3. Configurar HTTP server
  httpServer.on("/", handleRoot);
  httpServer.on("/capture", HTTP_GET, handleCapture);
  httpServer.begin();
  Serial.printf("[HTTP] ✅ Servidor ativo em http://%s/capture\n",
                WiFi.localIP().toString().c_str());

  // 4. Configurar MQTT
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512); // Mensagens de controlo são pequenas
  mqttConnect();

  Serial.println("\n[ECOBIN] 🟢 Sistema pronto!\n");
}

// ── Loop Principal ──────────────────────────
void loop() {
  // Manter HTTP server ativo
  httpServer.handleClient();

  // Manter MQTT conectado (reconexão automática)
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (mqttConnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    mqttClient.loop();
  }

  // Verificação periódica de mudança na frame (Smart Gate)
  if (systemReady && millis() - lastFrameCheck > FRAME_CHECK_INTERVAL) {
    lastFrameCheck = millis();

    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
      float change = calculateFrameChange(fb);
      esp_camera_fb_return(fb);

      if (change > CHANGE_THRESHOLD) {
        Serial.printf("[SMART GATE] 🔍 Mudança detetada: %.1f%% (threshold: %.1f%%)\n",
                      change, CHANGE_THRESHOLD);

        // Notificar gateway via MQTT
        mqttClient.publish(TOPIC_CAM_READY, "waste_detected", false);
        systemReady = false; // Esperar que o gateway processe

        Serial.println("[SMART GATE] 📤 Notificação enviada ao gateway");
      }
    }
  }
}
