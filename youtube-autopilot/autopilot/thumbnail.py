"""Thumbnail and fallback-slide rendering with Pillow."""

import textwrap
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFont

FONT_CANDIDATES = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/System/Library/Fonts/Helvetica.ttc",
    "C:/Windows/Fonts/arialbd.ttf",
]

PALETTES = [
    ((16, 24, 48), (64, 24, 96)),
    ((8, 40, 40), (16, 80, 120)),
    ((40, 16, 16), (120, 48, 24)),
    ((12, 12, 20), (48, 64, 40)),
]


def _load_font(size: int):
    for path in FONT_CANDIDATES:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


def _gradient(width: int, height: int, top: tuple, bottom: tuple) -> Image.Image:
    img = Image.new("RGB", (width, height))
    draw = ImageDraw.Draw(img)
    for y in range(height):
        t = y / max(1, height - 1)
        color = tuple(int(a + (b - a) * t) for a, b in zip(top, bottom))
        draw.line([(0, y), (width, y)], fill=color)
    return img


def make_thumbnail(text: str, dest: Path, width: int = 1280, height: int = 720,
                   frame: Path | None = None) -> Path:
    """Bold-text thumbnail. When a video frame is supplied it becomes the
    darkened background; otherwise a gradient is used."""
    img = None
    if frame is not None and frame.exists():
        try:
            img = Image.open(frame).convert("RGB")
            # cover-fit to thumbnail size
            scale = max(width / img.width, height / img.height)
            img = img.resize((int(img.width * scale) + 1, int(img.height * scale) + 1))
            left = (img.width - width) // 2
            top = (img.height - height) // 2
            img = img.crop((left, top, left + width, top + height))
            img = ImageEnhance.Brightness(img).enhance(0.55)
            img = ImageEnhance.Contrast(img).enhance(1.15)
        except OSError:
            img = None
    if img is None:
        img = _gradient(width, height, *PALETTES[0])
    draw = ImageDraw.Draw(img)
    # accent bar
    draw.rectangle([(0, height - 24), (width, height)], fill=(255, 196, 0))

    font = _load_font(int(height * 0.18))
    lines = textwrap.wrap(text.upper(), width=12) or [text.upper()]
    line_heights = []
    for line in lines:
        box = draw.textbbox((0, 0), line, font=font)
        line_heights.append(box[3] - box[1])
    total = sum(line_heights) + int(height * 0.04) * (len(lines) - 1)
    y = (height - total) // 2
    for line, lh in zip(lines, line_heights):
        box = draw.textbbox((0, 0), line, font=font)
        x = (width - (box[2] - box[0])) // 2
        draw.text((x + 5, y + 5), line, font=font, fill=(0, 0, 0))
        draw.text((x, y), line, font=font, fill=(255, 255, 255))
        y += lh + int(height * 0.04)

    img.save(dest, "PNG")
    return dest


def make_slide(label: str, dest: Path, width: int, height: int, seed: int = 0) -> Path:
    """Fallback visual when no stock footage is available for a scene."""
    top, bottom = PALETTES[seed % len(PALETTES)]
    img = _gradient(width, height, top, bottom)
    draw = ImageDraw.Draw(img)
    font = _load_font(int(min(width, height) * 0.05))
    box = draw.textbbox((0, 0), label, font=font)
    x = (width - (box[2] - box[0])) // 2
    y = (height - (box[3] - box[1])) // 2
    draw.text((x, y), label, font=font, fill=(220, 220, 220))
    img.save(dest, "PNG")
    return dest
