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

from dotenv import load_dotenv

from autopilot import assemble, research, script_gen, state, tts, uploader, visuals
from autopilot.config import (
    load_config, load_topics, require_env, resolve_channel, video_dimensions,
)
from autopilot.thumbnail import make_thumbnail

AI_DISCLOSURE_LINE = (
    "\n\nThis video's narration and voiceover were produced with AI assistance."
)

SHORT_MAX_MINUTES = 0.9


def pick_topic(cfg: dict, channel: dict, override: str | None) -> tuple[str, str | None]:
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
            picked = research.research_trending_topic(cfg, past)
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

    topic, brief = pick_topic(cfg, channel, args.topic)
    print(f"Channel: {channel['name']}  |  Format: {cfg['video']['aspect']}")
    print(f"Topic: {topic}")

    if brief is None and cfg["api"].get("research", True):
        print("Researching the topic (web search)...")
        brief = research.research_topic_facts(cfg, topic)

    print("Generating script + metadata...")
    package = script_gen.generate_polished_script(cfg, topic, research=brief)
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


def cmd_topics(args: argparse.Namespace) -> None:
    channel = resolve_channel(args.channel)
    cfg = load_config(channel["config"])
    require_env("ANTHROPIC_API_KEY")
    past = [e["title"] for e in state.load_published(channel["state_dir"])]
    for topic in script_gen.generate_topics(cfg, past):
        print(f"- {topic}")


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

    p_topics = sub.add_parser("topics", help="print fresh topic ideas")
    p_topics.add_argument("--channel", help="channel profile under channels/")

    args = parser.parse_args()
    if args.command == "auth":
        ch = resolve_channel(args.channel)
        uploader.run_auth_flow(ch["client_secret"], ch["token"])
    elif args.command == "run":
        cmd_run(args)
    elif args.command == "topics":
        cmd_topics(args)


if __name__ == "__main__":
    main()
