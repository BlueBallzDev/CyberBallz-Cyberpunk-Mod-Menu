"""Per-scene visuals: stock footage from Pexels and/or Pixabay (free APIs),
with a generated-slide fallback. Providers are tried in order for each scene,
so either key alone is enough and both together maximize match rate."""

import os
from pathlib import Path

import requests

from .thumbnail import make_slide

PEXELS_SEARCH = "https://api.pexels.com/videos/search"
PIXABAY_SEARCH = "https://pixabay.com/api/videos/"


def fetch_scene_visuals(scenes: list[dict], workdir: Path, width: int, height: int) -> list[Path]:
    """Return one visual file per scene (an .mp4 clip or a .png slide)."""
    pexels_key = os.environ.get("PEXELS_API_KEY")
    pixabay_key = os.environ.get("PIXABAY_API_KEY")
    portrait = height > width
    used_ids: set[tuple[str, int]] = set()
    paths: list[Path] = []

    for i, scene in enumerate(scenes):
        dest = workdir / f"scene_{i:02d}.mp4"
        clip = None
        if pexels_key:
            clip = _download_pexels_clip(
                pexels_key, scene["footage_keywords"],
                "portrait" if portrait else "landscape",
                dest, used_ids, width,
            )
        if clip is None and pixabay_key:
            clip = _download_pixabay_clip(
                pixabay_key, scene["footage_keywords"], dest, used_ids,
                width, portrait,
            )
        if clip is None:
            clip = workdir / f"scene_{i:02d}.png"
            make_slide(scene["footage_keywords"], clip, width, height, seed=i)
        paths.append(clip)
    return paths


def _download(url: str, dest: Path) -> bool:
    try:
        with requests.get(url, stream=True, timeout=120) as r:
            r.raise_for_status()
            with open(dest, "wb") as out:
                for chunk in r.iter_content(chunk_size=1 << 20):
                    out.write(chunk)
        return True
    except requests.RequestException as e:
        print(f"  footage download failed: {e}")
        return False


def _download_pexels_clip(
    api_key: str, query: str, orientation: str, dest: Path,
    used_ids: set[int], min_width: int,
) -> Path | None:
    try:
        resp = requests.get(
            PEXELS_SEARCH,
            headers={"Authorization": api_key},
            params={"query": query, "orientation": orientation, "per_page": 10},
            timeout=30,
        )
        resp.raise_for_status()
        videos = resp.json().get("videos", [])
    except requests.RequestException as e:
        print(f"  pexels search failed for {query!r}: {e}")
        return None

    for video in videos:
        if ("pexels", video["id"]) in used_ids:
            continue
        # Smallest rendition that still meets the target width keeps downloads fast.
        files = sorted(
            (f for f in video.get("video_files", []) if f.get("width")),
            key=lambda f: f["width"],
        )
        chosen = next((f for f in files if f["width"] >= min_width), files[-1] if files else None)
        if not chosen or not _download(chosen["link"], dest):
            continue
        used_ids.add(("pexels", video["id"]))
        return dest

    print(f"  no pexels results for {query!r}")
    return None


def _download_pixabay_clip(
    api_key: str, query: str, dest: Path, used_ids: set,
    min_width: int, portrait: bool,
) -> Path | None:
    try:
        resp = requests.get(
            PIXABAY_SEARCH,
            params={"key": api_key, "q": query[:100], "per_page": 10,
                    "safesearch": "true", "video_type": "film"},
            timeout=30,
        )
        resp.raise_for_status()
        hits = resp.json().get("hits", [])
    except requests.RequestException as e:
        print(f"  pixabay search failed for {query!r}: {e}")
        return None

    # Prefer clips whose orientation matches the target; landscape clips still
    # work for Shorts (the renderer center-crops), so match is a bonus not a must.
    def orientation_rank(hit: dict) -> int:
        large = (hit.get("videos") or {}).get("large") or {}
        is_portrait = large.get("height", 0) > large.get("width", 1)
        return 0 if is_portrait == portrait else 1

    for hit in sorted(hits, key=orientation_rank):
        if ("pixabay", hit["id"]) in used_ids:
            continue
        renditions = sorted(
            (r for r in (hit.get("videos") or {}).values()
             if r.get("url") and r.get("width")),
            key=lambda r: r["width"],
        )
        chosen = next((r for r in renditions if r["width"] >= min_width),
                      renditions[-1] if renditions else None)
        if not chosen or not _download(chosen["url"], dest):
            continue
        used_ids.add(("pixabay", hit["id"]))
        return dest

    print(f"  no pixabay results for {query!r}")
    return None
