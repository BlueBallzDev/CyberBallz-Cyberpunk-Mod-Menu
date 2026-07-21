# youtube-autopilot

A fully automated faceless YouTube pipeline. One command takes a topic and:

1. **Writes the video** — script, title, description, tags, and thumbnail text
   via the Claude API, tuned to your channel's niche/tone in `config.yaml`.
2. **Voices it** — free neural text-to-speech (edge-tts) with word-accurate
   subtitles generated as a side effect.
3. **Sources visuals** — per-scene stock footage from Pexels (free API), with
   generated background slides as a fallback.
4. **Assembles the video** — FFmpeg normalizes, concatenates, mixes optional
   background music, and burns captions.
5. **Makes a thumbnail** — bold-text 1280x720 PNG.
6. **Publishes** — YouTube Data API upload with title/description/tags,
   optional scheduling, and the custom thumbnail.

Supports 16:9 long-form and 9:16 Shorts (`video.aspect` in config).

```
topics.yaml ─▶ script (Claude) ─▶ TTS + SRT ─▶ stock footage ─▶ FFmpeg ─▶ upload
                                                                  │
                                             thumbnail.png ───────┘
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

### 3. YouTube upload credentials (one-time)

1. In [Google Cloud Console](https://console.cloud.google.com/), create a
   project and **enable the YouTube Data API v3**.
2. Configure the OAuth consent screen (External is fine; add your own Google
   account as a test user).
3. Create an **OAuth client ID → Desktop app**, download the JSON, and save it
   as `youtube-autopilot/client_secret.json`.
4. On a machine with a browser, run:

   ```bash
   python run.py auth
   ```

   This saves `state/token.json`. For a headless server or CI, run the auth
   locally once and copy that file over.

## Usage

```bash
python run.py run --dry-run      # script + metadata only, printed as JSON
python run.py run --no-upload    # full render, no publish — check output/
python run.py run                # produce and publish the next queued topic
python run.py run --topic "..."  # one-off topic
python run.py topics             # print fresh AI-generated topic ideas
```

Start with `--dry-run`, then `--no-upload`, and watch a couple of finished
videos before letting it publish unattended.

## Full automation

- **cron (any server):** `0 15 * * * cd /path/to/youtube-autopilot && python run.py run`
- **GitHub Actions:** copy `examples/github-actions-daily.yml` to
  `.github/workflows/` and add the listed repository secrets. It runs daily,
  uploads, and commits `state/published.json` back so topics never repeat.

## Things YouTube will make you deal with (read this)

These are platform constraints, not bugs in the pipeline:

- **Unverified-app private lock.** Videos uploaded through the API from an
  OAuth project that hasn't completed Google's API audit are **locked to
  private**. For a real public channel, request the audit/verification from
  the [YouTube API Services form](https://support.google.com/youtube/contact/yt_api_form)
  — it's routine for legitimate channels, but plan for a wait.
- **Quota.** `videos.insert` costs 1,600 units against a default 10,000
  units/day — roughly 6 uploads/day max unless you request a quota increase.
  1–2 uploads a day is comfortably inside the default.
- **Custom thumbnails** require a phone-verified channel
  (youtube.com/verify). The pipeline degrades gracefully if not verified.
- **Monetization policy.** YouTube's Partner Program rules (updated July 2025)
  demonetize "inauthentic" mass-produced content. Fully automated channels can
  be monetized when the content has genuine informational value and original
  writing — which is why the script prompt pushes specific, factual, hook-driven
  writing instead of generic filler. Low-effort spam volume is the thing that
  gets channels rejected, not automation per se.
- **AI disclosure.** YouTube requires disclosure for realistic synthetic
  media. `upload.ai_disclosure: true` appends a disclosure line to every
  description; keep it on.
- **Titles** are capped at 100 characters, descriptions at 5,000; the pipeline
  enforces the title cap.

## Tuning quality

- `config.yaml → channel.*` matters most: the more specific the niche, audience,
  and tone, the better the scripts.
- Pick a voice you like: `edge-tts --list-voices`. For a paid quality bump,
  `autopilot/tts.py` is ~60 lines and easy to swap for ElevenLabs or OpenAI TTS.
- Background music: put a file you have rights to at `assets/music.mp3` and set
  `video.music: assets/music.mp3`. It's mixed at low volume under the narration.
- Curate `topics.yaml` yourself for best results; `auto_topics` keeps the
  channel alive when the queue runs dry.

## Cost per video (approx.)

| Item | Cost |
|---|---|
| Script generation (Claude) | ~$0.05–0.15 |
| TTS (edge-tts) | Free |
| Stock footage (Pexels) | Free |
| YouTube upload | Free (quota-limited) |
