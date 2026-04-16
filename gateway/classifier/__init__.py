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

# Prompt estruturado para classificação de resíduos
CLASSIFICATION_PROMPT = """Analisa esta imagem de um resíduo depositado num contentor inteligente.

Classifica o resíduo numa das seguintes categorias:
- "plastico" — garrafas, embalagens plásticas, sacos, copos plásticos
- "papel" — papel, cartão, jornais, revistas, caixas de cartão
- "vidro" — garrafas de vidro, frascos, copos de vidro
- "organico" — restos de comida, cascas, folhas, resíduos biodegradáveis
- "nao_reciclavel" — resíduos que não encaixam em nenhuma categoria acima

Responde APENAS com um JSON válido no seguinte formato exato, sem markdown, sem código, sem explicações:
{"category": "categoria_aqui", "confidence": 0.95, "description": "descrição curta do objeto", "recyclable": true}

Regras:
- "category" deve ser uma das 5 categorias acima (em minúsculas, sem acentos exceto em "plastico")
- "confidence" deve ser um número entre 0.0 e 1.0
- "description" deve ser uma descrição curta em português do objeto identificado
- "recyclable" deve ser true para plastico/papel/vidro e false para organico/nao_reciclavel
- Se a imagem não contiver um resíduo claro, responde com category "nao_reciclavel" e confidence baixa
"""


class WasteClassifier:
    """Classificador de resíduos usando Google Gemini 2.0 Flash.

    Recebe uma imagem JPEG e retorna a classificação
    com categoria, confiança e descrição.
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
            dict: Resultado da classificação com chaves:
                - category (str): Categoria do resíduo
                - confidence (float): Nível de confiança (0-1)
                - description (str): Descrição do objeto
                - recyclable (bool): Se é reciclável
                - angle (int): Ângulo do carrossel para esta categoria

        Raises:
            Exception: Se a classificação falhar.
        """
        try:
            # Converter bytes JPEG para imagem PIL
            image = Image.open(BytesIO(jpeg_bytes))
            logger.info(
                f"🔍 A classificar imagem ({image.size[0]}x{image.size[1]})..."
            )

            # Chamar Gemini 2.0 Flash com imagem + prompt
            response = self.client.models.generate_content(
                model=self.model_name,
                contents=[CLASSIFICATION_PROMPT, image],
            )

            # Extrair e parsear resposta JSON
            raw_text = response.text.strip()
            logger.debug(f"Resposta bruta do Gemini: {raw_text}")

            # Limpar possíveis artefactos markdown (```json ... ```)
            if raw_text.startswith("```"):
                raw_text = raw_text.split("\n", 1)[-1]  # remover primeira linha
                raw_text = raw_text.rsplit("```", 1)[0]  # remover última linha
                raw_text = raw_text.strip()

            result = json.loads(raw_text)

            # Validar e enriquecer resultado
            result = self._validate_result(result)

            logger.info(
                f"✅ Classificação: {result['category']} "
                f"({result['confidence']:.0%}) — {result['description']}"
            )

            return result

        except json.JSONDecodeError as e:
            logger.error(f"❌ Resposta do Gemini não é JSON válido: {e}")
            logger.error(f"   Resposta bruta: {raw_text}")
            return self._fallback_result("Erro ao interpretar resposta da IA")

        except Exception as e:
            logger.error(f"❌ Erro na classificação: {e}")
            return self._fallback_result(str(e))

    def _validate_result(self, result):
        """Valida e enriquece o resultado da classificação.

        Args:
            result: Dict com o resultado bruto do Gemini.

        Returns:
            dict: Resultado validado e enriquecido com angle.
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
            angle = 0  # posição padrão para não reciclável

        return {
            "category": category,
            "confidence": confidence,
            "description": result.get("description", "Objeto não identificado"),
            "recyclable": result.get("recyclable", False),
            "angle": angle,
        }

    def _fallback_result(self, error_msg):
        """Resultado de fallback quando a classificação falha.

        Args:
            error_msg: Mensagem de erro para o log.

        Returns:
            dict: Resultado padrão (não reciclável, baixa confiança).
        """
        return {
            "category": "nao_reciclavel",
            "confidence": 0.0,
            "description": f"Erro: {error_msg}",
            "recyclable": False,
            "angle": 0,
        }
