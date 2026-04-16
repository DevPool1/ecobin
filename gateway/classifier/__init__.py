# ============================================
# ECOBIN — Classificador de Resíduos (Gemini)
# ============================================
# Usa JSON mode nativo do Gemini para respostas
# estruturadas — menos tokens, mais fiável.
# ============================================

import json
import logging
from io import BytesIO

from PIL import Image
from google import genai
from google.genai import types

logger = logging.getLogger("ecobin.classifier")

# Prompt curto — o schema JSON faz o resto
CLASSIFICATION_PROMPT = (
    "Classifica o resíduo na imagem. "
    "Categorias: plastico, papel, vidro, organico, nao_reciclavel."
)

# Schema que o Gemini deve seguir (garante JSON válido)
RESPONSE_SCHEMA = {
    "type": "object",
    "properties": {
        "category": {
            "type": "string",
            "enum": ["plastico", "papel", "vidro", "organico", "nao_reciclavel"],
        },
        "confidence": {"type": "number"},
        "description": {"type": "string"},
        "recyclable": {"type": "boolean"},
    },
    "required": ["category", "confidence", "description", "recyclable"],
}


class WasteClassifier:
    """Classificador de resíduos usando Google Gemini com JSON mode.

    Usa response_mime_type="application/json" + response_schema
    para respostas estruturadas — ~70% menos tokens que prompt text.
    """

    def __init__(self, config):
        """Inicializa o classificador com a API key do Gemini.

        Args:
            config: Objeto Config com GEMINI_API_KEY e GEMINI_MODEL.
        """
        self.model_name = config.GEMINI_MODEL
        self.categories = config.WASTE_CATEGORIES

        # Inicializar cliente Gemini (SDK moderno google-genai)
        self.client = genai.Client(api_key=config.GEMINI_API_KEY)

        logger.info(f"🤖 Classificador inicializado (modelo: {self.model_name})")

    def classify(self, jpeg_bytes):
        """Classifica um resíduo a partir de uma imagem JPEG.

        Args:
            jpeg_bytes: Bytes da imagem JPEG capturada pelo ESP32-CAM.

        Returns:
            dict: Resultado com category, confidence, description,
                  recyclable, angle.
        """
        try:
            # Converter bytes JPEG para imagem PIL
            image = Image.open(BytesIO(jpeg_bytes))
            logger.info(
                f"🔍 A classificar imagem ({image.size[0]}x{image.size[1]})..."
            )

            # Chamar Gemini com JSON mode nativo
            response = self.client.models.generate_content(
                model=self.model_name,
                contents=[CLASSIFICATION_PROMPT, image],
                config=types.GenerateContentConfig(
                    response_mime_type="application/json",
                    response_schema=RESPONSE_SCHEMA,
                ),
            )

            # Parse direto — JSON mode garante formato válido
            result = json.loads(response.text)

            # Enriquecer com angle do carrossel
            result = self._enrich_result(result)

            logger.info(
                f"✅ Classificação: {result['category']} "
                f"({result['confidence']:.0%}) — {result['description']}"
            )

            return result

        except Exception as e:
            logger.error(f"❌ Erro na classificação: {e}")
            return self._fallback_result(str(e))

    def _enrich_result(self, result):
        """Valida e enriquece o resultado com o ângulo do carrossel.

        Args:
            result: Dict com o resultado do Gemini.

        Returns:
            dict: Resultado enriquecido com angle.
        """
        category = result.get("category", "nao_reciclavel")

        # Garantir que a categoria é válida
        if category not in self.categories and category != "nao_reciclavel":
            logger.warning(f"Categoria desconhecida: {category} → nao_reciclavel")
            category = "nao_reciclavel"

        # Garantir confiança entre 0 e 1
        confidence = max(0.0, min(1.0, float(result.get("confidence", 0.5))))

        # Obter ângulo do carrossel
        if category in self.categories:
            angle = self.categories[category]["angle"]
        else:
            angle = 0

        return {
            "category": category,
            "confidence": confidence,
            "description": result.get("description", "Objeto não identificado"),
            "recyclable": result.get("recyclable", False),
            "angle": angle,
        }

    def _fallback_result(self, error_msg):
        """Resultado de fallback quando a classificação falha."""
        return {
            "category": "nao_reciclavel",
            "confidence": 0.0,
            "description": f"Erro: {error_msg}",
            "recyclable": False,
            "angle": 0,
        }
