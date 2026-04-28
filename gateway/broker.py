# ============================================
# ECOBIN — Broker MQTT Embutido (amqtt)
# ============================================
# Corre o broker MQTT como parte do gateway,
# sem necessidade de instalar Mosquitto.
# Baseado no broker.py do lixo-ia original.
# ============================================

import asyncio
import logging

logger = logging.getLogger("ecobin.broker")

# Configuração do broker amqtt
BROKER_CONFIG = {
    "listeners": {
        "default": {
            "type": "tcp",
            "bind": "0.0.0.0:1883",
        }
    },
    "sys_interval": 0,
    "auth": {
        "allow-anonymous": True,
    },
    "topic-check": {
        "enabled": False,
    },
}


async def start_broker():
    """Inicia o broker MQTT amqtt.

    Returns:
        Broker: Instância do broker a correr.
    """
    from amqtt.broker import Broker

    broker = Broker(BROKER_CONFIG)
    await broker.start()
    logger.info("✅ Broker MQTT (amqtt) a correr na porta 1883")
    return broker


async def stop_broker(broker):
    """Para o broker MQTT.

    Args:
        broker: Instância do broker a parar.
    """
    if broker:
        await broker.shutdown()
        logger.info("Broker MQTT parado.")
