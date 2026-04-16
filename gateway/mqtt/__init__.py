# ============================================
# ECOBIN — Cliente MQTT (Paho MQTT 2.0)
# ============================================
# Gere a comunicação MQTT entre o gateway e
# todos os nós do sistema (ESP32-CAM, ESP32-WROOM,
# Arduino R4 WiFi).
# ============================================

import json
import logging
import paho.mqtt.client as mqtt

logger = logging.getLogger("ecobin.mqtt")


class MQTTClient:
    """Cliente MQTT para o gateway ECOBIN.

    Usa paho-mqtt 2.0 com CallbackAPIVersion.VERSION2.
    Gere subscrições, publicações e callbacks.
    """

    def __init__(self, config):
        """Inicializa o cliente MQTT.

        Args:
            config: Objeto Config com as configurações do sistema.
        """
        self.config = config
        self._on_waste_detected = None  # callback externo

        # Criar cliente com API v2 (paho-mqtt >= 2.0)
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=config.MQTT_CLIENT_ID,
        )

        # Configurar autenticação (se definida)
        if config.MQTT_USERNAME:
            self.client.username_pw_set(config.MQTT_USERNAME, config.MQTT_PASSWORD)

        # Last Will Testament — se o gateway desconectar inesperadamente,
        # o broker publica esta mensagem automaticamente
        self.client.will_set(
            config.Topics.SYSTEM_STATUS,
            payload="offline",
            qos=1,
            retain=True,
        )

        # Registar callbacks internos
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

    def connect(self):
        """Conecta ao broker MQTT."""
        logger.info(
            f"A conectar ao broker MQTT em "
            f"{self.config.MQTT_BROKER_HOST}:{self.config.MQTT_BROKER_PORT}..."
        )
        self.client.connect(
            self.config.MQTT_BROKER_HOST,
            self.config.MQTT_BROKER_PORT,
            keepalive=60,
        )
        self.client.loop_start()

    def disconnect(self):
        """Desconecta do broker MQTT de forma limpa."""
        self.publish(self.config.Topics.SYSTEM_STATUS, "offline")
        self.client.loop_stop()
        self.client.disconnect()
        logger.info("Desconectado do broker MQTT.")

    def publish(self, topic, payload, qos=1, retain=False):
        """Publica uma mensagem num tópico MQTT.

        Args:
            topic: Tópico MQTT (ex: 'ecobin/classification')
            payload: Conteúdo da mensagem (str ou dict)
            qos: Quality of Service (0, 1 ou 2)
            retain: Se True, o broker retém a última mensagem
        """
        if isinstance(payload, dict):
            payload = json.dumps(payload)

        result = self.client.publish(topic, payload, qos=qos, retain=retain)
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            logger.debug(f"📤 Publicado em {topic}: {payload[:100]}")
        else:
            logger.error(f"❌ Erro ao publicar em {topic}: rc={result.rc}")

    def set_waste_detected_callback(self, callback):
        """Define o callback chamado quando o ESP32-CAM deteta resíduo.

        Args:
            callback: Função chamada com a mensagem como argumento.
        """
        self._on_waste_detected = callback

    # ── Callbacks internos ───────────────────

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        """Chamado quando a conexão ao broker é estabelecida."""
        if reason_code == 0:
            logger.info("✅ Conectado ao broker MQTT!")

            # Subscrever tópicos de interesse
            topics = [
                (self.config.Topics.CAM_STATUS, 1),
                (self.config.Topics.CAM_READY, 1),
                (self.config.Topics.MOTOR_STATUS, 1),
                (self.config.Topics.FILL_LEVEL, 1),
            ]
            for topic, qos in topics:
                client.subscribe(topic, qos)
                logger.debug(f"📥 Subscrito a: {topic}")

            # Anunciar que o gateway está online
            self.publish(
                self.config.Topics.SYSTEM_STATUS, "online", retain=True
            )
        else:
            logger.error(f"❌ Falha na conexão MQTT: {reason_code}")

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        """Chamado quando a conexão ao broker é perdida."""
        if reason_code != 0:
            logger.warning(f"⚠️  Desconexão inesperada do MQTT: {reason_code}")
        else:
            logger.info("Desconectado do MQTT (limpo).")

    def _on_message(self, client, userdata, msg):
        """Chamado quando uma mensagem é recebida num tópico subscrito."""
        topic = msg.topic
        try:
            payload = msg.payload.decode("utf-8")
        except UnicodeDecodeError:
            payload = msg.payload
            logger.warning(f"Payload binário em {topic} (não UTF-8)")

        logger.info(f"📩 Mensagem recebida: {topic} → {payload}")

        # Routing de mensagens por tópico
        if topic == self.config.Topics.CAM_READY:
            if self._on_waste_detected:
                self._on_waste_detected(payload)

        elif topic == self.config.Topics.CAM_STATUS:
            logger.info(f"📷 ESP32-CAM status: {payload}")

        elif topic == self.config.Topics.MOTOR_STATUS:
            logger.info(f"⚙️  Motor status: {payload}")

        elif topic == self.config.Topics.FILL_LEVEL:
            try:
                data = json.loads(payload)
                logger.info(
                    f"📊 Nível: compartimento {data.get('compartment')} "
                    f"→ {data.get('percentage')}%"
                )
            except json.JSONDecodeError:
                logger.warning(f"Payload inválido em {topic}: {payload}")
