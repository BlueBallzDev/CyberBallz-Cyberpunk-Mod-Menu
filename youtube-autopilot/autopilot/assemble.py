"""FFmpeg video assembly: normalize clips, concat, add narration + subtitles."""

import json
import subprocess
from pathlib import Path


def _run(args: list[str], cwd: Path) -> None:
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"ffmpeg failed:\n{' '.join(args)}\n{result.stderr[-3000:]}")


def probe_duration(path: Path) -> float:
    result = subprocess.run(
        ["ffprobe", "-v", "quiet", "-print_format", "json", "-show_format", str(path)],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"ffprobe failed for {path}")
    return float(json.loads(result.stdout)["format"]["duration"])


def assemble_video(
    visuals: list[Path],
    narration_mp3: Path,
    srt_path: Path | None,
    workdir: Path,
    width: int,
    height: int,
    music: Path | None = None,
) -> Path:
    """Build the final mp4. All intermediate files live in workdir."""
    total = probe_duration(narration_mp3)
    per_scene = total / len(visuals)

    scale = (
        f"scale={width}:{height}:force_original_aspect_ratio=increase,"
        f"crop={width}:{height},fps=30,format=yuv420p"
    )

    # 1. Normalize every visual to an identically-encoded segment of equal length.
    segments = []
    for i, visual in enumerate(visuals):
        seg = workdir / f"seg_{i:02d}.mp4"
        if visual.suffix.lower() == ".png":
            inputs = ["-loop", "1", "-t", f"{per_scene:.3f}", "-i", visual.name]
        else:
            inputs = ["-stream_loop", "-1", "-i", visual.name, "-t", f"{per_scene:.3f}"]
        _run(
            ["ffmpeg", "-y", *inputs, "-vf", scale, "-an",
             "-c:v", "libx264", "-preset", "veryfast", "-crf", "20", seg.name],
            cwd=workdir,
        )
        segments.append(seg)

    # 2. Concatenate the uniform segments.
    concat_list = workdir / "concat.txt"
    concat_list.write_text("".join(f"file '{s.name}'\n" for s in segments))
    silent = workdir / "silent.mp4"
    _run(
        ["ffmpeg", "-y", "-f", "concat", "-safe", "0", "-i", concat_list.name,
         "-c", "copy", silent.name],
        cwd=workdir,
    )

    # 3. Mux narration (and optional music), burn subtitles.
    final = workdir / "final.mp4"
    args = ["ffmpeg", "-y", "-i", silent.name, "-i", narration_mp3.name]
    if music is not None:
        args += ["-stream_loop", "-1", "-i", str(music)]
        args += [
            "-filter_complex",
            "[2:a]volume=0.12[m];[1:a][m]amix=inputs=2:duration=first:dropout_transition=3[a]",
            "-map", "0:v", "-map", "[a]",
        ]
    else:
        args += ["-map", "0:v", "-map", "1:a"]
    if srt_path is not None:
        font_size = 20 if height > width else 16
        style = (
            f"FontSize={font_size},PrimaryColour=&HFFFFFF&,"
            "OutlineColour=&H80000000&,BorderStyle=1,Outline=2,Shadow=0,MarginV=40"
        )
        args += ["-vf", f"subtitles={srt_path.name}:force_style='{style}'"]
        args += ["-c:v", "libx264", "-preset", "veryfast", "-crf", "20"]
    else:
        args += ["-c:v", "copy"]
    args += ["-c:a", "aac", "-b:a", "192k", "-shortest", final.name]
    _run(args, cwd=workdir)
    return final
