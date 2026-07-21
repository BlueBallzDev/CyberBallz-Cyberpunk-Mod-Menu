"""Published-video log so the same topic is never produced twice."""

import json
from datetime import datetime, timezone

from .config import STATE_DIR

PUBLISHED_FILE = STATE_DIR / "published.json"


def load_published() -> list[dict]:
    if PUBLISHED_FILE.exists():
        with open(PUBLISHED_FILE, encoding="utf-8") as f:
            return json.load(f)
    return []


def record_published(topic: str, title: str, video_id: str | None) -> None:
    entries = load_published()
    entries.append(
        {
            "topic": topic,
            "title": title,
            "video_id": video_id,
            "published_at": datetime.now(timezone.utc).isoformat(),
        }
    )
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    with open(PUBLISHED_FILE, "w", encoding="utf-8") as f:
        json.dump(entries, f, indent=2)


def published_topics() -> set[str]:
    return {e["topic"] for e in load_published()}
