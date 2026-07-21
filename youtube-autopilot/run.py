#!/usr/bin/env python3
"""youtube-autopilot — one command from topic to published video.

    python run.py auth [--channel NAME]        # one-time YouTube OAuth (per channel)
    python run.py run  [--channel NAME]        # produce + upload the next queued topic
    python run.py run --channel tech --aspect short     # force a vertical Short
    python run.py run --channel tech --aspect landscape # force a regular video
    python run.py run --topic "..."            # produce a specific topic
    python run.py run --no-upload              # render but don't publish
    python run.py run --dry-run                # script/metadata only, printed
    python run.py topics [--channel NAME]      # print fresh AI topic ideas

Channels live in channels/<name>/ (config.yaml, topics.yaml, credentials,
state). With no --channel, the root-level config.yaml is used.
"""

import argparse
import json
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path

from dotenv import load_dotenv

from autopilot import (
    analytics, assemble, clips, research, script_gen, state, tts, uploader, visuals,
)
from autopilot.config import (
    load_config, load_topics, require_env, resolve_channel, video_dimensions,
)
from autopilot.thumbnail import make_thumbnail

AI_DISCLOSURE_LINE = (
    "\n\nThis video's narration and voiceover were produced with AI assistance."
)

SHORT_MAX_MINUTES = 0.9


def refresh_performance(channel: dict) -> str | None:
    """Best-effort analytics refresh + digest. Never blocks production."""
    try:
        if channel["token"].exists():
            updated = analytics.refresh_stats(channel)
            if updated:
                print(f"Analytics: refreshed stats for {updated} video(s)")
    except Exception as e:
        print(f"Analytics refresh skipped ({e}). If this is a permissions error, "
              f"re-run: python run.py auth --channel {channel['name']}")
    return analytics.performance_summary(state.load_published(channel["state_dir"]))


def pick_topic(cfg: dict, channel: dict, override: str | None,
               performance: str | None = None) -> tuple[str, str | None]:
    """Return (topic, research_brief_or_None)."""
    if override:
        return override, None
    done = state.published_topics(channel["state_dir"])
    for topic in load_topics(channel["topics"]):
        if topic not in done:
            return topic, None

    past = [e["title"] for e in state.load_published(channel["state_dir"])]
    if cfg["api"].get("research", True):
        print("Topic queue exhausted - researching what's trending in the niche...")
        for _ in range(3):
            picked = research.research_trending_topic(cfg, past, performance)
            if picked["topic"] not in done:
                return picked["topic"], picked["brief"]
        raise SystemExit("Research kept proposing already-covered topics.")
    if not cfg["api"]["auto_topics"]:
        raise SystemExit(
            f"Topic queue exhausted for channel {channel['name']!r}. "
            "Add topics or enable api.research / api.auto_topics."
        )
    print("Topic queue exhausted - generating fresh ideas...")
    ideas = script_gen.generate_topics(cfg, past)
    fresh = [t for t in ideas if t not in done]
    if not fresh:
        raise SystemExit("Could not generate an unused topic.")
    return fresh[0], None


def cmd_run(args: argparse.Namespace) -> None:
    channel = resolve_channel(args.channel)
    cfg = load_config(channel["config"])
    if args.aspect:
        cfg["video"]["aspect"] = args.aspect
    if args.minutes:
        cfg["video"]["target_minutes"] = args.minutes
    if (cfg["video"]["aspect"] == "short"
            and cfg["video"]["target_minutes"] > SHORT_MAX_MINUTES):
        print(f"Shorts must be under a minute - clamping length to {SHORT_MAX_MINUTES} min")
        cfg["video"]["target_minutes"] = 0.75

    require_env("ANTHROPIC_API_KEY")
    if not args.dry_run and shutil.which("ffmpeg") is None:
        raise SystemExit("ffmpeg not found on PATH - install it first.")

    performance = refresh_performance(channel)
    if performance:
        print("Analytics: feeding audience performance signal into generation")

    topic, brief = pick_topic(cfg, channel, args.topic, performance)
    print(f"Channel: {channel['name']}  |  Format: {cfg['video']['aspect']}")
    print(f"Topic: {topic}")

    if brief is None and cfg["api"].get("research", True):
        print("Researching the topic (web search)...")
        brief = research.research_topic_facts(cfg, topic)

    print("Generating script + metadata...")
    package = script_gen.generate_polished_script(cfg, topic, research=brief,
                                                  performance=performance)
    description = package["description"]
    if cfg["upload"]["ai_disclosure"]:
        description += AI_DISCLOSURE_LINE

    print(f"  title: {package['title']}")
    print(f"  scenes: {len(package['scenes'])}")

    if args.dry_run:
        print(json.dumps(package, indent=2))
        return

    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    slug = re.sub(r"[^a-z0-9]+", "-", package["title"].lower()).strip("-")[:48]
    workdir = channel["output_dir"] / f"{stamp}-{slug}"
    workdir.mkdir(parents=True, exist_ok=True)

    print("Synthesizing narration...")
    narration_text = " ".join(s["narration"] for s in package["scenes"])
    mp3 = workdir / "narration.mp3"
    srt = workdir / "captions.srt"
    tts.synthesize(narration_text, cfg["video"]["voice"], mp3, srt,
                   rate=cfg["video"]["voice_rate"])

    width, height = video_dimensions(cfg)
    print("Fetching visuals...")
    clips = visuals.fetch_scene_visuals(package["scenes"], workdir, width, height)

    print("Assembling video...")
    music = cfg["video"]["music"]
    final = assemble.assemble_video(
        clips, mp3,
        srt if cfg["video"]["captions"] else None,
        workdir, width, height,
        music=(channel["base"] / music) if music else None,
    )
    print(f"  rendered: {final}")

    thumb = make_thumbnail(package["thumbnail_text"], workdir / "thumbnail.png")

    if args.no_upload:
        print("Skipping upload (--no-upload). Video is ready in the output folder.")
        return

    print("Uploading to YouTube...")
    up = cfg["upload"]
    video_id = uploader.upload_video(
        final,
        token_file=channel["token"],
        title=package["title"],
        description=description,
        tags=package["tags"],
        category_id=str(up["category_id"]),
        privacy=up["privacy"],
        publish_at=up["publish_at"],
        notify_subscribers=up["notify_subscribers"],
        thumbnail_path=thumb,
    )
    state.record_published(channel["state_dir"], topic, package["title"], video_id)
    print("Done.")


def cmd_clip(args: argparse.Namespace) -> None:
    channel = resolve_channel(args.channel)
    cfg = load_config(channel["config"])
    require_env("ANTHROPIC_API_KEY")
    if shutil.which("ffmpeg") is None:
        raise SystemExit("ffmpeg not found on PATH - install it first.")
    if args.url:
        clips.require_ytdlp()

    aspect = args.aspect
    ccfg = cfg.get("clips", {})
    count = args.count or ccfg.get("count", 3)
    min_s = args.min_seconds or ccfg.get("min_seconds", 15)
    max_s = args.max_seconds or ccfg.get("max_seconds", 55)

    source_label = args.url or str(args.file)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    workdir = channel["output_dir"] / f"{stamp}-clips"
    workdir.mkdir(parents=True, exist_ok=True)
    print(f"Channel: {channel['name']}  |  Format: {aspect}  |  Source: {source_label}")

    # 1. Find the highlights: transcript mode if captions exist, else loudness.
    cues = None
    if args.url:
        print("Fetching captions...")
        vtt = clips.fetch_auto_subs(args.url, workdir)
        if vtt is not None:
            cues = clips.parse_vtt(vtt)
            print(f"  {len(cues)} caption cues")

    if cues:
        print("Selecting highlights from the transcript...")
        highlights = clips.select_highlights(
            cfg, clips.compact_transcript(cues), count, min_s, max_s, args.context,
        )
    else:
        if not args.context:
            raise SystemExit(
                "No captions found, so highlight titles can't be inferred. "
                "Re-run with --context \"streamer name, game, what the stream was\"."
            )
        print("No captions - analyzing audio energy for highlight spikes...")
        if args.url:
            media = clips.download_audio(args.url, workdir)
        else:
            media = Path(args.file)
            if media.parent != workdir:
                shutil.copy(media, workdir / media.name)
                media = workdir / media.name
        windows = clips.audio_energy_windows(media, count, max_s, workdir)
        highlights = [{"start_s": s, "end_s": e, "title": None} for s, e in windows]
    print(f"  {len(highlights)} highlight(s) selected")

    # 2. Produce and upload each clip.
    for i, h in enumerate(highlights):
        start, end = h["start_s"], h["end_s"]
        print(f"Clip {i + 1}/{len(highlights)}: "
              f"{clips._seconds_to_ts(start)}-{clips._seconds_to_ts(end)}")
        if args.url:
            raw = clips.download_section(args.url, start, end, i, workdir)
        else:
            raw = clips.cut_local_section(Path(args.file), start, end,
                                          workdir / f"raw_{i:02d}.mp4")
        srt = None
        if cues and cfg["video"]["captions"]:
            srt = clips.slice_srt(cues, start, end, workdir / f"clip_{i:02d}.srt")
        final = clips.render_clip(raw, workdir / f"clip_{i:02d}.mp4", aspect, srt)

        if h.get("title"):
            title, description, tags = h["title"], h["description"], h["tags"]
        else:
            meta = clips.clip_metadata(cfg, args.context, aspect)
            title, description, tags = meta["title"], meta["description"], meta["tags"]
        if aspect == "short" and "#shorts" not in title.lower():
            title = (title[:91] + " #Shorts") if len(title) > 91 else title + " #Shorts"
        print(f"  title: {title}")

        if args.no_upload:
            print(f"  rendered (not uploaded): {final}")
            continue
        up = cfg["upload"]
        video_id = uploader.upload_video(
            final,
            token_file=channel["token"],
            title=title[:100],
            description=description,
            tags=tags[:15],
            category_id=str(up["category_id"]),
            privacy=up["privacy"],
            publish_at=None,
            notify_subscribers=up["notify_subscribers"],
            thumbnail_path=None,
        )
        state.record_published(
            channel["state_dir"],
            f"clip: {source_label} @ {clips._seconds_to_ts(start)}",
            title, video_id,
        )
    print("Done.")


def cmd_topics(args: argparse.Namespace) -> None:
    channel = resolve_channel(args.channel)
    cfg = load_config(channel["config"])
    require_env("ANTHROPIC_API_KEY")
    past = [e["title"] for e in state.load_published(channel["state_dir"])]
    for topic in script_gen.generate_topics(cfg, past):
        print(f"- {topic}")


def cmd_stats(args: argparse.Namespace) -> None:
    channel = resolve_channel(args.channel)
    updated = analytics.refresh_stats(channel)
    entries = state.load_published(channel["state_dir"])
    print(f"Channel: {channel['name']}  ({updated} video(s) refreshed)\n")
    scored = [e for e in entries if e.get("stats")]
    if not scored:
        print("No analytics yet - stats appear once uploads have views.")
        return
    for e in sorted(scored, key=lambda e: e["stats"]["views"], reverse=True):
        s = e["stats"]
        print(f"{s['views']:>8} views  {s['avg_view_pct']:>5.1f}% watched  "
              f"{s['subs_gained']:>4} subs  {e['title']}")
    summary = analytics.performance_summary(entries)
    if summary:
        print("\nDigest fed to the model on the next run:\n" + summary)


def main() -> None:
    load_dotenv()
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p_auth = sub.add_parser("auth", help="one-time YouTube OAuth flow")
    p_auth.add_argument("--channel", help="channel profile under channels/")

    p_run = sub.add_parser("run", help="produce and upload the next video")
    p_run.add_argument("--channel", help="channel profile under channels/")
    p_run.add_argument("--topic", help="override the topic queue")
    p_run.add_argument("--aspect", choices=["landscape", "short"],
                       help="override the configured format for this run")
    p_run.add_argument("--minutes", type=float,
                       help="override the target length for this run")
    p_run.add_argument("--dry-run", action="store_true",
                       help="generate script + metadata only")
    p_run.add_argument("--no-upload", action="store_true",
                       help="render the video but skip the upload")

    p_clip = sub.add_parser(
        "clip",
        help="cut highlight clips from a stream VOD or local recording "
             "(only content you own or have permission to clip)",
    )
    p_clip.add_argument("--channel", help="channel profile under channels/")
    src = p_clip.add_mutually_exclusive_group(required=True)
    src.add_argument("--url", help="VOD URL (anything yt-dlp supports)")
    src.add_argument("--file", help="local recording path")
    p_clip.add_argument("--count", type=int, help="number of clips to produce")
    p_clip.add_argument("--aspect", choices=["short", "landscape"], default="short")
    p_clip.add_argument("--context",
                        help='e.g. "MyStreamName playing Cyberpunk 2077, chaos run" '
                             "(required when the VOD has no captions)")
    p_clip.add_argument("--min-seconds", type=int, dest="min_seconds")
    p_clip.add_argument("--max-seconds", type=int, dest="max_seconds")
    p_clip.add_argument("--no-upload", action="store_true",
                        help="render clips but skip the upload")

    p_topics = sub.add_parser("topics", help="print fresh topic ideas")
    p_topics.add_argument("--channel", help="channel profile under channels/")

    p_stats = sub.add_parser(
        "stats", help="refresh and show per-video analytics for a channel")
    p_stats.add_argument("--channel", help="channel profile under channels/")

    args = parser.parse_args()
    if args.command == "auth":
        ch = resolve_channel(args.channel)
        uploader.run_auth_flow(ch["client_secret"], ch["token"])
    elif args.command == "run":
        cmd_run(args)
    elif args.command == "clip":
        cmd_clip(args)
    elif args.command == "topics":
        cmd_topics(args)
    elif args.command == "stats":
        cmd_stats(args)


if __name__ == "__main__":
    main()
