// ============================================
// ECOBIN — Interface Mestre (Arduino UNO R4 WiFi)
// ============================================
// Gere a interação humana (Ecrã OLED, Buzzer, NeoPixels).
// Usa o Sensor PIR/TIR para acordar o sistema e mandar tirar foto.
// Lê o Sensor Ultrassónico para reportar o nível do lixo.
// ============================================

#include <WiFiS3.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

// ── Configurações de Rede ───────────────────
const char* WIFI_SSID     = "ECOBIN_Hotspot";
const char* WIFI_PASSWORD  = "ecobin2026";
const char* MQTT_BROKER    = "192.168.1.100"; 
const int   MQTT_PORT      = 1883;

// ── Tópicos MQTT ────────────────────────────
const char* TOPIC_CLASSIFICATION = "ecobin/classification";
const char* TOPIC_SYS_STATUS     = "ecobin/system/status";
const char* TOPIC_CAM_READY      = "ecobin/cam/ready"; // Publicamos aqui quando o PIR deteta lixo
const char* TOPIC_FILL_LEVEL     = "ecobin/fill/level"; // Publicamos aqui a % do ultrassónico

// ── Pinos (Arduino UNO R4) ──────────────────
#define PIN_PIR       2   // Sensor Infravermelho/PIR
#define PIN_TRIG      3   // Sensor Ultrassónico (Emissor)
#define PIN_ECHO      4   // Sensor Ultrassónico (Receptor)
#define PIN_BUZZER    5   // Coluna de som / Buzzer
#define PIN_NEOPIXEL  6   // Fita de LEDs
#define NUM_PIXELS    16  // Ajusta se o teu anel tiver mais/menos LEDs!

// ── Hardware Instâncias ─────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ── Variáveis de Estado ─────────────────────
int totalEcoPoints = 0;
String lastClassification = "Sistema Pronto!";
String currentSystemStatus = "idle";
volatile bool motionDetected = false; 
unsigned long lastUltrasonicScan = 0;

// ============================================
// INTERRUPÇÃO (PIR / TIR)
// ============================================
void onMotion() {
  motionDetected = true;
}

// ============================================
// NEOPIXELS & BUZZER
// ============================================
void setStripColor(uint32_t color) {
  for(int i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void playSuccessSound() {
  tone(PIN_BUZZER, 1000, 100); delay(150);
  tone(PIN_BUZZER, 1500, 150);
}

void playErrorSound() {
  tone(PIN_BUZZER, 300, 500); 
}

// ============================================
// DISPLAY OLED
// ============================================
void updateDisplay() {
  display.clearDisplay();
  
  // Barra Superior
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("ECOBIN");
  display.setCursor(80, 0);
  display.print("PTS:");
  display.print(totalEcoPoints);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Zona Central
  if (currentSystemStatus == "classifying") {
    display.setTextSize(2);
    display.setCursor(5, 25);
    display.print("A PENSAR...");
  } else {
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.print("Ultimo detetado:");
    
    display.setTextSize(2);
    display.setCursor(0, 35);
    display.print(lastClassification);
  }
  display.display();
}

void showPointsAnimation(int pointsGained) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 25);
  display.print("+"); display.print(pointsGained); display.print(" ECO");
  display.display();
  delay(2000); 
  updateDisplay(); 
}

// ============================================
// MQTT & LÓGICA DE DADOS
// ============================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  
  Serial.print("[MQTT] ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(message.c_str());

  if (String(topic) == TOPIC_SYS_STATUS) {
    currentSystemStatus = message;
    updateDisplay();
    
    if (message == "classifying") {
      setStripColor(strip.Color(255, 255, 255)); // Branco ajuda a câmara!
    } else if (message == "idle") {
      setStripColor(strip.Color(0, 0, 0)); // Apaga os LEDs quando inativo
    }
  }

  if (String(topic) == TOPIC_CLASSIFICATION) {
    StaticJsonDocument<256> doc;
    if (!deserializeJson(doc, message)) {
      lastClassification = doc["description"].as<String>();
      String category = doc["category"].as<String>();
      int points = doc["points"].as<int>();

      // Pintar NeoPixels com a cor do ecoponto durante 3 seg
      if (category == "plastico") setStripColor(strip.Color(255, 255, 0)); // Amarelo
      else if (category == "papel") setStripColor(strip.Color(0, 0, 255)); // Azul
      else if (category == "vidro") setStripColor(strip.Color(0, 255, 0)); // Verde
      else setStripColor(strip.Color(50, 50, 50)); // Indiferenciado (Preto/Cinza Escuro)

      if (points > 0) {
        totalEcoPoints += points;
        playSuccessSound();
        showPointsAnimation(points);
      } else {
        playErrorSound();
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(10, 25);
        display.print("LIXO COMUM");
        display.display();
        delay(2000);
        updateDisplay();
      }
    }
  }
}

// ============================================
// SENSOR ULTRASSÓNICO (Mede o nivil de lixo)
// ============================================
void checkBinLevel() {
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duration = pulseIn(PIN_ECHO, HIGH, 30000); // 30ms timeout
  if (duration == 0) return;

  float distance_cm = duration * 0.034 / 2;
  
  // Converte a distância numa percentagem (ex: 40cm = vazio (0%), 5cm = cheio (100%))
  // AJUSTA ESTES VALORES À ALTURA DO TEU BALDE!
  float height_empty = 40.0;
  float height_full = 5.0;
  
  int percentage = map(distance_cm, height_empty, height_full, 0, 100);
  percentage = constrain(percentage, 0, 100);

  // Mandar para o Gateway
  String payload = String("{\"distance\":") + String(distance_cm) + ",\"fill_pct\":" + String(percentage) + "}";
  mqttClient.publish(TOPIC_FILL_LEVEL, payload.c_str());
  
  Serial.println("[SENSOR] Nível de enchimento reportado: " + String(percentage) + "%");
}

// ============================================
// SETUP & LOOP
// ============================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  // Associar o sensor PIR a uma interrupção de hardware (MUITA velocidade de reação!)
  attachInterrupt(digitalPinToInterrupt(PIN_PIR), onMotion, RISING);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED falhou!"); for(;;);
  }
  
  strip.begin();
  strip.show(); // Desliga todos
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 25);
  display.print("ECOBIN");
  display.display();

  Serial.println("\n[WiFi] A conectar...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" Ligado!");

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  updateDisplay();
}

void loop() {
  if (!mqttClient.connected()) {
    while (!mqttClient.connected()) {
      if (mqttClient.connect("ecobin-arduino")) {
        mqttClient.subscribe(TOPIC_CLASSIFICATION);
        mqttClient.subscribe(TOPIC_SYS_STATUS);
      } else delay(2000);
    }
  }
  mqttClient.loop();

  // Se o Sensor detetou ação E o sistema está parado a descansar:
  if (motionDetected) {
    motionDetected = false; 
    if (currentSystemStatus == "idle") {
      Serial.println("[PIR] Ojeto detetado! A acordar o Gateway...");
      mqttClient.publish(TOPIC_CAM_READY, "waste_detected");
      
      // O Gateway vai agora mudar o estado para "classifying"
      // o que vai acender a luz NeoPixel a branco automaticamente.
    }
  }

  // Verifica o nível do lixo a cada 10 segundos
  if (millis() - lastUltrasonicScan > 10000) {
    lastUltrasonicScan = millis();
    checkBinLevel();
  }
}
