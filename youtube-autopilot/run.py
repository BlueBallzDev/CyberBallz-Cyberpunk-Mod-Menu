#!/usr/bin/env python3
"""youtube-autopilot — one command from topic to published video.

    python run.py auth                 # one-time YouTube OAuth
    python run.py run                  # produce + upload the next queued topic
    python run.py run --topic "..."    # produce a specific topic
    python run.py run --no-upload      # render the video but don't publish
    python run.py run --dry-run        # generate script/metadata only, print it
    python run.py topics               # print fresh AI-generated topic ideas
"""

import argparse
import json
import re
import shutil
from datetime import datetime, timezone

from dotenv import load_dotenv

from autopilot import assemble, script_gen, state, tts, uploader, visuals
from autopilot.config import (
    OUTPUT_DIR, load_config, load_topics, require_env, video_dimensions,
)
from autopilot.thumbnail import make_thumbnail

AI_DISCLOSURE_LINE = (
    "\n\nThis video's narration and voiceover were produced with AI assistance."
)


def pick_topic(cfg: dict, override: str | None) -> str:
    if override:
        return override
    done = state.published_topics()
    for topic in load_topics():
        if topic not in done:
            return topic
    if not cfg["api"]["auto_topics"]:
        raise SystemExit("Topic queue exhausted. Add topics to topics.yaml or enable api.auto_topics.")
    print("Topic queue exhausted - generating fresh ideas...")
    past = [e["title"] for e in state.load_published()]
    ideas = script_gen.generate_topics(cfg, past)
    fresh = [t for t in ideas if t not in done]
    if not fresh:
        raise SystemExit("Could not generate an unused topic.")
    return fresh[0]


def cmd_run(args: argparse.Namespace) -> None:
    cfg = load_config()
    require_env("ANTHROPIC_API_KEY")
    if not args.dry_run and shutil.which("ffmpeg") is None:
        raise SystemExit("ffmpeg not found on PATH - install it first.")

    topic = pick_topic(cfg, args.topic)
    print(f"Topic: {topic}")

    print("Generating script + metadata...")
    package = script_gen.generate_script(cfg, topic)
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
    workdir = OUTPUT_DIR / f"{stamp}-{slug}"
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
        music=(OUTPUT_DIR.parent / music) if music else None,
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
        title=package["title"],
        description=description,
        tags=package["tags"],
        category_id=str(up["category_id"]),
        privacy=up["privacy"],
        publish_at=up["publish_at"],
        notify_subscribers=up["notify_subscribers"],
        thumbnail_path=thumb,
    )
    state.record_published(topic, package["title"], video_id)
    print("Done.")


def cmd_topics(_args: argparse.Namespace) -> None:
    cfg = load_config()
    require_env("ANTHROPIC_API_KEY")
    past = [e["title"] for e in state.load_published()]
    for topic in script_gen.generate_topics(cfg, past):
        print(f"- {topic}")


def main() -> None:
    load_dotenv()
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("auth", help="one-time YouTube OAuth flow")

    p_run = sub.add_parser("run", help="produce and upload the next video")
    p_run.add_argument("--topic", help="override the topic queue")
    p_run.add_argument("--dry-run", action="store_true",
                       help="generate script + metadata only")
    p_run.add_argument("--no-upload", action="store_true",
                       help="render the video but skip the upload")

    sub.add_parser("topics", help="print fresh topic ideas")

    args = parser.parse_args()
    if args.command == "auth":
        uploader.run_auth_flow()
    elif args.command == "run":
        cmd_run(args)
    elif args.command == "topics":
        cmd_topics(args)


if __name__ == "__main__":
    main()
