# ============================================
# ECOBIN — Configuração Centralizada
# ============================================
# Carrega variáveis do .env e centraliza
# todas as configurações do sistema.
# ============================================

import os
from pathlib import Path
from dotenv import load_dotenv

# Carregar .env do diretório do gateway
_env_path = Path(__file__).parent / ".env"
load_dotenv(_env_path)


class Config:
    """Configuração centralizada do ECOBIN Gateway."""

    # --- MQTT Broker ---
    MQTT_BROKER_HOST = os.getenv("MQTT_BROKER_HOST", "localhost")
    MQTT_BROKER_PORT = int(os.getenv("MQTT_BROKER_PORT", "1883"))
    MQTT_CLIENT_ID = os.getenv("MQTT_CLIENT_ID", "ecobin-gateway")
    MQTT_USERNAME = os.getenv("MQTT_USERNAME", "")
    MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")

    # --- Tópicos MQTT ---
    class Topics:
        # ESP32-CAM → Gateway
        CAM_STATUS = "ecobin/cam/status"         # online/offline
        CAM_READY = "ecobin/cam/ready"           # waste_detected

        # Gateway → Todos
        CLASSIFICATION = "ecobin/classification"  # JSON resultado
        SYSTEM_STATUS = "ecobin/system/status"    # idle/classifying/sorting/error

        # Gateway → ESP32-WROOM (motor)
        MOTOR_COMMAND = "ecobin/motor/command"    # rotate:0|90|180|270
        SERVO_COMMAND = "ecobin/servo/command"    # open/close

        # ESP32-WROOM → Gateway
        MOTOR_STATUS = "ecobin/motor/status"      # done/error
        FILL_LEVEL = "ecobin/fill/level"          # JSON {compartment, percentage}

    # --- ESP32-CAM ---
    ESP32_CAM_IP = os.getenv("ESP32_CAM_IP", "192.168.4.2")
    ESP32_CAM_PORT = int(os.getenv("ESP32_CAM_PORT", "80"))
    ESP32_CAM_CAPTURE_URL = f"http://{ESP32_CAM_IP}:{ESP32_CAM_PORT}/capture"
    CAPTURE_TIMEOUT = int(os.getenv("CAPTURE_TIMEOUT", "5"))  # segundos
    CAPTURE_RETRIES = int(os.getenv("CAPTURE_RETRIES", "3"))

    # --- Google Gemini API ---
    GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "")
    GEMINI_MODEL = os.getenv("GEMINI_MODEL", "gemini-2.0-flash")

    # --- Categorias de Resíduos ---
    # Mapeamento categoria → ângulo do carrossel
    WASTE_CATEGORIES = {
        "plastico": {"angle": 0, "color": "#FFFF00", "label": "Plástico"},
        "papel": {"angle": 90, "color": "#0000FF", "label": "Papel/Cartão"},
        "vidro": {"angle": 180, "color": "#00FF00", "label": "Vidro"},
        "organico": {"angle": 270, "color": "#8B4513", "label": "Orgânico"},
    }

    # --- Base de Dados ---
    DATABASE_PATH = os.getenv("DATABASE_PATH", str(Path(__file__).parent / "database" / "ecobin.db"))

    # --- Web Dashboard ---
    WEB_HOST = os.getenv("WEB_HOST", "0.0.0.0")
    WEB_PORT = int(os.getenv("WEB_PORT", "5000"))

    # --- Logging ---
    LOG_LEVEL = os.getenv("LOG_LEVEL", "INFO")
    SAVE_IMAGES = os.getenv("SAVE_IMAGES", "true").lower() == "true"
    IMAGES_DIR = os.getenv("IMAGES_DIR", str(Path(__file__).parent / "captured_images"))

    @classmethod
    def validate(cls):
        """Valida que todas as configurações críticas estão definidas."""
        errors = []
        if not cls.GEMINI_API_KEY:
            errors.append("GEMINI_API_KEY não definida no .env")
        if not cls.ESP32_CAM_IP:
            errors.append("ESP32_CAM_IP não definida no .env")
        if errors:
            raise ValueError(
                "Configuração incompleta:\n" + "\n".join(f"  ❌ {e}" for e in errors)
            )
        return True

    @classmethod
    def print_config(cls):
        """Mostra a configuração atual (sem segredos)."""
        print("\n♻️  ECOBIN Gateway — Configuração")
        print("=" * 40)
        print(f"  MQTT Broker:  {cls.MQTT_BROKER_HOST}:{cls.MQTT_BROKER_PORT}")
        print(f"  ESP32-CAM:    {cls.ESP32_CAM_CAPTURE_URL}")
        print(f"  Gemini Model: {cls.GEMINI_MODEL}")
        print(f"  API Key:      {'✅ Definida' if cls.GEMINI_API_KEY else '❌ Em falta'}")
        print(f"  Database:     {cls.DATABASE_PATH}")
        print(f"  Save Images:  {cls.SAVE_IMAGES}")
        print("=" * 40)
