"""Configuration loading for youtube-autopilot."""

import os
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
OUTPUT_DIR = ROOT / "output"
STATE_DIR = ROOT / "state"

DEFAULTS = {
    "channel": {
        "niche": "general interest explainers",
        "audience": "a broad adult audience",
        "tone": "clear and conversational",
        "language": "English",
    },
    "video": {
        "aspect": "landscape",
        "target_minutes": 4,
        "voice": "en-US-GuyNeural",
        "voice_rate": "+0%",
        "captions": True,
        "music": None,
    },
    "upload": {
        "privacy": "public",
        "category_id": "28",
        "notify_subscribers": True,
        "publish_at": None,
        "ai_disclosure": True,
    },
    "api": {
        "model": "claude-opus-4-8",
        "auto_topics": True,
    },
}


def load_config(path: Path | None = None) -> dict:
    path = path or ROOT / "config.yaml"
    cfg = {section: dict(values) for section, values in DEFAULTS.items()}
    if path.exists():
        with open(path, encoding="utf-8") as f:
            user = yaml.safe_load(f) or {}
        for section, values in user.items():
            if isinstance(values, dict):
                cfg.setdefault(section, {}).update(values)
            else:
                cfg[section] = values
    return cfg


def load_topics(path: Path | None = None) -> list[str]:
    path = path or ROOT / "topics.yaml"
    if not path.exists():
        return []
    with open(path, encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    return [str(t) for t in data.get("topics", [])]


def video_dimensions(cfg: dict) -> tuple[int, int]:
    if cfg["video"]["aspect"] == "short":
        return 1080, 1920
    return 1920, 1080


def require_env(name: str) -> str:
    value = os.environ.get(name)
    if not value:
        raise SystemExit(
            f"{name} is not set. Copy .env.example to .env and fill it in."
        )
    return value
