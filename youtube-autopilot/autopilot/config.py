"""Configuration and channel-profile loading for youtube-autopilot."""

import os
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
CHANNELS_DIR = ROOT / "channels"

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
        "voice_provider": "edge",       # edge (free) or elevenlabs (premium)
        "voice": "en-US-GuyNeural",     # edge-tts voice
        "voice_rate": "+0%",
        "elevenlabs_voice_id": "21m00Tcm4TlvDq8ikWAM",
        "elevenlabs_model": "eleven_multilingual_v2",
        "captions": True,
        "caption_style": "auto",        # auto: pop for Shorts, classic for landscape
        "hook_overlay": True,           # first-frame text-hook card (pop style only)
        "motion": True,                 # Ken Burns push on every scene
        "grade": True,                  # light contrast/saturation + vignette
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
        "research": True,
        "quality_gate": True,
        "min_quality": 8,
        "max_revisions": 2,
    },
    "clips": {
        "count": 3,
        "min_seconds": 15,
        "max_seconds": 55,
    },
}


def resolve_channel(name: str | None) -> dict:
    """Map a channel name to its file locations.

    Without a name, the root-level config.yaml/topics.yaml act as a single
    default channel. With a name, everything lives under channels/<name>/,
    so each channel keeps its own config, topic queue, credentials, and
    published log.
    """
    if name:
        base = CHANNELS_DIR / name
        if not base.is_dir():
            available = sorted(
                p.name for p in CHANNELS_DIR.iterdir() if p.is_dir()
            ) if CHANNELS_DIR.is_dir() else []
            raise SystemExit(
                f"Unknown channel {name!r}. Available: {', '.join(available) or '(none)'}. "
                f"Create channels/{name}/config.yaml to add it."
            )
        state_dir = base / "state"
    else:
        base = ROOT
        state_dir = ROOT / "state"
    return {
        "name": name or "default",
        "base": base,
        "config": base / "config.yaml",
        "topics": base / "topics.yaml",
        "state_dir": state_dir,
        "client_secret": base / "client_secret.json",
        "token": state_dir / "token.json",
        "output_dir": ROOT / "output" / (name or "default"),
    }


def load_config(path: Path) -> dict:
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


def load_topics(path: Path) -> list[str]:
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
