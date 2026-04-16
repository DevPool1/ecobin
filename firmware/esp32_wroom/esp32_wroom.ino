// ============================================
// ECOBIN — Nó de Contentor (ESP32-WROOM)
// ============================================
// Responsável por toda a parte MUSCULAR do ecoponto.
// Recebe ordens via MQTT e atua nos motores.
//
// Hardware Físico: 
// 1x Motor de Passo 28BYJ-48 + ULN2003 (Roda a Plataforma)
// 1x Servo Motor SG90 (Abre e fecha a bandeja/porta)
// 1x Módulo Relé HL-51 (Desliga a energia do Stepper para não aquecer)
// ============================================

#include <WiFi.h>
#include <PubSubClient.h>
#include <Stepper.h>
#include <ESP32Servo.h>

// ── Configurações de Rede ───────────────────
const char* WIFI_SSID     = "ECOBIN_Hotspot";
const char* WIFI_PASSWORD  = "ecobin2026";
const char* MQTT_BROKER    = "192.168.1.100";  // Mudar para o IP final do Raspberry Pi
const int   MQTT_PORT      = 1883;

// ── Tópicos MQTT ────────────────────────────
const char* TOPIC_MOTOR_COMMAND = "ecobin/motor/command";
const char* TOPIC_SERVO_COMMAND = "ecobin/servo/command";
const char* TOPIC_SYSTEM_STATUS = "ecobin/system/status";

// ── Pinos (GPIO do ESP32-WROOM) ─────────────
#define RELAY_PIN     19  // Módulo Relé
#define SERVO_PIN     18  // Servo SG90

// Pinos para o Driver ULN2003 (IN1, IN2, IN3, IN4)
#define STEP1_PIN     25
#define STEP2_PIN     26
#define STEP3_PIN     27
#define STEP4_PIN     14

// ── Motor de Passo (Carrossel) ──────────────
// O motor 28BYJ-48 tem 2048 passos por volta completa (360º)
const int STEPS_PER_REV = 2048; 
Stepper carrossel(STEPS_PER_REV, STEP1_PIN, STEP3_PIN, STEP2_PIN, STEP4_PIN);

int currentAngle = 0; // Guardamos o ângulo em que estamos (0 = Plástico)

// ── Servo (Bandeja / Alçapão) ───────────────
Servo trapdoor;
const int SERVO_FECHADO = 0;   // Ângulo para manter lixo na bandeja
const int SERVO_ABERTO  = 90;  // Ângulo para atirar o lixo para o ecoponto

// ── Instâncias Globais ──────────────────────
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ============================================
// FUNÇÕES MECÂNICAS
// ============================================

// Usamos o relé apenas no exato momento em que o disco tem de rodar!
// Motores de passo gastam muita energia e ficam a ferver se o relé não cortar.
void moveCarouselTo(int targetAngle) {
  // 1. Ligar a energia do motor via Relé
  digitalWrite(RELAY_PIN, HIGH); // Ajustar HIGH/LOW dependendo se o Relé é Ativo-Alto ou Baixo
  delay(100); 

  // 2. Calcular quantos passos dar (positivo = direita, negativo = esquerda)
  int diffAngle = targetAngle - currentAngle;
  
  // Otimização: Rotação pelo caminho mais curto!
  if (diffAngle > 180) diffAngle -= 360;
  if (diffAngle < -180) diffAngle += 360;
  
  float stepsToMove = (diffAngle / 360.0) * STEPS_PER_REV;
  
  Serial.printf("[MOTOR] A mover de %dº para %dº (%d passos)...\n", currentAngle, targetAngle, (int)stepsToMove);
  
  // 3. Rodar
  carrossel.step(stepsToMove);
  currentAngle = targetAngle; // Atualizamos a memória
  
  // 4. Desligar a energia via Relé
  delay(100);
  digitalWrite(RELAY_PIN, LOW);
  
  Serial.println("[MOTOR] Chegou ao destino.");
}

void controlTrapdoor(String cmd) {
  if (cmd == "open") {
    Serial.println("[SERVO] A abrir a porta...");
    trapdoor.write(SERVO_ABERTO);
  } else if (cmd == "close") {
    Serial.println("[SERVO] A fechar a porta...");
    trapdoor.write(SERVO_FECHADO);
  }
}

// ============================================
// REDE & MQTT
// ============================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) { message += (char)payload[i]; }
  
  Serial.printf("[MQTT] Comando: %s -> %s\n", topic, message.c_str());

  if (String(topic) == TOPIC_MOTOR_COMMAND) {
    if (message.startsWith("rotate:")) {
      int targetAngle = message.substring(7).toInt();
      moveCarouselTo(targetAngle);
    }
  }
  else if (String(topic) == TOPIC_SERVO_COMMAND) {
    controlTrapdoor(message);
  }
}

void setup() {
  Serial.begin(115200);

  // 1. Configurar Pinos
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Relé desligado por defeito

  // O Servo precisa associar o pino físico
  trapdoor.setPeriodHertz(50); 
  trapdoor.attach(SERVO_PIN, 500, 2400); 
  trapdoor.write(SERVO_FECHADO); // Fechar logo ao arrancar a máquina

  carrossel.setSpeed(15); // RPM (Não colocar muito alto no ULN2003)

  // 2. WiFi
  Serial.println("\n[WiFi] A conectar...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" Ligado!");

  // 3. MQTT
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  if (!mqttClient.connected()) {
    while (!mqttClient.connected()) {
      Serial.print("[MQTT] A ligar ao Broker...");
      if (mqttClient.connect("ecobin-wroom")) {
        Serial.println(" Ligado!");
        mqttClient.subscribe(TOPIC_MOTOR_COMMAND);
        mqttClient.subscribe(TOPIC_SERVO_COMMAND);
      } else {
        Serial.println(" Falhou, a tentar em 5s");
        delay(5000);
      }
    }
  }
  mqttClient.loop();
}
