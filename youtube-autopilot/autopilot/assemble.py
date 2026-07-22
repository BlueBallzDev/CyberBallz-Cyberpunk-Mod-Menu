"""FFmpeg video assembly: normalize clips with motion and grade, crossfade,
add narration + captions + ducked music."""

import json
import subprocess
from pathlib import Path

FADE = 0.5  # crossfade duration between scenes, seconds


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


def extract_frame(video: Path, dest: Path, at_fraction: float = 0.25) -> Path:
    """Grab a representative frame from the finished video for the thumbnail."""
    t = probe_duration(video) * at_fraction
    _run(
        ["ffmpeg", "-y", "-ss", f"{t:.2f}", "-i", video.name,
         "-frames:v", "1", dest.name],
        cwd=video.parent,
    )
    return dest


def _motion_filter(is_image: bool, index: int, duration: float,
                   width: int, height: int) -> str:
    """Slow Ken Burns push per scene — in on even scenes, out on odd — so
    nothing on screen is ever static."""
    if is_image:
        # zoompan on a looped still; zoom follows the output frame counter
        frames = max(1, int(duration * 30))
        if index % 2 == 0:
            z = "min(1+0.0022*on,1.16)"
        else:
            z = "max(1.16-0.0022*on,1.0)"
        return (
            f"scale={width * 2}:{height * 2},"
            f"zoompan=z='{z}':x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)'"
            f":d=1:s={width}x{height}:fps=30"
        )
    # video: time-varying center crop = gentle push in/out
    if index % 2 == 0:
        expr = f"(1-0.10*t/{duration:.3f})"
    else:
        expr = f"(0.90+0.10*t/{duration:.3f})"
    return (
        f"crop=w='floor(iw*{expr}/2)*2':h='floor(ih*{expr}/2)*2',"
        f"scale={width}:{height}"
    )


GRADE = "eq=contrast=1.05:saturation=1.12,vignette=PI/5"


def assemble_video(
    visuals: list[Path],
    narration_mp3: Path,
    captions_path: Path | None,
    workdir: Path,
    width: int,
    height: int,
    music: Path | None = None,
    motion: bool = True,
    grade: bool = True,
) -> Path:
    """Build the final mp4. captions_path may be .srt (classic bottom style)
    or .ass (pop/karaoke style — carries its own styling)."""
    total = probe_duration(narration_mp3)
    n = len(visuals)
    # Crossfades overlap segments, so pad each segment's length such that the
    # joined video still matches the narration: n*per - (n-1)*FADE == total.
    per_scene = (total + (n - 1) * FADE) / n

    fill = (
        f"scale={width}:{height}:force_original_aspect_ratio=increase,"
        f"crop={width}:{height}"
    )

    # 1. Normalize every visual to an identically-encoded segment of equal
    #    length, with per-scene motion and a light grade.
    segments = []
    for i, visual in enumerate(visuals):
        seg = workdir / f"seg_{i:02d}.mp4"
        is_image = visual.suffix.lower() == ".png"
        if is_image:
            inputs = ["-framerate", "30", "-loop", "1",
                      "-t", f"{per_scene:.3f}", "-i", visual.name]
            # stills scale inside the motion filter; without motion, fill-crop
            vf = _motion_filter(True, i, per_scene, width, height) if motion else fill
        else:
            inputs = ["-stream_loop", "-1", "-i", visual.name, "-t", f"{per_scene:.3f}"]
            vf = fill + ("," + _motion_filter(False, i, per_scene, width, height)
                         if motion else "")
        if grade:
            vf += "," + GRADE
        vf += ",fps=30,format=yuv420p"
        _run(
            ["ffmpeg", "-y", *inputs, "-vf", vf, "-an",
             "-c:v", "libx264", "-preset", "veryfast", "-crf", "20", seg.name],
            cwd=workdir,
        )
        segments.append(seg)

    # 2. Join the segments with crossfades (plain copy when there's only one).
    silent = workdir / "silent.mp4"
    if n == 1:
        _run(["ffmpeg", "-y", "-i", segments[0].name, "-c", "copy", silent.name],
             cwd=workdir)
    else:
        args = ["ffmpeg", "-y"]
        for seg in segments:
            args += ["-i", seg.name]
        chains = []
        prev = "[0:v]"
        for i in range(1, n):
            out = f"[v{i}]"
            offset = i * (per_scene - FADE)
            chains.append(
                f"{prev}[{i}:v]xfade=transition=fade:duration={FADE}:offset={offset:.3f}{out}"
            )
            prev = out
        args += [
            "-filter_complex", ";".join(chains), "-map", prev,
            "-c:v", "libx264", "-preset", "veryfast", "-crf", "20", silent.name,
        ]
        _run(args, cwd=workdir)

    # 3. Mux narration (plus music ducked under the voice), burn captions.
    final = workdir / "final.mp4"
    args = ["ffmpeg", "-y", "-i", silent.name, "-i", narration_mp3.name]
    if music is not None:
        args += ["-stream_loop", "-1", "-i", str(music)]
        args += [
            "-filter_complex",
            # music at bed level, compressed hard whenever the voice speaks
            "[2:a]volume=0.35[m];"
            "[m][1:a]sidechaincompress=threshold=0.02:ratio=12:attack=10:release=400[duck];"
            "[1:a][duck]amix=inputs=2:duration=first:dropout_transition=2:normalize=0[a]",
            "-map", "0:v", "-map", "[a]",
        ]
    else:
        args += ["-map", "0:v", "-map", "1:a"]
    if captions_path is not None:
        if captions_path.suffix.lower() == ".ass":
            args += ["-vf", f"subtitles={captions_path.name}"]
        else:
            font_size = 20 if height > width else 16
            style = (
                f"FontSize={font_size},PrimaryColour=&HFFFFFF&,"
                "OutlineColour=&H80000000&,BorderStyle=1,Outline=2,Shadow=0,MarginV=40"
            )
            args += ["-vf", f"subtitles={captions_path.name}:force_style='{style}'"]
        args += ["-c:v", "libx264", "-preset", "veryfast", "-crf", "20"]
    else:
        args += ["-c:v", "copy"]
    args += ["-c:a", "aac", "-b:a", "192k", "-shortest", final.name]
    _run(args, cwd=workdir)
    return final
