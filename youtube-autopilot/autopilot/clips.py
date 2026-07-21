"""Stream/VOD clipping: find highlight moments in long recordings and cut
them into upload-ready clips.

Two detection modes:
- Transcript mode (best): yt-dlp fetches the VOD's captions, Claude reads the
  timestamped transcript and picks the strongest self-contained moments, and
  writes each clip's title/description/tags in the same pass.
- Audio-energy fallback: when no captions exist (Twitch VODs, local files),
  loudness analysis finds the hype/laughter spikes and Claude writes metadata
  from the context you supply.

Only clip streams you own or have permission to clip.
"""

import json
import re
import shutil
import subprocess
from pathlib import Path

import anthropic

from .assemble import probe_duration

HIGHLIGHTS_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "required": ["clips"],
    "properties": {
        "clips": {
            "type": "array",
            "items": {
                "type": "object",
                "additionalProperties": False,
                "required": ["start", "end", "title", "description", "tags", "reason"],
                "properties": {
                    "start": {"type": "string"},
                    "end": {"type": "string"},
                    "title": {"type": "string"},
                    "description": {"type": "string"},
                    "tags": {"type": "array", "items": {"type": "string"}},
                    "reason": {"type": "string"},
                },
            },
        },
    },
}

CLIP_META_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "required": ["title", "description", "tags"],
    "properties": {
        "title": {"type": "string"},
        "description": {"type": "string"},
        "tags": {"type": "array", "items": {"type": "string"}},
    },
}

SUB_STYLE = (
    "FontSize={size},PrimaryColour=&HFFFFFF&,OutlineColour=&H80000000&,"
    "BorderStyle=1,Outline=2,Shadow=0,MarginV=60"
)


def _ff(args: list[str], cwd: Path) -> subprocess.CompletedProcess:
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"command failed:\n{' '.join(args)}\n{result.stderr[-3000:]}")
    return result


def _ts_to_seconds(ts: str) -> float:
    parts = [float(p) for p in ts.strip().split(":")]
    seconds = 0.0
    for p in parts:
        seconds = seconds * 60 + p
    return seconds


def _seconds_to_ts(seconds: float) -> str:
    s = int(seconds)
    return f"{s // 3600:02d}:{(s % 3600) // 60:02d}:{s % 60:02d}"


# ---------------------------------------------------------------- transcript

def fetch_auto_subs(url: str, workdir: Path) -> Path | None:
    """Fetch the VOD's (auto-)captions without downloading the video."""
    subprocess.run(
        ["yt-dlp", "--skip-download", "--write-subs", "--write-auto-subs",
         "--sub-langs", "en.*,en", "--sub-format", "vtt",
         "-o", "vod", url],
        cwd=workdir, capture_output=True, text=True,
    )
    hits = sorted(workdir.glob("vod*.vtt"))
    return hits[0] if hits else None


def parse_vtt(path: Path) -> list[tuple[float, float, str]]:
    """Parse a VTT file into (start, end, text) cues, de-duplicating the
    rolling repeats that auto-captions produce."""
    cues: list[tuple[float, float, str]] = []
    seen_lines: list[str] = []
    text_lines: list[str] = []
    start = end = None

    def flush():
        nonlocal start, end, text_lines
        if start is not None and text_lines:
            # Auto-captions roll: each cue re-shows text from the previous one
            # (sometimes partially). Drop any line already contained in it.
            prev_joined = " ".join(seen_lines)
            fresh = [l for l in text_lines if l not in prev_joined]
            if fresh:
                cues.append((start, end, " ".join(fresh)))
            seen_lines[:] = text_lines
        start, end, text_lines = None, None, []

    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if "-->" in line:
            flush()
            times = line.split("-->")
            start = _ts_to_seconds(times[0])
            end = _ts_to_seconds(times[1].strip().split(" ")[0])
        elif line and start is not None and not line.startswith(("WEBVTT", "Kind:", "Language:", "NOTE")):
            clean = re.sub(r"<[^>]+>", "", line).strip()
            if clean:
                text_lines.append(clean)
    flush()
    return cues


def compact_transcript(cues: list[tuple[float, float, str]], bin_seconds: int = 15,
                       max_chars: int = 400_000) -> str:
    """Merge cues into timestamped lines Claude can scan efficiently."""
    bins: dict[int, list[str]] = {}
    for start, _end, text in cues:
        bins.setdefault(int(start // bin_seconds), []).append(text)
    lines = [
        f"[{_seconds_to_ts(b * bin_seconds)}] {' '.join(texts)}"
        for b, texts in sorted(bins.items())
    ]
    out = "\n".join(lines)
    if len(out) > max_chars:
        out = out[:max_chars] + "\n[transcript truncated]"
    return out


def select_highlights(cfg: dict, transcript: str, count: int,
                      min_seconds: int, max_seconds: int,
                      context: str | None) -> list[dict]:
    """Have Claude pick the strongest clip-worthy moments and write metadata."""
    ch = cfg["channel"]
    context_block = f"\nContext about this stream: {context}" if context else ""
    prompt = f"""You are a clip editor for a YouTube channel.

Channel niche: {ch['niche']}
Audience: {ch['audience']}{context_block}

Below is a timestamped transcript of a long stream/VOD. Pick the {count}
strongest clip-worthy moments and write upload metadata for each.

What makes a great clip:
- Completely self-contained: a viewer with zero context understands and
  enjoys it. It has its own beginning, peak, and landing.
- A real peak: a genuinely funny exchange, an impressive play or moment of
  skill, a surprising confession or hot take, a satisfying payoff.
- Clean edges: start one or two seconds before the moment begins (never
  mid-sentence) and end right after the payoff lands - don't linger.

Rules:
- Each clip must be between {min_seconds} and {max_seconds} seconds long.
- start and end are "HH:MM:SS" timestamps from the transcript.
- Clips must not overlap.
- title: under 90 characters, specific to what actually happens, written like
  a human clip channel would ("He didn't know the mic was on"), no all-caps
  words, no misleading bait.
- description: 1-3 sentences plus 3-5 hashtags on the last line.
- tags: 6-12 short search phrases.
- reason: one sentence on why this moment will retain viewers.
- Rank the clips strongest first.

TRANSCRIPT:
{transcript}"""

    client = anthropic.Anthropic()
    response = client.messages.create(
        model=cfg["api"]["model"],
        max_tokens=16000,
        thinking={"type": "adaptive"},
        output_config={"format": {"type": "json_schema", "schema": HIGHLIGHTS_SCHEMA}},
        messages=[{"role": "user", "content": prompt}],
    )
    text = next(b.text for b in response.content if b.type == "text")
    clips = json.loads(text)["clips"]

    result = []
    for c in clips[:count]:
        start = _ts_to_seconds(c["start"])
        end = _ts_to_seconds(c["end"])
        if end <= start:
            continue
        if end - start > max_seconds:
            end = start + max_seconds
        if end - start < min_seconds:
            end = start + min_seconds
        result.append({**c, "start_s": start, "end_s": end})
    return result


def clip_metadata(cfg: dict, context: str, aspect: str) -> dict:
    """Metadata for an energy-detected clip (no transcript available)."""
    ch = cfg["channel"]
    prompt = f"""You write upload metadata for a YouTube clip channel.

Channel niche: {ch['niche']}
This clip is a highlight moment cut from: {context}

There is no transcript, so write honest, non-specific but enticing metadata:
- title: under 90 characters, no misleading claims about what happens.{" End with #Shorts." if aspect == "short" else ""}
- description: 1-2 sentences plus 3-5 hashtags on the last line.
- tags: 6-12 short search phrases."""
    client = anthropic.Anthropic()
    response = client.messages.create(
        model=cfg["api"]["model"],
        max_tokens=2048,
        thinking={"type": "adaptive"},
        output_config={"format": {"type": "json_schema", "schema": CLIP_META_SCHEMA}},
        messages=[{"role": "user", "content": prompt}],
    )
    text = next(b.text for b in response.content if b.type == "text")
    return json.loads(text)


# ------------------------------------------------------------- audio energy

def audio_energy_windows(media: Path, count: int, window_seconds: int,
                         workdir: Path) -> list[tuple[float, float]]:
    """Find the loudest non-overlapping windows (hype/laughter spikes)."""
    result = subprocess.run(
        ["ffprobe", "-v", "error", "-f", "lavfi",
         "-i", f"amovie={media.name},aresample=8000,asetnsamples=n=8000,"
               "astats=metadata=1:reset=1",
         "-show_entries", "frame_tags=lavfi.astats.Overall.RMS_level",
         "-of", "csv=p=0"],
        cwd=workdir, capture_output=True, text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"audio analysis failed: {result.stderr[-1000:]}")
    levels = []
    for line in result.stdout.splitlines():
        line = line.strip().rstrip(",")
        if not line:
            continue
        try:
            levels.append(float(line))
        except ValueError:
            levels.append(-99.0)  # -inf / silence

    if len(levels) < window_seconds:
        return [(0.0, float(len(levels) or window_seconds))]

    # rolling mean loudness per window start
    scores = []
    window_sum = sum(levels[:window_seconds])
    scores.append(window_sum)
    for i in range(1, len(levels) - window_seconds + 1):
        window_sum += levels[i + window_seconds - 1] - levels[i - 1]
        scores.append(window_sum)

    picked: list[tuple[float, float]] = []
    for i in sorted(range(len(scores)), key=lambda i: scores[i], reverse=True):
        start, end = float(i), float(i + window_seconds)
        if all(end <= s or start >= e for s, e in picked):
            picked.append((start, end))
            if len(picked) == count:
                break
    picked.sort()
    return picked


# ------------------------------------------------------------ media fetching

def require_ytdlp() -> None:
    if shutil.which("yt-dlp") is None:
        raise SystemExit("yt-dlp not found - install it with: pip install yt-dlp")


def download_audio(url: str, workdir: Path) -> Path:
    _ff(["yt-dlp", "-f", "bestaudio/best", "-o", "audio.%(ext)s", url], cwd=workdir)
    hits = sorted(p for p in workdir.glob("audio.*") if p.suffix != ".part")
    if not hits:
        raise RuntimeError("audio download produced no file")
    return hits[0]


def download_section(url: str, start: float, end: float, index: int,
                     workdir: Path) -> Path:
    """Download just one time-range of the VOD (fast even on long streams)."""
    section = f"*{_seconds_to_ts(start)}-{_seconds_to_ts(end)}"
    _ff(
        ["yt-dlp", "-f", "bv*[height<=1080]+ba/b[height<=1080]/b",
         "--download-sections", section, "--force-keyframes-at-cuts",
         "--merge-output-format", "mp4",
         "-o", f"raw_{index:02d}.%(ext)s", url],
        cwd=workdir,
    )
    hits = sorted(p for p in workdir.glob(f"raw_{index:02d}.*") if p.suffix != ".part")
    if not hits:
        raise RuntimeError("section download produced no file")
    return hits[0]


def cut_local_section(src: Path, start: float, end: float, dest: Path) -> Path:
    _ff(
        ["ffmpeg", "-y", "-ss", f"{start:.2f}", "-i", str(src),
         "-t", f"{end - start:.2f}",
         "-c:v", "libx264", "-preset", "veryfast", "-crf", "20",
         "-c:a", "aac", "-b:a", "192k", dest.name],
        cwd=dest.parent,
    )
    return dest


# ------------------------------------------------------------- clip rendering

def slice_srt(cues: list[tuple[float, float, str]], start: float, end: float,
              dest: Path) -> Path | None:
    """Extract the cues inside [start, end] and re-zero their timestamps."""
    def fmt(t: float) -> str:
        ms = int(round(max(0.0, t) * 1000))
        h, rem = divmod(ms, 3_600_000)
        m, rem = divmod(rem, 60_000)
        s, ms = divmod(rem, 1000)
        return f"{h:02d}:{m:02d}:{s:02d},{ms:03d}"

    selected = [(cs, ce, text) for cs, ce, text in cues if ce > start and cs < end]
    if not selected:
        return None
    with open(dest, "w", encoding="utf-8") as f:
        for i, (cs, ce, text) in enumerate(selected, 1):
            f.write(f"{i}\n{fmt(cs - start)} --> {fmt(min(ce, end) - start)}\n{text}\n\n")
    return dest


def render_clip(raw: Path, dest: Path, aspect: str, srt: Path | None) -> Path:
    """Render the final clip: vertical blur-pad layout for Shorts, straight
    re-encode for landscape, with optional burned captions."""
    workdir = dest.parent
    if aspect == "short":
        graph = (
            "[0:v]split=2[bg0][fg0];"
            "[bg0]scale=1080:1920:force_original_aspect_ratio=increase,"
            "crop=1080:1920,boxblur=24:4[bg];"
            "[fg0]scale=1080:-2[fg];"
            "[bg][fg]overlay=(W-w)/2:(H-h)/2[v]"
        )
        if srt is not None:
            graph += f";[v]subtitles={srt.name}:force_style='{SUB_STYLE.format(size=18)}'[v]"
        args = ["ffmpeg", "-y", "-i", raw.name, "-filter_complex", graph,
                "-map", "[v]", "-map", "0:a?"]
    else:
        args = ["ffmpeg", "-y", "-i", raw.name]
        if srt is not None:
            args += ["-vf", f"subtitles={srt.name}:force_style='{SUB_STYLE.format(size=16)}'"]
        args += ["-map", "0:v", "-map", "0:a?"]
    args += ["-c:v", "libx264", "-preset", "veryfast", "-crf", "20",
             "-c:a", "aac", "-b:a", "192k", dest.name]
    _ff(args, cwd=workdir)
    return dest
