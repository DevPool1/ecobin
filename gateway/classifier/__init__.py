# ============================================
# ECOBIN — Classificador de Resíduos (Gemini)
# ============================================
# Usa a API Google Gemini 2.0 Flash para
# classificar resíduos a partir de imagens JPEG.
# ============================================

import json
import logging
from io import BytesIO

from PIL import Image
from google import genai
from google.genai import types

logger = logging.getLogger("ecobin.classifier")

# Prompt otimizado para ecrã OLED (SSD1306 128x64) e Gamificação (Eco-points)
CLASSIFICATION_PROMPT = """Classifica o resíduo num destes contentores: plastico, papel, vidro, ou indiferenciado.
Responde APENAS com um JSON minificado:
{"cat":"plastico","oled":"Garrafa d'Agua","pts":50}

Regras:
1. "cat": OBRIGATORIAMENTE "plastico", "papel", "vidro" ou "indiferenciado".
2. "oled": Nome EXATO e específico do objeto (1 a 3 palavras, máx 20 letras). Tem de refletir o teu escrutínio físico do objeto!
3. "pts": Gamificação! plastico/papel/vidro = 50, indiferenciado = 0.
4. Orgânicos, lixo sujo (ex: guardanapos usados) ou não reciclável vão para "indiferenciado" (0 pts).
5. PRECISÃO EXTREMA DE MATERIAL: Analisa a textura da imagem detalhadamente. Distingue visualmente, por exemplo, um lenço de papel celulose (quebradiço) de uma toalhita húmida (tecido não-tecido tipo pano, elástico e sem brilho). O nome tem de ser o mais perito possível.
"""


class WasteClassifier:
    """Classificador de resíduos usando Google Gemini 2.0 Flash."""

    def __init__(self, config):
        self.model_name = config.GEMINI_MODEL
        self.categories = config.WASTE_CATEGORIES
        self.client = genai.Client(api_key=config.GEMINI_API_KEY)
        logger.info(f"🤖 Classificador inicializado (modelo: {self.model_name})")

    def classify(self, jpeg_bytes):
        try:
            image = Image.open(BytesIO(jpeg_bytes))
            logger.info(f"🔍 A classificar imagem ({image.size[0]}x{image.size[1]})...")

            response = self.client.models.generate_content(
                model=self.model_name,
                contents=[CLASSIFICATION_PROMPT, image],
            )

            raw_text = response.text.strip()
            logger.debug(f"Resposta bruta do Gemini: {raw_text}")

            if raw_text.startswith("```"):
                raw_text = raw_text.split("\n", 1)[-1]
                raw_text = raw_text.rsplit("```", 1)[0]
                raw_text = raw_text.strip()

            result = json.loads(raw_text)
            result = self._validate_result(result)

            logger.info(
                f"✅ Classificação: {result['category']} "
                f"| Ecrã: {result['description']} | Pontos: +{result['points']} ECO"
            )

            return result

        except Exception as e:
            logger.error(f"❌ Erro na classificação: {e}")
            return self._fallback_result(str(e))

    def _validate_result(self, result):
        category = result.get("cat", result.get("category", "indiferenciado"))
        if category not in self.categories and category != "indiferenciado":
            logger.warning(f"Categoria desconhecida: {category} → indiferenciado")
            category = "indiferenciado"

        return {
            "category": category,
            "confidence": (
                0.9 if category != "indiferenciado" else 0.0
            ),  # Simplificação para OLED
            "description": result.get("oled", "Objeto"),
            "recyclable": category in ["plastico", "papel", "vidro"],
            "angle": (
                self.categories[category]["angle"]
                if category in self.categories
                else 270
            ),
            "points": result.get("pts", 0),
        }

    def _fallback_result(self, error_msg):
        return {
            "category": "indiferenciado",
            "confidence": 0.0,
            "description": "Erro IA",
            "recyclable": False,
            "angle": 270,
            "points": 0,
        }
