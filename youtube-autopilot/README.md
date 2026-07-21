# youtube-autopilot

A fully automated faceless YouTube pipeline — now with **multi-channel
support** so you can run several channels, each posting multiple times a day,
mixing Shorts and short regular videos. Per video it:

1. **Researches it** — Claude searches the web before every video: verifying
   facts and finding fresh angles for queued topics, and, when the queue is
   empty, picking whatever is genuinely pulling attention in the niche that
   week instead of guessing.
2. **Writes the video** — script, title, description, tags, and thumbnail text
   via the Claude API, tuned to each channel's niche/tone, built on the
   verified research.
3. **Quality-gates it** — a second, deliberately harsh editorial pass scores
   every script 1–10 for hook strength, retention structure, factual soundness,
   and naturalness (it hunts down generated-copy clichés specifically), then
   rewrites until the script clears the configured bar. Nothing mediocre
   reaches production.
4. **Voices it** — free neural text-to-speech (edge-tts) with word-accurate
   subtitles generated as a side effect.
5. **Sources visuals** — per-scene stock footage from Pexels (free API), with
   generated background slides as a fallback.
6. **Assembles the video** — FFmpeg normalizes clips, joins scenes with
   crossfade transitions, mixes optional background music, and burns captions.
7. **Makes a thumbnail** — bold-text 1280x720 PNG.
8. **Publishes** — YouTube Data API upload with title/description/tags,
   optional scheduling, and the custom thumbnail.

Supports 16:9 long-form and 9:16 Shorts, switchable per run.

```
topic queue ──┐
              ├─▶ web research ─▶ script ─▶ critique/revise loop ─▶ TTS + SRT
trend search ─┘    (Claude)      (Claude)        (Claude)              │
                                                                       ▼
              upload ◀─ thumbnail ◀─ FFmpeg (crossfades) ◀─ stock footage
```

## Setup

### 1. System requirements

- Python 3.10+
- **ffmpeg** on PATH (`sudo apt install ffmpeg`, `brew install ffmpeg`)

```bash
cd youtube-autopilot
pip install -r requirements.txt
cp .env.example .env   # then fill in the keys
```

### 2. API keys (.env)

| Key | Where | Cost |
|---|---|---|
| `ANTHROPIC_API_KEY` | platform.claude.com | Pennies per video (one script call) |
| `PEXELS_API_KEY` | pexels.com/api | Free (optional — slides fallback without it) |

Both keys are shared across all channels.

### 3. YouTube upload credentials (one-time, per channel)

1. In [Google Cloud Console](https://console.cloud.google.com/), create a
   project and **enable the YouTube Data API v3**. Use a **separate project
   per channel** — each project gets its own 10,000-unit daily quota and its
   own API audit, so one channel's limits never block another.
2. Configure the OAuth consent screen (External is fine; add your own Google
   account as a test user).
3. Create an **OAuth client ID → Desktop app**, download the JSON, and save it
   as `channels/<name>/client_secret.json` (or the repo root for the default
   channel).
4. On a machine with a browser:

   ```bash
   python run.py auth --channel <name>
   ```

   Sign in with the Google account that owns that channel — the resulting
   `channels/<name>/state/token.json` determines where uploads land. For a
   headless server or CI, run auth locally once and copy the token file over.

## Channels

Each channel is a folder under `channels/`:

```
channels/
  my-shorts-channel/
    config.yaml          # niche, tone, voice, format, upload settings
    topics.yaml          # that channel's topic queue
    client_secret.json   # that channel's OAuth client   (gitignored)
    state/
      token.json         # that channel's login          (gitignored)
      published.json     # what it has already posted
```

Two ready-made profiles are included — copy and rename them:

- `channels/example-shorts/` — Shorts-first: 45-second verticals, energetic
  voice, punchy hook-driven scripts.
- `channels/example-explainers/` — short regular videos: 3-minute 16:9
  documentary-style explainers.

Running without `--channel` uses the root-level `config.yaml` as a default
channel, same as before.

## Usage

```bash
python run.py run --channel my-shorts --dry-run     # script + metadata only
python run.py run --channel my-shorts --no-upload   # full render, no publish
python run.py run --channel my-shorts               # produce + publish next topic
python run.py run --channel my-explainers --aspect short   # one-off format override
python run.py run --minutes 2                        # one-off length override
python run.py topics --channel my-shorts             # fresh AI topic ideas
```

A channel configured for landscape can still post Shorts (`--aspect short`)
and vice versa — the script style, title (`#Shorts`), rendering, and caption
sizing all follow the override. Start every new channel with `--dry-run`, then
`--no-upload`, and watch a few finished videos before letting it publish
unattended.

## Full automation (multiple channels, multiple posts per day)

**cron (any server)** — one line per channel per slot:

```cron
0 13 * * *  cd /path/youtube-autopilot && python run.py run --channel my-shorts
0 17 * * *  cd /path/youtube-autopilot && python run.py run --channel my-explainers
0 22 * * *  cd /path/youtube-autopilot && python run.py run --channel my-shorts
0 18 * * *  cd /path/youtube-autopilot && python run.py run --channel my-explainers --aspect short
```

**GitHub Actions** — copy `examples/github-actions-multichannel.yml` to
`.github/workflows/`. It runs three times a day across every channel in its
matrix (Shorts in the morning/evening slots, a regular video midday), keyed to
per-channel secrets, and commits each channel's `published.json` back so
topics never repeat. `examples/github-actions-daily.yml` remains as a simpler
single-channel starting point.

## Things YouTube will make you deal with (read this)

These are platform constraints, not bugs in the pipeline:

- **Unverified-app private lock.** Videos uploaded through the API from an
  OAuth project that hasn't completed Google's API audit are **locked to
  private**. For a real public channel, request the audit/verification from
  the [YouTube API Services form](https://support.google.com/youtube/contact/yt_api_form)
  — routine for legitimate channels, but plan for a wait, and it's per
  Google Cloud project (another reason to use one project per channel).
- **Quota.** `videos.insert` costs 1,600 units of a project's default 10,000
  units/day — about 6 uploads/day per project. With one project per channel,
  2-4 posts/day/channel fits comfortably.
- **Custom thumbnails** require a phone-verified channel
  (youtube.com/verify). The pipeline degrades gracefully if not verified.
- **Monetization policy.** YouTube's Partner Program rules (updated July 2025)
  demonetize "inauthentic" mass-produced content, and this matters more the
  more you scale. Practical guidance: give each channel a genuinely distinct
  niche (not the same content re-skinned), keep the writing specific and
  factual, and ramp volume gradually — a brand-new channel posting 4x/day on
  day one looks like spam to both the algorithm and reviewers. Automation
  itself is allowed; low-value volume is what gets channels demonetized or
  removed.
- **More uploads ≠ proportionally more views.** YouTube distributes based on
  click-through and watch time per video. Two good videos a day beat six
  mediocre ones — if average performance drops, the algorithm shows the whole
  channel less. Scale a channel up only after its existing cadence is holding
  retention.
- **AI disclosure.** YouTube requires disclosure for realistic synthetic
  media. `upload.ai_disclosure: true` appends a disclosure line to every
  description; keep it on.
- **Titles** are capped at 100 characters, descriptions at 5,000; the pipeline
  enforces the title cap.

## Research and the quality gate

Both are on by default and configurable per channel in `config.yaml → api`:

- `research: true` — before scripting, Claude runs live web searches. For a
  queued topic it builds a verified fact sheet (numbers, names, dates, recent
  developments, the misconception worth debunking) that becomes the script's
  factual backbone. When the topic queue is empty it instead researches what's
  currently rising in the niche and picks the topic with the best click **and
  watch-time** potential — so an idle channel stays topical on its own.
- `quality_gate: true` — every script is reviewed by a deliberately strict
  editorial pass (an average script scores 6/10) checking hook strength,
  retention structure (dead spots, missing re-hooks, saggy middles), factual
  soundness against the research, packaging accuracy, and TTS-readiness. It
  specifically flags generated-copy tells — clichés like "delve", "game-changer",
  uniform sentence rhythm, tidy moralizing endings — and the script is rewritten
  until it scores `min_quality` (default 8) or `max_revisions` is exhausted.

The writing prompt itself enforces a spoken-voice style guide (contractions,
aggressive sentence-length variation, concrete specifics, curiosity loops,
a banned-phrase list), so the gate is a second net, not the only one.

**On "not detectable as AI":** what actually gets automated channels flagged —
by viewers and by YouTube — is *sloppy* AI content: generic scripts, factual
errors, monotone pacing. That's what the research step, style guide, and gate
attack, and it's the honest lever for views. What this pipeline won't do is
help misrepresent provenance: the `ai_disclosure` description line stays as a
config option, and YouTube separately requires disclosure for realistic
synthetic media. A well-written, well-researched video with a good voice
doesn't need to hide anything — quality reads as quality. For the last mile of
"doesn't sound synthetic", the biggest single upgrade is a premium voice
(swap `autopilot/tts.py` for ElevenLabs — ~60 lines).

## Tuning quality

- `config.yaml → channel.*` matters most: the more specific the niche,
  audience, and tone, the better the scripts. This is also your main tool for
  making channels feel distinct from each other.
- Pick a distinct voice per channel: `edge-tts --list-voices`. For a paid
  quality bump, `autopilot/tts.py` is ~60 lines and easy to swap for
  ElevenLabs or OpenAI TTS.
- Background music: put a file you have rights to at
  `channels/<name>/assets/music.mp3` and set `video.music: assets/music.mp3`.
  It's mixed at low volume under the narration.
- Curate each `topics.yaml` yourself for best results; `auto_topics` keeps a
  channel alive when its queue runs dry.

## Cost per video (approx.)

| Item | Cost |
|---|---|
| Web research (Claude + search) | ~$0.10–0.25 |
| Script + quality gate (Claude) | ~$0.10–0.30 |
| TTS (edge-tts) | Free |
| Stock footage (Pexels) | Free |
| YouTube upload | Free (quota-limited) |

Roughly $0.20–0.55 per video all-in. Disable `research`/`quality_gate` per
channel to trade quality for cost.
