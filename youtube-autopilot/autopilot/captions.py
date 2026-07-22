"""Caption styles.

"classic" — SRT burned along the bottom (documentary look, landscape default).
"pop" — big centered word-highlight captions in ASS/karaoke format (the
modern Shorts look): words appear in small groups, and the spoken word lights
up yellow in sync with the voice.
"""

import json
from pathlib import Path

GROUP_MAX_WORDS = 3
GROUP_MAX_GAP = 0.5

ASS_TEMPLATE = """[Script Info]
ScriptType: v4.00+
PlayResX: {width}
PlayResY: {height}
WrapStyle: 0
ScaledBorderAndShadow: yes

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Pop,DejaVu Sans,{size},&H0000FFFF,&H00FFFFFF,&H00000000,&H96000000,-1,0,0,0,100,100,1,0,1,{outline},0,2,60,60,{margin_v},1
Style: Hook,DejaVu Sans,{hook_size},&H00FFFFFF,&H00FFFFFF,&H00000000,&HA0000000,-1,0,0,0,100,100,1,0,3,{hook_pad},0,8,70,70,{hook_margin_v},1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
"""


def load_words(path: Path) -> list[tuple[float, float, str]]:
    return [tuple(w) for w in json.loads(path.read_text(encoding="utf-8"))]


def _ts(seconds: float) -> str:
    cs = int(round(max(0.0, seconds) * 100))
    h, rem = divmod(cs, 360_000)
    m, rem = divmod(rem, 6_000)
    s, cs = divmod(rem, 100)
    return f"{h}:{m:02d}:{s:02d}.{cs:02d}"


def _groups(words: list[tuple[float, float, str]]):
    group: list[tuple[float, float, str]] = []
    for w in words:
        if group and (len(group) >= GROUP_MAX_WORDS or w[0] - group[-1][1] > GROUP_MAX_GAP):
            yield group
            group = []
        group.append(w)
    if group:
        yield group


def write_ass_pop(words: list[tuple[float, float, str]], dest: Path,
                  width: int, height: int, hook_text: str | None = None,
                  hook_seconds: float = 3.0) -> Path:
    """Word-highlight karaoke captions, centered in the lower-middle of frame,
    plus an optional first-frame text-hook card (boxed, upper third) — the
    "reason to care" every high-performing Short shows before anything happens."""
    size = max(28, int(height * 0.052))
    hook_size = max(26, int(height * 0.042))
    lines = [ASS_TEMPLATE.format(
        width=width, height=height, size=size,
        outline=max(2, size // 14), margin_v=int(height * 0.33),
        hook_size=hook_size, hook_pad=max(8, hook_size // 5),
        hook_margin_v=int(height * 0.24),
    )]
    if hook_text:
        lines.append(
            f"Dialogue: 1,{_ts(0.0)},{_ts(hook_seconds)},Hook,,0,0,0,,{hook_text}\n"
        )
    for group in _groups(words):
        start, end = group[0][0], group[-1][1] + 0.05
        parts = []
        for i, (w_start, w_end, text) in enumerate(group):
            # \k spans run back-to-back from the event start, so each word's
            # span covers it plus the silence before the next word.
            span_end = group[i + 1][0] if i + 1 < len(group) else w_end
            k_cs = max(1, int(round((span_end - w_start) * 100)))
            parts.append(f"{{\\k{k_cs}}}{text.upper()}")
        lines.append(
            f"Dialogue: 0,{_ts(start)},{_ts(end)},Pop,,0,0,0,,{' '.join(parts)}\n"
        )
    dest.write_text("".join(lines), encoding="utf-8")
    return dest
