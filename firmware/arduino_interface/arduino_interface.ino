// ============================================
// ECOBIN — Interface e Display (Arduino UNO R4 WiFi)
// ============================================
// Conecta-se ao WiFi e ao broker MQTT do Raspberry Pi.
// Recebe as classificações e mostra-as num ecrã OLED 
// SSD1306 (128x64 I2C), acumulando "ECO-PONTOS" de gamificação!
// ============================================

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h> // Instalar no Gestor de Bibliotecas!

// ── Configurações ───────────────────────────
const char* WIFI_SSID     = "ECOBIN_Hotspot";
const char* WIFI_PASSWORD  = "ecobin2026";
const char* MQTT_BROKER    = "192.168.1.100"; // ← IP do Raspberry Pi
const int   MQTT_PORT      = 1883;

// Tópicos MQTT
const char* TOPIC_CLASSIFICATION = "ecobin/classification";
const char* TOPIC_SYS_STATUS     = "ecobin/system/status";

// ── Display OLED (SSD1306 I2C) ──────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C // O endereço comum para este OLED 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── Estado Global ───────────────────────────
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

int totalEcoPoints = 0; // Os pontos acumulados do utilizador!
String lastClassification = "A aguardar...";
String currentSystemStatus = "idle";

// ── Funções do Display ──────────────────────
void updateDisplay() {
  display.clearDisplay();
  
  // Barra de Menu no topo
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("ECOBIN");
  
  display.setCursor(80, 0);
  display.print("PTS:");
  display.print(totalEcoPoints);
  
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Zona Central: Status ou Classificação
  if (currentSystemStatus == "classifying") {
    display.setTextSize(2);
    display.setCursor(5, 25);
    display.print("A PENSAR...");
  } 
  else {
    // Mostrar a última classificação recebida
    display.setTextSize(2);
    // Centrar um pouco o texto (aproximado)
    display.setCursor(0, 25);
    display.print(lastClassification);
    
    display.setTextSize(1);
    display.setCursor(0, 50);
    display.print("Pronto para o proximo!");
  }

  display.display();
}

void showPointsAnimation(int pointsGained) {
  // Uma animação rápida quando se ganha pontos
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 25);
  display.print("+");
  display.print(pointsGained);
  display.print(" ECO!");
  display.display();
  delay(2000); // mostrar por 2 segundos
  updateDisplay(); // Voltar ao ecrã normal
}

// ── Callback MQTT ───────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("\n[MQTT] Recebido no tópico: " + String(topic));
  Serial.println("[MQTT] Mensagem: " + message);

  // 1. Mudança de Estado do Sistema
  if (String(topic) == TOPIC_SYS_STATUS) {
    currentSystemStatus = message;
    updateDisplay();
  }

  // 2. Classificação Recebida
  if (String(topic) == TOPIC_CLASSIFICATION) {
    // O payload é um JSON (vem do Raspberry Pi)
    // Exemplo: {"category": "plastico", "confidence": 0.9, "description": "Garrafa", "points": 50}
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (!error) {
      String objName = doc["description"].as<String>(); // O "oled" ultra-curto do Gemini
      int points = doc["points"].as<int>();

      lastClassification = objName;
      if (points > 0) {
        totalEcoPoints += points;
        showPointsAnimation(points);
      } else {
        // Lixo não reciclável (0 pontos)
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(10, 25);
        display.print("REJEITADO!");
        display.display();
        delay(2000);
        updateDisplay();
      }
    } else {
      Serial.println("[MQTT] Erro a ler JSON!");
    }
  }
}

// ── Setup de Ligações ───────────────────────
void connectWiFi() {
  Serial.print("[WiFi] A ligar a ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Ligado! IP: " + WiFi.localIP().toString());
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] A ligar ao Broker...");
    if (mqttClient.connect("ecobin-arduino")) {
      Serial.println(" Ligado!");
      mqttClient.subscribe(TOPIC_CLASSIFICATION);
      mqttClient.subscribe(TOPIC_SYS_STATUS);
    } else {
      Serial.print(" Falhou, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" a tentar em 5s");
      delay(5000);
    }
  }
}

// ── Setup Principal ─────────────────────────
void setup() {
  Serial.begin(115200);

  // 1. Iniciar o Ecrã OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Falha ao alocar memória para o SSD1306"));
    for(;;); // Não avança se o OLED falhar
  }
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 25);
  display.print("ECOBIN");
  display.display();

  // 2. Ligar WiFi e MQTT
  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  updateDisplay();
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
}
