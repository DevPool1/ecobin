# ============================================
# ECOBIN — Base de Dados SQLite
# ============================================
# Regista todas as classificações com timestamp,
# categoria, confiança e imagem associada.
# ============================================

import json
import sqlite3
import logging
from datetime import datetime
from pathlib import Path

logger = logging.getLogger("ecobin.database")

# Schema da base de dados
SCHEMA = """
CREATE TABLE IF NOT EXISTS classifications (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL DEFAULT (datetime('now', 'localtime')),
    category TEXT NOT NULL,
    confidence REAL NOT NULL,
    description TEXT,
    recyclable BOOLEAN NOT NULL DEFAULT 0,
    angle INTEGER NOT NULL DEFAULT 0,
    image_path TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now', 'localtime'))
);

CREATE TABLE IF NOT EXISTS fill_levels (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL DEFAULT (datetime('now', 'localtime')),
    compartment TEXT NOT NULL,
    percentage REAL NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now', 'localtime'))
);

CREATE TABLE IF NOT EXISTS system_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL DEFAULT (datetime('now', 'localtime')),
    event_type TEXT NOT NULL,
    details TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now', 'localtime'))
);
"""


class Database:
    """Gestor da base de dados SQLite do ECOBIN.

    Regista classificações, níveis de enchimento
    e eventos do sistema.
    """

    def __init__(self, config):
        """Inicializa a base de dados.

        Args:
            config: Objeto Config com DATABASE_PATH.
        """
        self.db_path = Path(config.DATABASE_PATH)
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        self._init_db()

    def _init_db(self):
        """Cria as tabelas se não existirem."""
        with sqlite3.connect(self.db_path) as conn:
            conn.executescript(SCHEMA)
            logger.info(f"💾 Base de dados inicializada: {self.db_path}")

    def log_classification(self, result, image_path=None):
        """Regista uma classificação na base de dados.

        Args:
            result: Dict com category, confidence, description, recyclable, angle.
            image_path: Caminho da imagem capturada (opcional).

        Returns:
            int: ID do registo criado.
        """
        with sqlite3.connect(self.db_path) as conn:
            cursor = conn.execute(
                """
                INSERT INTO classifications 
                    (category, confidence, description, recyclable, angle, image_path)
                VALUES (?, ?, ?, ?, ?, ?)
                """,
                (
                    result["category"],
                    result["confidence"],
                    result["description"],
                    result["recyclable"],
                    result["angle"],
                    str(image_path) if image_path else None,
                ),
            )
            record_id = cursor.lastrowid
            logger.debug(f"💾 Classificação registada: ID={record_id}")
            return record_id

    def log_fill_level(self, compartment, percentage):
        """Regista o nível de enchimento de um compartimento.

        Args:
            compartment: Nome do compartimento (ex: "plastico").
            percentage: Percentagem de enchimento (0-100).
        """
        with sqlite3.connect(self.db_path) as conn:
            conn.execute(
                "INSERT INTO fill_levels (compartment, percentage) VALUES (?, ?)",
                (compartment, percentage),
            )
            logger.debug(f"📊 Nível registado: {compartment} → {percentage}%")

    def log_event(self, event_type, details=None):
        """Regista um evento do sistema.

        Args:
            event_type: Tipo de evento (ex: "startup", "error", "classification").
            details: Detalhes adicionais (string ou dict).
        """
        if isinstance(details, dict):
            details = json.dumps(details)
        with sqlite3.connect(self.db_path) as conn:
            conn.execute(
                "INSERT INTO system_events (event_type, details) VALUES (?, ?)",
                (event_type, details),
            )

    def get_history(self, limit=50):
        """Obtém o histórico de classificações.

        Args:
            limit: Número máximo de resultados.

        Returns:
            list: Lista de dicts com os dados das classificações.
        """
        with sqlite3.connect(self.db_path) as conn:
            conn.row_factory = sqlite3.Row
            rows = conn.execute(
                "SELECT * FROM classifications ORDER BY id DESC LIMIT ?",
                (limit,),
            ).fetchall()
            return [dict(row) for row in rows]

    def get_latest_fill_levels(self):
        """Obtém os últimos níveis de enchimento por compartimento.

        Returns:
            dict: {compartimento: percentagem}
        """
        with sqlite3.connect(self.db_path) as conn:
            conn.row_factory = sqlite3.Row
            rows = conn.execute("""
                SELECT compartment, percentage 
                FROM fill_levels 
                WHERE id IN (
                    SELECT MAX(id) FROM fill_levels GROUP BY compartment
                )
                """).fetchall()
            return {row["compartment"]: row["percentage"] for row in rows}

    def get_stats(self):
        """Obtém estatísticas gerais do sistema.

        Returns:
            dict: Estatísticas (total, por categoria, taxa média).
        """
        with sqlite3.connect(self.db_path) as conn:
            total = conn.execute("SELECT COUNT(*) FROM classifications").fetchone()[0]

            by_category = dict(
                conn.execute(
                    "SELECT category, COUNT(*) FROM classifications GROUP BY category"
                ).fetchall()
            )

            avg_confidence = conn.execute(
                "SELECT AVG(confidence) FROM classifications"
            ).fetchone()[0]

            return {
                "total_classifications": total,
                "by_category": by_category,
                "avg_confidence": round(avg_confidence or 0, 3),
            }
