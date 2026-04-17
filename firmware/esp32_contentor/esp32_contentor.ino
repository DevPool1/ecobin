// ============================================
// ECOBIN — Nó de Contentor (ESP32-WROOM-32UE)
// ============================================
// Recebe comandos MQTT do Gateway e controla:
//   - Motor de Passo 28BYJ-48 (carrossel 4 compartimentos)
//   - Servo SG90 (bandeja de triagem / trapdoor)
//   - HC-SR04 (nível de enchimento)
//   - Sensor PIR (deteção de proximidade)
// ============================================

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// ── Configurações de Rede ───────────────────
const char* WIFI_SSID     = "NOME_DO_TEU_WIFI";    // <- Alterar
const char* WIFI_PASSWORD  = "PASS_DO_TEU_WIFI";   // <- Alterar
const char* MQTT_BROKER    = "192.168.1.X";         // <- IP do Raspberry Pi ou PC com gateway
const int   MQTT_PORT      = 1883;

// ── Tópicos MQTT ────────────────────────────
const char* TOPIC_MOTOR   = "ecobin/motor/command";   // Ex: "rotate:90"
const char* TOPIC_SERVO   = "ecobin/servo/command";   // "open" ou "close"
const char* TOPIC_FILL    = "ecobin/fill/level";      // Publica nível de enchimento
const char* TOPIC_STATUS  = "ecobin/contentor/status"; // Publica estado interno

// ── Pinos: Motor de Passo (28BYJ-48 + ULN2003) ─
// Ligar: IN1=GPIO26, IN2=GPIO27, IN3=GPIO14, IN4=GPIO12
#define STEPPER_IN1  26
#define STEPPER_IN2  27
#define STEPPER_IN3  14
#define STEPPER_IN4  12

// ── Pinos: Servo SG90 ───────────────────────
#define SERVO_PIN    13
#define SERVO_FECHADO 0    // Ângulo de bandeja fechada (ajustar após testar!)
#define SERVO_ABERTO  90   // Ângulo de bandeja aberta (ajustar após testar!)

// ── Pinos: HC-SR04 (Ultrassónico) ──────────
#define PIN_TRIG     4
#define PIN_ECHO     5

// ── Pinos: Sensor PIR ───────────────────────
#define PIN_PIR      15

// ── Constantes do Motor de Passo ───────────
// O 28BYJ-48 faz 2048 passos por rotação completa (modo 4-fases)
// 90° = 512 passos | 180° = 1024 passos | 270° = 1536 passos
#define PASSOS_POR_ROTACAO  2048
#define PASSOS_POR_90_GRAUS  512
#define VELOCIDADE_MOTOR     800  // Microsegundos entre passos (mais baixo = mais rápido)

// ── Estado do Carrossel ─────────────────────
// Mapeia categoria -> posição em graus
// IMPORTANTE: ajustar esta ordem consoante a disposição física do teu carrossel!
// 0°=Plástico | 90°=Papel | 180°=Vidro | 270°=Indiferenciado
int posicaoAtualGraus = 0;

// Sequência de meio-passo para o 28BYJ-48 (8 fases -> mais suave e com mais torque)
const int SEQUENCIA[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};
int faseAtual = 0;

// ── Instâncias ──────────────────────────────
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Servo servo;

// ── Variáveis de Estado ─────────────────────
unsigned long ultimoScanFill = 0;
bool sistemaOcupado = false;

// ============================================
// MOTOR DE PASSO — Funções de Controlo
// ============================================

void desligarMotor() {
  // Desliga as bobines para não aquecer quando parado
  digitalWrite(STEPPER_IN1, LOW);
  digitalWrite(STEPPER_IN2, LOW);
  digitalWrite(STEPPER_IN3, LOW);
  digitalWrite(STEPPER_IN4, LOW);
}

void darUmPasso(bool direcaoHoraria) {
  if (direcaoHoraria) {
    faseAtual = (faseAtual + 1) % 8;
  } else {
    faseAtual = (faseAtual + 7) % 8;
  }
  digitalWrite(STEPPER_IN1, SEQUENCIA[faseAtual][0]);
  digitalWrite(STEPPER_IN2, SEQUENCIA[faseAtual][1]);
  digitalWrite(STEPPER_IN3, SEQUENCIA[faseAtual][2]);
  digitalWrite(STEPPER_IN4, SEQUENCIA[faseAtual][3]);
  delayMicroseconds(VELOCIDADE_MOTOR);
}

void rodarParaAngulo(int anguloDestino) {
  // Normalizar ângulo entre 0 e 359
  anguloDestino = ((anguloDestino % 360) + 360) % 360;

  // Calcular diferença mais curta (horária ou anti-horária)
  int diferenca = anguloDestino - posicaoAtualGraus;
  if (diferenca > 180)  diferenca -= 360;
  if (diferenca < -180) diferenca += 360;

  if (diferenca == 0) {
    Serial.println("[MOTOR] Já na posição correta, sem movimento.");
    return;
  }

  // Converter graus em passos (com 8 fases: 4096 passos por rotação)
  int numeroPassos = (int)(abs(diferenca) / 360.0 * 4096);
  bool horario = (diferenca > 0);

  Serial.printf("[MOTOR] Rodar %d° %s (%d passos)\n",
    abs(diferenca), horario ? "horário" : "anti-horário", numeroPassos);

  for (int i = 0; i < numeroPassos; i++) {
    darUmPasso(horario);
  }

  desligarMotor();
  posicaoAtualGraus = anguloDestino;
  Serial.printf("[MOTOR] Posição atual: %d°\n", posicaoAtualGraus);
}

// ============================================
// SERVO — Funções de Controlo
// ============================================

void abrirBandeja() {
  Serial.println("[SERVO] A abrir bandeja...");
  servo.write(SERVO_ABERTO);
  delay(500); // Dar tempo para o servo mover
}

void fecharBandeja() {
  Serial.println("[SERVO] A fechar bandeja...");
  servo.write(SERVO_FECHADO);
  delay(500);
}

// ============================================
// SENSOR ULTRASSÓNICO — Mede nível de enchimento
// ============================================

void reportarNivelEnchimento() {
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duracao = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duracao == 0) return;

  float distancia_cm = duracao * 0.034 / 2.0;

  // AJUSTAR à altura real do teu contentor!
  float altura_vazio = 30.0;  // cm quando vazio
  float altura_cheio = 3.0;   // cm quando cheio

  int percentagem = (int)((altura_vazio - distancia_cm) / (altura_vazio - altura_cheio) * 100);
  percentagem = constrain(percentagem, 0, 100);

  // Publicar via MQTT
  String payload = "{\"fill_pct\":" + String(percentagem) +
                   ",\"dist_cm\":" + String(distancia_cm) + "}";
  mqttClient.publish(TOPIC_FILL, payload.c_str());
  Serial.print("[SENSOR] Enchimento: ");
  Serial.print(percentagem);
  Serial.println("%");
}

// ============================================
// MQTT — Callback (Lógica principal de comandos)
// ============================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String mensagem;
  for (unsigned int i = 0; i < length; i++) mensagem += (char)payload[i];

  Serial.print("[MQTT] ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(mensagem);

  // ── Comando de Motor ─────────────────────
  if (String(topic) == TOPIC_MOTOR) {
    if (mensagem.startsWith("rotate:")) {
      int angulo = mensagem.substring(7).toInt();
      sistemaOcupado = true;
      mqttClient.publish(TOPIC_STATUS, "rotating");
      rodarParaAngulo(angulo);
      mqttClient.publish(TOPIC_STATUS, "idle");
      sistemaOcupado = false;
    }
  }

  // ── Comando de Servo ─────────────────────
  if (String(topic) == TOPIC_SERVO) {
    if (mensagem == "open") {
      abrirBandeja();
      mqttClient.publish(TOPIC_STATUS, "trapdoor_open");
    } else if (mensagem == "close") {
      fecharBandeja();
      mqttClient.publish(TOPIC_STATUS, "trapdoor_closed");
    }
  }
}

// ============================================
// SETUP
// ============================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n[ECOBIN] Nó de Contentor — A iniciar...");

  // Pinos do Motor de Passo
  pinMode(STEPPER_IN1, OUTPUT);
  pinMode(STEPPER_IN2, OUTPUT);
  pinMode(STEPPER_IN3, OUTPUT);
  pinMode(STEPPER_IN4, OUTPUT);
  desligarMotor();

  // Servo
  servo.attach(SERVO_PIN);
  fecharBandeja(); // Garantir posição fechada no arranque

  // HC-SR04
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  // Sensor PIR
  pinMode(PIN_PIR, INPUT);

  // Wi-Fi
  Serial.print("[WiFi] A ligar...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print(" Ligado! IP: ");
  Serial.println(WiFi.localIP());

  // MQTT
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512); // Buffer suficiente para JSON

  Serial.println("[ECOBIN] Nó de Contentor pronto e à espera de comandos.");
}

// ============================================
// LOOP
// ============================================

void loop() {
  // Reconectar ao MQTT se necessário
  if (!mqttClient.connected()) {
    Serial.print("[MQTT] A reconectar...");
    if (mqttClient.connect("ecobin-contentor", NULL, NULL,
                           TOPIC_STATUS, 1, true, "offline")) {
      mqttClient.publish(TOPIC_STATUS, "online", true);
      mqttClient.subscribe(TOPIC_MOTOR);
      mqttClient.subscribe(TOPIC_SERVO);
      Serial.println(" OK!");
    } else {
      Serial.print(" Falhou (rc=");
      Serial.print(mqttClient.state());
      Serial.println("). A tentar em 3s...");
      delay(3000);
      return;
    }
  }
  mqttClient.loop();

  // Reportar nível de enchimento a cada 15 segundos
  if (millis() - ultimoScanFill > 15000) {
    ultimoScanFill = millis();
    if (!sistemaOcupado) {
      reportarNivelEnchimento();
    }
  }
}
