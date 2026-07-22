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
6. **Assembles the video** — FFmpeg normalizes clips, adds a slow Ken Burns
   push to every scene (alternating in/out so nothing sits static), applies a
   light color grade + vignette, joins scenes with crossfades, ducks optional
   music under the voice, and burns captions. Shorts get word-pop karaoke
   captions (2-3 word groups, spoken word highlighted, center-third placement
   — the current retention meta); landscape gets a classic bottom caption bar.
   Both are per-channel configurable (`caption_style`, `motion`, `grade`).
7. **Makes a thumbnail** — bold-text 1280x720 PNG.
8. **Publishes** — YouTube Data API upload with title/description/tags,
   optional scheduling, and the custom thumbnail.

Supports 16:9 long-form and 9:16 Shorts, switchable per run — plus a
**stream/VOD clipper** (`run.py clip`) that finds highlight moments in long
recordings and cuts them into caption-burned clips (see below).

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
| `PEXELS_API_KEY` | pexels.com/api | Free (optional) |
| `PIXABAY_API_KEY` | pixabay.com/api/docs | Free (optional) |

Keys are shared across all channels. The two footage providers are
interchangeable — either alone works, both together maximize the match rate
(tried in order per scene: Pexels → Pixabay → generated slide).

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

> **Important for unattended automation:** while your OAuth consent screen is
> in **Testing** status, Google expires refresh tokens after 7 days — your
> automation would silently stop uploading weekly. Set the consent screen to
> **In production** (Cloud Console → OAuth consent screen → Publish app) so
> tokens stay valid indefinitely. You don't need Google's verification review
> for your own private use; "unverified app" warnings during auth are fine.

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

## The performance feedback loop

The system learns from your actual audience. Before every production run it
pulls each published video's metrics from the YouTube Analytics API — views,
watch minutes, average view duration, average percentage watched, likes,
subscribers gained — stores them in the channel's `published.json`, and builds
a what-worked/what-flopped digest (top performers by views, weakest videos by
retention). That digest is injected into:

- **trend research** — topic selection weighs toward subjects that rhyme with
  what held your viewers;
- **script + title generation** — the writer is told what your top titles and
  framings have in common, and what the low-retention videos did wrong.

It activates automatically once a channel has 3+ videos with view data; before
that, runs behave as normal. Inspect what the model sees with:

```bash
python run.py stats --channel my-shorts
```

Notes:
- The analytics scope was added to the OAuth flow — for channels authenticated
  before this feature, re-run `python run.py auth --channel <name>` once.
  (Uploads keep working on old tokens either way; the refresh just skips with
  a hint until you re-auth.)
- Click-through rate and impressions aren't exposed by the public Analytics
  API (they're YouTube Studio-only), so retention percentage is the primary
  quality signal — which is the metric the algorithm cares most about anyway.

## Clipping streams and VODs

`run.py clip` turns long recordings — stream VODs, podcasts, your own gameplay
sessions — into upload-ready highlight clips:

```bash
# 3 vertical Shorts from a VOD, auto-detected highlights, auto captions:
python run.py clip --channel my-clips --url "https://youtube.com/watch?v=VOD_ID"

# From a local recording, landscape, 5 clips, review before publishing:
python run.py clip --channel my-clips --file recording.mp4 --aspect landscape \
    --count 5 --context "my Cyberpunk 2077 chaos run" --no-upload
```

How it finds the highlights:

- **Transcript mode** (URLs with captions): the VOD's captions are fetched
  *without* downloading the video, Claude reads the timestamped transcript and
  picks the strongest self-contained moments — clean starts, a real peak, an
  ending right after the payoff — and writes each clip's title, description,
  and tags in the same pass. Only the selected time-ranges are then
  downloaded, so a 6-hour VOD doesn't mean a 6-hour download.
- **Audio-energy fallback** (no captions — most Twitch VODs, local files):
  loudness analysis finds the hype/laughter spikes and Claude writes honest
  metadata from the `--context` you provide.

Shorts get the standard clip-channel treatment: blurred-background 9:16
layout with the original footage centered and captions burned in. Landscape
clips keep the original frame. Clip length bounds and count are configurable
per channel (`config.yaml → clips`).

**Rights, in plain terms:** only clip content you own or have explicit
permission to clip (your own streams, or creators who allow clip channels —
many do, often with conditions like crediting or revenue share; check their
rules). Clipping without permission invites copyright strikes, and YouTube's
reused-content policy separately demonetizes re-uploads that add no original
value — captions, tight editing, and good moment selection help, but
permission is the foundation. The clipper never adds the AI-narration
disclosure line since clips contain real people, not synthetic narration.

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
- **Voices.** Two providers, per channel via `video.voice_provider`:
  - `edge` (default, free): pick a distinct voice per channel from
    `edge-tts --list-voices`.
  - `elevenlabs` (premium, the biggest "doesn't sound synthetic" upgrade):
    set `ELEVENLABS_API_KEY` in `.env` and optionally
    `video.elevenlabs_voice_id` (any voice from your ElevenLabs library).
    Word-level timestamps come from the API, so burned captions stay
    perfectly synced. Long scripts are chunked at sentence boundaries
    automatically. Cost is roughly $0.10–0.30 per minute of narration
    depending on your plan.
- **Thumbnails** are built from an actual frame of the finished video
  (darkened, with the bold text overlay) — falling back to a gradient if the
  frame grab fails.
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
