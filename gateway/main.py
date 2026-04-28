# ============================================
# ECOBIN — Gateway Inteligente (Raspberry Pi)
# ============================================
# Ponto de entrada do sistema gateway.
# Orquestra: Broker MQTT + ESP32-CAM + Gemini + SQLite
#
# Baseado no trabalho existente em ~/lixo-ia/
# ============================================

"""
ECOBIN Gateway — Entry Point

Sequência de operação:
1. Inicia broker MQTT embutido (amqtt — sem precisar de Mosquitto)
2. Inicia cliente MQTT e subscreve tópicos
3. Quando ESP32-CAM notifica resíduo detetado:
   a. Captura imagem via HTTP GET ao ESP32-CAM
   b. Classifica via Gemini 2.0 Flash
   c. Publica resultado no MQTT
   d. Regista na base de dados SQLite
4. Aguarda próximo evento
"""

import asyncio
import json
import time
import signal
import logging
import sys
import threading

from config import Config
from broker import start_broker, stop_broker
from mqtt import MQTTClient
from camera_client import CameraClient
from classifier import WasteClassifier
from database import Database

# Configurar logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s — %(message)s",
    datefmt="%H:%M:%S",
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler("ecobin_gateway.log", encoding="utf-8"),
    ],
)
logger = logging.getLogger("ecobin.main")


class EcobinGateway:
    """Orquestrador principal do gateway ECOBIN."""

    def __init__(self):
        """Inicializa todos os componentes do gateway."""
        logger.info("♻️  ECOBIN Gateway — A iniciar...")

        # Validar configuração
        Config.validate()
        Config.print_config()

        # Inicializar componentes
        self.mqtt = MQTTClient(Config)
        self.camera = CameraClient(Config)
        self.classifier = WasteClassifier(Config)
        self.db = Database(Config)

        # Estado do sistema
        self._processing = False
        self._broker = None

        # Registar callback para quando o ESP32-CAM deteta resíduo
        self.mqtt.set_waste_detected_callback(self._on_waste_detected)

    def start(self):
        """Arranca o broker MQTT e o gateway."""
        try:
            # 1. Iniciar broker MQTT embutido (amqtt)
            logger.info("🔄 A iniciar broker MQTT embutido...")
            self._broker_thread = threading.Thread(target=self._run_broker, daemon=True)
            self._broker_thread.start()

            # Esperar que o broker arranque
            time.sleep(2)

            # 2. Conectar cliente MQTT ao broker local
            self.mqtt.connect()

            # 3. Registar evento de startup
            self.db.log_event("startup", "Gateway iniciado com sucesso")

            logger.info("🟢 Gateway ativo — à espera de resíduos...")
            logger.info("   Prima Ctrl+C para parar.\n")

            # Loop principal
            while True:
                time.sleep(0.1)

        except KeyboardInterrupt:
            logger.info("\n⛔ A encerrar gateway...")
        except Exception as e:
            logger.error(f"❌ Erro fatal: {e}")
            import traceback

            traceback.print_exc()
            self.db.log_event("error", str(e))
        finally:
            self._shutdown()

    def _run_broker(self):
        """Corre o broker MQTT numa thread separada com o seu próprio event loop."""
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        try:
            self._broker = loop.run_until_complete(start_broker())
            loop.run_forever()
        except Exception as e:
            logger.error(f"❌ Erro no broker: {e}")

    def _on_waste_detected(self, payload):
        """Callback chamado quando o ESP32-CAM deteta um resíduo.

        Sequência:
        1. Captura imagem via HTTP
        2. Classifica via Gemini
        3. Publica resultado no MQTT
        4. Regista na base de dados

        Args:
            payload: Mensagem do tópico ecobin/cam/ready.
        """
        # Evitar processamento concorrente
        if self._processing:
            logger.warning("⚠️  Já a processar — ignorar novo pedido.")
            return

        self._processing = True
        self.mqtt.publish(Config.Topics.SYSTEM_STATUS, "classifying")

        try:
            # ── 1. Capturar imagem ───────────────────
            logger.info("=" * 50)
            logger.info("🔄 NOVA CLASSIFICAÇÃO")
            logger.info("=" * 50)

            jpeg_bytes, image_path = self.camera.capture()

            # ── 2. Classificar com Gemini ────────────
            result = self.classifier.classify(jpeg_bytes)

            # ── 3. Publicar resultado via MQTT ───────
            self.mqtt.publish(
                Config.Topics.CLASSIFICATION,
                result,
            )

            # ── 4. Comandar motor (Sempre move para a categoria) ──
            motor_cmd = f"rotate:{result['angle']}"
            self.mqtt.publish(Config.Topics.MOTOR_COMMAND, motor_cmd)
            logger.info(f"⚙️  Motor → {motor_cmd} ({result['category']})")

            # Esperar um pouco e abrir servo
            time.sleep(2)
            self.mqtt.publish(Config.Topics.SERVO_COMMAND, "open")
            time.sleep(1)
            self.mqtt.publish(Config.Topics.SERVO_COMMAND, "close")

            # ── 5. Registar na base de dados ─────────
            record_id = self.db.log_classification(result, image_path)

            logger.info(f"💾 Registado: ID={record_id}")
            logger.info(
                f"📋 Resultado: {result['category']} "
                f"({result['confidence']:.0%}) — {result['description']}"
            )
            logger.info("=" * 50 + "\n")

        except ConnectionError as e:
            logger.error(f"📷 Erro de câmara: {e}")
            self.mqtt.publish(Config.Topics.SYSTEM_STATUS, "error")
            self.db.log_event("error", f"Camera: {e}")

        except Exception as e:
            logger.error(f"❌ Erro na classificação: {e}")
            import traceback

            traceback.print_exc()
            self.mqtt.publish(Config.Topics.SYSTEM_STATUS, "error")
            self.db.log_event("error", str(e))

        finally:
            self._processing = False
            self.mqtt.publish(Config.Topics.SYSTEM_STATUS, "idle")

    def _shutdown(self):
        """Encerra todos os componentes de forma limpa."""
        self.db.log_event("shutdown", "Gateway encerrado")
        self.mqtt.disconnect()
        logger.info("👋 Gateway encerrado.")


# ── Ponto de entrada ────────────────────────
if __name__ == "__main__":
    gateway = EcobinGateway()
    gateway.start()
