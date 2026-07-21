"""Performance feedback loop: pull each published video's real audience
metrics from the YouTube Analytics API and feed them back into topic research
and script generation, so the system learns from what actually held viewers."""

from datetime import date

from googleapiclient.discovery import build

from . import state
from .uploader import get_credentials

METRICS = ("views,estimatedMinutesWatched,averageViewDuration,"
           "averageViewPercentage,likes,subscribersGained")


def refresh_stats(channel: dict) -> int:
    """Fetch fresh metrics for every published video and store them in the
    channel's published.json. Returns the number of videos updated."""
    entries = state.load_published(channel["state_dir"])
    ids = [e["video_id"] for e in entries if e.get("video_id")]
    if not ids:
        return 0

    creds = get_credentials(channel["token"])
    yta = build("youtubeAnalytics", "v2", credentials=creds)
    start = min(e["published_at"][:10] for e in entries if e.get("video_id"))
    resp = yta.reports().query(
        ids="channel==MINE",
        startDate=start,
        endDate=date.today().isoformat(),
        metrics=METRICS,
        dimensions="video",
        filters=f"video=={','.join(ids[:500])}",
        maxResults=500,
    ).execute()

    columns = [h["name"] for h in resp.get("columnHeaders", [])]
    by_video: dict[str, dict] = {}
    for row in resp.get("rows", []):
        record = dict(zip(columns, row))
        by_video[record.pop("video")] = {
            "views": int(record.get("views", 0)),
            "watch_minutes": round(float(record.get("estimatedMinutesWatched", 0)), 1),
            "avg_view_seconds": round(float(record.get("averageViewDuration", 0)), 1),
            "avg_view_pct": round(float(record.get("averageViewPercentage", 0)), 1),
            "likes": int(record.get("likes", 0)),
            "subs_gained": int(record.get("subscribersGained", 0)),
        }

    updated = 0
    for e in entries:
        stats = by_video.get(e.get("video_id"))
        if stats is not None:
            e["stats"] = stats
            updated += 1
    if updated:
        state.save_published(channel["state_dir"], entries)
    return updated


def performance_summary(entries: list[dict]) -> str | None:
    """A compact what-worked/what-flopped digest for the model, or None if
    there isn't enough data yet to be meaningful."""
    scored = [e for e in entries if e.get("stats") and e["stats"].get("views", 0) > 0]
    if len(scored) < 3:
        return None

    def line(e: dict) -> str:
        s = e["stats"]
        return (f'- "{e["title"]}" - {s["views"]} views, '
                f'{s["avg_view_pct"]}% avg watched, {s["subs_gained"]} subs gained')

    top = sorted(scored, key=lambda e: e["stats"]["views"], reverse=True)[:5]
    parts = ["Top performers by views:"] + [line(e) for e in top]
    if len(scored) >= 5:
        worst = sorted(scored, key=lambda e: e["stats"]["avg_view_pct"])[:3]
        parts += ["Weakest retention (viewers left earliest):"] + [line(e) for e in worst]
    return "\n".join(parts)
