"""Narration synthesis with edge-tts (free neural voices) + SRT subtitles."""

import asyncio
from pathlib import Path

import edge_tts

# 100-nanosecond units per second (edge-tts word boundary timing unit)
TICKS = 10_000_000

MAX_WORDS_PER_CUE = 7
MAX_GAP_SECONDS = 0.6


def synthesize(text: str, voice: str, mp3_path: Path, srt_path: Path, rate: str = "+0%") -> None:
    asyncio.run(_synthesize(text, voice, mp3_path, srt_path, rate))


async def _synthesize(text: str, voice: str, mp3_path: Path, srt_path: Path, rate: str) -> None:
    communicate = edge_tts.Communicate(text, voice, rate=rate)
    words: list[dict] = []
    with open(mp3_path, "wb") as f:
        async for chunk in communicate.stream():
            if chunk["type"] == "audio":
                f.write(chunk["data"])
            elif chunk["type"] == "WordBoundary":
                words.append(chunk)
    _write_srt(words, srt_path)


def _fmt(seconds: float) -> str:
    ms = int(round(seconds * 1000))
    h, rem = divmod(ms, 3_600_000)
    m, rem = divmod(rem, 60_000)
    s, ms = divmod(rem, 1000)
    return f"{h:02d}:{m:02d}:{s:02d},{ms:03d}"


def _write_srt(words: list[dict], srt_path: Path) -> None:
    """Group word-boundary events into short readable cues."""
    cues: list[tuple[float, float, str]] = []
    current: list[str] = []
    start = end = 0.0

    for w in words:
        w_start = w["offset"] / TICKS
        w_end = (w["offset"] + w["duration"]) / TICKS
        if current and (len(current) >= MAX_WORDS_PER_CUE or w_start - end > MAX_GAP_SECONDS):
            cues.append((start, end, " ".join(current)))
            current = []
        if not current:
            start = w_start
        current.append(w["text"])
        end = w_end
    if current:
        cues.append((start, end, " ".join(current)))

    with open(srt_path, "w", encoding="utf-8") as f:
        for i, (cue_start, cue_end, cue_text) in enumerate(cues, 1):
            f.write(f"{i}\n{_fmt(cue_start)} --> {_fmt(cue_end)}\n{cue_text}\n\n")
