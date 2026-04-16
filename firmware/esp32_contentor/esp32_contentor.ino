// ============================================
// ECOBIN — Nó de Contentor (ESP32-WROOM)
// ============================================
// Controla o mecanismo físico de triagem:
// motor de passo, servo, sensores.
//
// Hardware: ESP32-WROOM-32UE
// Atuadores: Motor 28BYJ-48, Servo SG90
// Sensores: HC-SR04, Reed Switch, PIR, LDR
// Feedback: NeoPixel WS2812B, Buzzer
// Comunicação: MQTT client
// ============================================

// TODO: Implementar firmware do ESP32-WROOM
// - Motor de passo: rotação do carrossel (0°/90°/180°/270°)
// - Servo SG90: abrir/fechar bandeja (trapdoor)
// - HC-SR04: medir nível de enchimento
// - Reed Switch: confirmar estado da bandeja
// - PIR: deteção de proximidade
// - NeoPixel: feedback cromático por categoria
// - Buzzer: feedback sonoro
// - LDR: modo noturno
// - MQTT: receber comandos e publicar estados

void setup() {
  Serial.begin(115200);
  Serial.println("[ECOBIN] Nó de Contentor — ESP32-WROOM");
  // TODO: Inicialização
}

void loop() {
  // TODO: Lógica principal
}
