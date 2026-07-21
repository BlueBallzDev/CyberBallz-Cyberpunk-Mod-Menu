"""Script, title, description, and tag generation via the Claude API."""

import json

import anthropic

SCRIPT_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "required": ["title", "description", "tags", "thumbnail_text", "scenes"],
    "properties": {
        "title": {"type": "string"},
        "description": {"type": "string"},
        "tags": {"type": "array", "items": {"type": "string"}},
        "thumbnail_text": {"type": "string"},
        "scenes": {
            "type": "array",
            "items": {
                "type": "object",
                "additionalProperties": False,
                "required": ["narration", "footage_keywords"],
                "properties": {
                    "narration": {"type": "string"},
                    "footage_keywords": {"type": "string"},
                },
            },
        },
    },
}

TOPICS_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "required": ["topics"],
    "properties": {
        "topics": {"type": "array", "items": {"type": "string"}},
    },
}


def _extract_json(response) -> dict:
    text = next(b.text for b in response.content if b.type == "text")
    return json.loads(text)


def generate_script(cfg: dict, topic: str) -> dict:
    """Return {title, description, tags, thumbnail_text, scenes:[{narration, footage_keywords}]}."""
    ch, vid = cfg["channel"], cfg["video"]
    is_short = vid["aspect"] == "short"
    target_words = max(60, int(vid["target_minutes"] * 145))

    prompt = f"""You are the head writer for a faceless YouTube channel.

Channel niche: {ch['niche']}
Audience: {ch['audience']}
Tone: {ch['tone']}
Language: {ch['language']}
Format: {"vertical Short (under 60 seconds)" if is_short else "long-form video"}

Write a complete video package for this topic:

TOPIC: {topic}

Requirements:
- The narration is read aloud by a text-to-speech voice. Write natural spoken
  prose only: no headings, no emoji, no stage directions, no "[pause]" markers,
  no bullet lists. Spell numbers the way a narrator would say them.
- Total narration length: about {target_words} words.
- Split the narration into {"3-5" if is_short else "7-14"} scenes. Each scene is one
  coherent beat of the story. The first scene must hook the viewer in the first
  two sentences with a concrete, surprising fact or question - never "in this
  video we will".
- End with a single, natural closing line (a thought that lands, not "like and
  subscribe" spam; one brief subscribe mention is fine mid-video or at the end).
- For each scene, give footage_keywords: 2-4 plain words a stock-video site
  could match (e.g. "factory robot arm", "ocean cable ship"). Concrete and
  visual - no abstract concepts.
- title: under 90 characters, specific and curiosity-driven, accurate to the
  script's actual content. No all-caps words, at most one number.{" Include #Shorts at the end." if is_short else ""}
- description: 120-250 words. The first line must work as a standalone hook
  (it shows in search). Then a short summary paragraph, then 3-5 relevant
  hashtags on the final line.
- tags: 8-15 short search phrases, most specific first.
- thumbnail_text: at most 4 punchy words for the thumbnail overlay.
- Everything must be factually accurate. If a claim is uncertain, soften it or
  cut it - never invent statistics."""

    client = anthropic.Anthropic()
    response = client.messages.create(
        model=cfg["api"]["model"],
        max_tokens=16000,
        thinking={"type": "adaptive"},
        output_config={"format": {"type": "json_schema", "schema": SCRIPT_SCHEMA}},
        messages=[{"role": "user", "content": prompt}],
    )
    data = _extract_json(response)
    data["title"] = data["title"][:100]
    data["tags"] = data["tags"][:15]
    return data


def generate_topics(cfg: dict, past_titles: list[str], count: int = 10) -> list[str]:
    """Generate fresh topic ideas that don't repeat what was already covered."""
    ch = cfg["channel"]
    past = "\n".join(f"- {t}" for t in past_titles[-40:]) or "(none yet)"

    prompt = f"""You plan content for a faceless YouTube channel.

Channel niche: {ch['niche']}
Audience: {ch['audience']}

Videos already published (do not repeat or closely overlap these):
{past}

Propose {count} new video topics. Each topic must be a single specific,
concrete video idea with a built-in hook - the kind of premise a viewer clicks
because it promises one clear payoff. Avoid listicles, avoid vague themes,
avoid anything requiring breaking news to stay accurate."""

    client = anthropic.Anthropic()
    response = client.messages.create(
        model=cfg["api"]["model"],
        max_tokens=4096,
        thinking={"type": "adaptive"},
        output_config={"format": {"type": "json_schema", "schema": TOPICS_SCHEMA}},
        messages=[{"role": "user", "content": prompt}],
    )
    return _extract_json(response)["topics"]
