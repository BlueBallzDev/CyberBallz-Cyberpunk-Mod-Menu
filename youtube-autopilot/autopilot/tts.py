"""Narration synthesis with word-accurate SRT subtitles.

Providers (config: video.voice_provider):
- "edge" (default): edge-tts, free neural voices.
- "elevenlabs": premium voices via the ElevenLabs API — the single biggest
  "doesn't sound synthetic" upgrade. Needs ELEVENLABS_API_KEY in .env.
"""

import asyncio
import base64
import os
import re
from pathlib import Path

import edge_tts
import requests

# 100-nanosecond units per second (edge-tts word boundary timing unit)
TICKS = 10_000_000

MAX_WORDS_PER_CUE = 7
MAX_GAP_SECONDS = 0.6

ELEVENLABS_URL = "https://api.elevenlabs.io/v1/text-to-speech/{voice_id}/with-timestamps"
ELEVENLABS_CHUNK_CHARS = 2500


def synthesize(text: str, video_cfg: dict, mp3_path: Path, srt_path: Path,
               words_path: Path | None = None) -> None:
    provider = video_cfg.get("voice_provider", "edge")
    if provider == "elevenlabs":
        words = _synthesize_elevenlabs(text, video_cfg, mp3_path, srt_path)
    elif provider == "edge":
        words = asyncio.run(_synthesize_edge(text, video_cfg, mp3_path, srt_path))
    else:
        raise SystemExit(f"Unknown voice_provider {provider!r} (use edge or elevenlabs)")
    if words_path is not None:
        import json
        words_path.write_text(json.dumps(words), encoding="utf-8")


# ------------------------------------------------------------------ edge-tts

async def _synthesize_edge(text: str, video_cfg: dict, mp3_path: Path,
                           srt_path: Path) -> list[tuple[float, float, str]]:
    communicate = edge_tts.Communicate(
        text, video_cfg.get("voice", "en-US-GuyNeural"),
        rate=video_cfg.get("voice_rate", "+0%"),
    )
    words: list[tuple[float, float, str]] = []
    with open(mp3_path, "wb") as f:
        async for chunk in communicate.stream():
            if chunk["type"] == "audio":
                f.write(chunk["data"])
            elif chunk["type"] == "WordBoundary":
                words.append((
                    chunk["offset"] / TICKS,
                    (chunk["offset"] + chunk["duration"]) / TICKS,
                    chunk["text"],
                ))
    write_srt(words, srt_path)
    return words


# ---------------------------------------------------------------- elevenlabs

def _synthesize_elevenlabs(text: str, video_cfg: dict, mp3_path: Path,
                           srt_path: Path) -> list[tuple[float, float, str]]:
    api_key = os.environ.get("ELEVENLABS_API_KEY")
    if not api_key:
        raise SystemExit(
            "voice_provider is 'elevenlabs' but ELEVENLABS_API_KEY is not set in .env"
        )
    voice_id = video_cfg.get("elevenlabs_voice_id", "21m00Tcm4TlvDq8ikWAM")
    model_id = video_cfg.get("elevenlabs_model", "eleven_multilingual_v2")

    audio = b""
    words: list[tuple[float, float, str]] = []
    offset = 0.0
    for chunk in chunk_text(text, ELEVENLABS_CHUNK_CHARS):
        resp = requests.post(
            ELEVENLABS_URL.format(voice_id=voice_id),
            headers={"xi-api-key": api_key},
            params={"output_format": "mp3_44100_128"},
            json={"text": chunk, "model_id": model_id},
            timeout=600,
        )
        if resp.status_code != 200:
            raise RuntimeError(f"ElevenLabs API error {resp.status_code}: {resp.text[:500]}")
        data = resp.json()
        audio += base64.b64decode(data["audio_base64"])
        alignment = data.get("alignment") or {}
        chunk_words = alignment_to_words(alignment, offset)
        words.extend(chunk_words)
        ends = alignment.get("character_end_times_seconds") or []
        offset += ends[-1] if ends else 0.0

    mp3_path.write_bytes(audio)
    write_srt(words, srt_path)
    return words


def chunk_text(text: str, limit: int) -> list[str]:
    """Split text into sentence-aligned chunks no longer than limit chars."""
    sentences = re.split(r"(?<=[.!?])\s+", text.strip())
    chunks: list[str] = []
    current = ""
    for sentence in sentences:
        if current and len(current) + 1 + len(sentence) > limit:
            chunks.append(current)
            current = sentence
        else:
            current = f"{current} {sentence}".strip()
        # a single sentence longer than the limit is sent as-is
    if current:
        chunks.append(current)
    return chunks


def alignment_to_words(alignment: dict, offset: float) -> list[tuple[float, float, str]]:
    """Convert ElevenLabs character-level alignment to word timings."""
    chars = alignment.get("characters") or []
    starts = alignment.get("character_start_times_seconds") or []
    ends = alignment.get("character_end_times_seconds") or []
    words: list[tuple[float, float, str]] = []
    word = ""
    w_start = w_end = 0.0
    for ch, cs, ce in zip(chars, starts, ends):
        if ch.isspace():
            if word:
                words.append((offset + w_start, offset + w_end, word))
                word = ""
        else:
            if not word:
                w_start = cs
            word += ch
            w_end = ce
    if word:
        words.append((offset + w_start, offset + w_end, word))
    return words


# ---------------------------------------------------------------- subtitles

def _fmt(seconds: float) -> str:
    ms = int(round(seconds * 1000))
    h, rem = divmod(ms, 3_600_000)
    m, rem = divmod(rem, 60_000)
    s, ms = divmod(rem, 1000)
    return f"{h:02d}:{m:02d}:{s:02d},{ms:03d}"


def write_srt(words: list[tuple[float, float, str]], srt_path: Path) -> None:
    """Group word timings into short readable cues."""
    cues: list[tuple[float, float, str]] = []
    current: list[str] = []
    start = end = 0.0

    for w_start, w_end, w_text in words:
        if current and (len(current) >= MAX_WORDS_PER_CUE or w_start - end > MAX_GAP_SECONDS):
            cues.append((start, end, " ".join(current)))
            current = []
        if not current:
            start = w_start
        current.append(w_text)
        end = w_end
    if current:
        cues.append((start, end, " ".join(current)))

    with open(srt_path, "w", encoding="utf-8") as f:
        for i, (cue_start, cue_end, cue_text) in enumerate(cues, 1):
            f.write(f"{i}\n{_fmt(cue_start)} --> {_fmt(cue_end)}\n{cue_text}\n\n")
