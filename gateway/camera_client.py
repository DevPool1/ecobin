# ============================================
# ECOBIN — Cliente HTTP para ESP32-CAM
# ============================================
# Captura imagens JPEG do ESP32-CAM via HTTP.
# Separado do MQTT (best practice: MQTT para
# controlo, HTTP para transferência de dados).
# ============================================

import logging
import time
from pathlib import Path
from datetime import datetime

import requests

logger = logging.getLogger("ecobin.camera")


class CameraClient:
    """Cliente HTTP para capturar imagens do ESP32-CAM.

    O ESP32-CAM serve um endpoint /capture que retorna
    um frame JPEG quando chamado via HTTP GET.
    """

    def __init__(self, config):
        """Inicializa o cliente da câmara.

        Args:
            config: Objeto Config com URL, timeout e retries.
        """
        self.capture_url = config.ESP32_CAM_CAPTURE_URL
        self.timeout = config.CAPTURE_TIMEOUT
        self.retries = config.CAPTURE_RETRIES
        self.save_images = config.SAVE_IMAGES
        self.images_dir = Path(config.IMAGES_DIR)

        # Criar diretório para imagens capturadas
        if self.save_images:
            self.images_dir.mkdir(parents=True, exist_ok=True)

    def capture(self):
        """Captura uma imagem JPEG do ESP32-CAM.

        Tenta até self.retries vezes em caso de falha.

        Returns:
            tuple: (jpeg_bytes, filepath) onde filepath é o caminho
                   do ficheiro guardado (ou None se save_images=False)

        Raises:
            ConnectionError: Se não conseguir capturar após todas as tentativas.
        """
        last_error = None

        for attempt in range(1, self.retries + 1):
            try:
                logger.info(
                    f"📷 A capturar imagem do ESP32-CAM "
                    f"(tentativa {attempt}/{self.retries})..."
                )

                response = requests.get(
                    self.capture_url,
                    timeout=self.timeout,
                    stream=True,
                )
                response.raise_for_status()

                # Verificar que é uma imagem JPEG
                content_type = response.headers.get("Content-Type", "")
                if "image" not in content_type and len(response.content) < 100:
                    raise ValueError(
                        f"Resposta inesperada: Content-Type={content_type}, "
                        f"tamanho={len(response.content)} bytes"
                    )

                jpeg_bytes = response.content
                logger.info(
                    f"✅ Imagem capturada: {len(jpeg_bytes)} bytes "
                    f"({len(jpeg_bytes) / 1024:.1f} KB)"
                )

                # Guardar no disco (opcional)
                filepath = None
                if self.save_images:
                    filepath = self._save_image(jpeg_bytes)

                return jpeg_bytes, filepath

            except requests.exceptions.Timeout:
                last_error = f"Timeout após {self.timeout}s"
                logger.warning(f"⏱️  {last_error} (tentativa {attempt})")

            except requests.exceptions.ConnectionError as e:
                last_error = f"Conexão recusada: {e}"
                logger.warning(f"🔌 {last_error} (tentativa {attempt})")

            except Exception as e:
                last_error = str(e)
                logger.warning(f"❌ Erro: {last_error} (tentativa {attempt})")

            # Esperar antes de tentar novamente
            if attempt < self.retries:
                time.sleep(1)

        raise ConnectionError(
            f"Falha ao capturar imagem após {self.retries} tentativas. "
            f"Último erro: {last_error}"
        )

    def _save_image(self, jpeg_bytes):
        """Guarda a imagem JPEG no disco.

        Args:
            jpeg_bytes: Bytes da imagem JPEG.

        Returns:
            Path: Caminho absoluto do ficheiro guardado.
        """
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"capture_{timestamp}.jpg"
        filepath = self.images_dir / filename
        filepath.write_bytes(jpeg_bytes)
        logger.debug(f"💾 Imagem guardada: {filepath}")
        return filepath

    def is_available(self):
        """Verifica se o ESP32-CAM está acessível.

        Returns:
            bool: True se a câmara responde, False caso contrário.
        """
        try:
            response = requests.get(
                self.capture_url.replace("/capture", "/"),
                timeout=2,
            )
            return response.status_code == 200
        except Exception:
            return False
