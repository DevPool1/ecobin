// ============================================
// ECOBIN — Nó de Visão (ESP32-CAM)
// ============================================
// Captura frames JPEG do resíduo na bandeja
// e serve via endpoint HTTP para o Raspberry Pi.
//
// Hardware: ESP32-CAM com câmara OV2640
// Comunicação: HTTP server + MQTT client
// ============================================

// TODO: Implementar firmware do ESP32-CAM
// - Inicializar câmara OV2640
// - Criar servidor HTTP com endpoint /capture
// - Conectar ao Wi-Fi
// - Conectar ao broker MQTT (Mosquitto no RPi)
// - Publicar estado no tópico MQTT

void setup() {
  Serial.begin(115200);
  Serial.println("[ECOBIN] Nó de Visão — ESP32-CAM");
  // TODO: Inicialização
}

void loop() {
  // TODO: Lógica principal
}
