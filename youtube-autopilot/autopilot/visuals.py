"""Per-scene visuals: Pexels stock footage with a generated-slide fallback."""

import os
from pathlib import Path

import requests

from .thumbnail import make_slide

PEXELS_SEARCH = "https://api.pexels.com/videos/search"


def fetch_scene_visuals(scenes: list[dict], workdir: Path, width: int, height: int) -> list[Path]:
    """Return one visual file per scene (an .mp4 clip or a .png slide)."""
    api_key = os.environ.get("PEXELS_API_KEY")
    orientation = "portrait" if height > width else "landscape"
    used_ids: set[int] = set()
    paths: list[Path] = []

    for i, scene in enumerate(scenes):
        clip = None
        if api_key:
            clip = _download_pexels_clip(
                api_key, scene["footage_keywords"], orientation,
                workdir / f"scene_{i:02d}.mp4", used_ids, width,
            )
        if clip is None:
            clip = workdir / f"scene_{i:02d}.png"
            make_slide(scene["footage_keywords"], clip, width, height, seed=i)
        paths.append(clip)
    return paths


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
        if video["id"] in used_ids:
            continue
        # Smallest rendition that still meets the target width keeps downloads fast.
        files = sorted(
            (f for f in video.get("video_files", []) if f.get("width")),
            key=lambda f: f["width"],
        )
        chosen = next((f for f in files if f["width"] >= min_width), files[-1] if files else None)
        if not chosen:
            continue
        try:
            with requests.get(chosen["link"], stream=True, timeout=120) as r:
                r.raise_for_status()
                with open(dest, "wb") as out:
                    for chunk in r.iter_content(chunk_size=1 << 20):
                        out.write(chunk)
        except requests.RequestException as e:
            print(f"  pexels download failed for {query!r}: {e}")
            continue
        used_ids.add(video["id"])
        return dest

    print(f"  no pexels results for {query!r}, using generated slide")
    return None
