"""Per-channel published-video log so the same topic is never produced twice."""

import json
from datetime import datetime, timezone
from pathlib import Path


def _published_file(state_dir: Path) -> Path:
    return state_dir / "published.json"


def load_published(state_dir: Path) -> list[dict]:
    f = _published_file(state_dir)
    if f.exists():
        with open(f, encoding="utf-8") as fh:
            return json.load(fh)
    return []


def save_published(state_dir: Path, entries: list[dict]) -> None:
    state_dir.mkdir(parents=True, exist_ok=True)
    with open(_published_file(state_dir), "w", encoding="utf-8") as f:
        json.dump(entries, f, indent=2)


def record_published(state_dir: Path, topic: str, title: str, video_id: str | None) -> None:
    entries = load_published(state_dir)
    entries.append(
        {
            "topic": topic,
            "title": title,
            "video_id": video_id,
            "published_at": datetime.now(timezone.utc).isoformat(),
        }
    )
    save_published(state_dir, entries)


def published_topics(state_dir: Path) -> set[str]:
    return {e["topic"] for e in load_published(state_dir)}
