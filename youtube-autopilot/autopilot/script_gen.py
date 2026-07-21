"""Script, title, description, and tag generation via the Claude API,
with an automated critique-and-revise quality gate."""

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

REVIEW_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "required": ["score", "issues"],
    "properties": {
        "score": {"type": "integer"},
        "issues": {"type": "array", "items": {"type": "string"}},
    },
}

# The writing rules that keep scripts from reading like generated copy.
STYLE_GUIDE = """WRITING STYLE - follow all of these:
- Write like one specific person talking to one viewer. Use contractions.
  Address the viewer as "you". An occasional "I" or aside is good.
- Vary sentence length aggressively. Follow a long sentence with a three-word
  one. Fragments are fine. Rhythm is everything for spoken delivery.
- Be concrete. Named people, real numbers, actual places, specific years.
  Every generalization gets replaced with an example.
- Open a curiosity loop in the hook and don't fully close it until near the
  end. Add a smaller re-hook every 30-45 seconds of narration ("and that's
  not even the strange part").
- BANNED words and constructions (these read as generated copy): delve,
  dive into, unpack, unlock, harness, leverage, game-changer, revolutionize,
  landscape, tapestry, realm, "in today's world", "in today's fast-paced",
  "it's important to note", "at the end of the day", "whether you're X or Y",
  "join us as we", "let's explore", "little did they know", "but here's the
  thing" more than once, "not just X, but Y" more than once.
- Never start consecutive sentences with the same word. Never stack three
  parallel clauses in a row. No rhetorical question more than once per minute.
- No moralizing summary ending ("...and that's why innovation matters").
  End on a concrete image, a sharp fact, or the payoff of the opening loop."""


def _extract_json(response) -> dict:
    text = next(b.text for b in response.content if b.type == "text")
    return json.loads(text)


def _create(cfg: dict, prompt: str, schema: dict, max_tokens: int = 16000) -> dict:
    client = anthropic.Anthropic()
    response = client.messages.create(
        model=cfg["api"]["model"],
        max_tokens=max_tokens,
        thinking={"type": "adaptive"},
        output_config={"format": {"type": "json_schema", "schema": schema}},
        messages=[{"role": "user", "content": prompt}],
    )
    return _extract_json(response)


def generate_script(cfg: dict, topic: str, research: str | None = None) -> dict:
    """Return {title, description, tags, thumbnail_text, scenes:[{narration, footage_keywords}]}."""
    ch, vid = cfg["channel"], cfg["video"]
    is_short = vid["aspect"] == "short"
    target_words = max(60, int(vid["target_minutes"] * 145))

    research_block = ""
    if research:
        research_block = f"""
VERIFIED RESEARCH (this is your factual backbone - build the script from these
facts, keep them accurate, and do not invent claims beyond them):
{research}
"""

    prompt = f"""You are the head writer for a professional YouTube channel.

Channel niche: {ch['niche']}
Audience: {ch['audience']}
Tone: {ch['tone']}
Language: {ch['language']}
Format: {"vertical Short (under 60 seconds)" if is_short else "long-form video"}

Write a complete video package for this topic:

TOPIC: {topic}
{research_block}
{STYLE_GUIDE}

STRUCTURE:
- The narration is read aloud by a voiceover. Natural spoken prose only: no
  headings, no emoji, no stage directions, no "[pause]" markers, no bullet
  lists. Spell numbers the way a narrator would say them.
- Total narration length: about {target_words} words.
- Split the narration into {"3-5" if is_short else "7-14"} scenes. Each scene is one
  coherent beat. The first scene must hook in the first two sentences with a
  concrete, surprising fact or question - never "in this video we will".
- One brief, natural subscribe mention at most - mid-video or woven into the
  ending, never a hard sell.
- For each scene, give footage_keywords: 2-4 plain words a stock-video site
  could match (e.g. "factory robot arm", "ocean cable ship"). Concrete and
  visual - no abstract concepts.

METADATA:
- title: under 90 characters, specific and curiosity-driven, accurate to what
  the script actually delivers. No all-caps words, at most one number.{" Include #Shorts at the end." if is_short else ""}
- description: 120-250 words. First line must work as a standalone hook (it
  shows in search). Then a short summary paragraph, then 3-5 relevant
  hashtags on the final line.
- tags: 8-15 short search phrases, most specific first.
- thumbnail_text: at most 4 punchy words for the thumbnail overlay.
- Factual accuracy beats drama. If a claim is uncertain, soften it or cut it."""

    data = _create(cfg, prompt, SCRIPT_SCHEMA)
    data["title"] = data["title"][:100]
    data["tags"] = data["tags"][:15]
    return data


def critique_script(cfg: dict, package: dict, research: str | None = None) -> dict:
    """Score a package 1-10 and list concrete fixes. Strict on purpose."""
    research_block = f"\nRESEARCH THE SCRIPT WAS BASED ON:\n{research}\n" if research else ""
    prompt = f"""You are a ruthless YouTube retention consultant and script editor.
Review this video package before it is produced. Be strict: an average
competent script scores 6. Reserve 8+ for scripts you would bet money on.

PACKAGE:
{json.dumps(package, indent=2)}
{research_block}
Score it 1-10 overall and list every concrete issue you find, quoting the
offending text where possible. Check for:

1. HOOK - would a cold viewer still be watching at second five? Is the
   curiosity loop real, or decorative?
2. RETENTION - dead spots, filler sentences, repeated points, a middle that
   sags, missing re-hooks, an ending that fizzles.
3. NATURALNESS - anything that reads like generated marketing copy: banned
   cliches (delve, dive into, game-changer, "in today's world", tidy triads,
   "not just X but Y"), uniform sentence rhythm, moralizing conclusions,
   missing contractions. Quote every instance.
4. FACTS - claims that are vague, suspicious, or unsupported{" by the research" if research else ""}.
   Numbers written in ways a narrator would stumble on.
5. TITLE AND PACKAGING - does the title over-promise what the script delivers?
   Is the thumbnail text punchy and non-redundant with the title? Does the
   description's first line hook?
6. TTS-READINESS - anything a text-to-speech voice would mangle: abbreviations,
   symbols, awkward parentheticals.

Every issue must be specific enough that a writer could fix it without asking
questions."""
    return _create(cfg, prompt, REVIEW_SCHEMA, max_tokens=8000)


def revise_script(cfg: dict, package: dict, review: dict, research: str | None = None) -> dict:
    """Rewrite the package to resolve every issue the critique raised."""
    research_block = f"\nVERIFIED RESEARCH (factual backbone, keep it accurate):\n{research}\n" if research else ""
    issues = "\n".join(f"- {i}" for i in review["issues"])
    prompt = f"""You are the head writer revising a YouTube video package after an
editorial review. Fix every issue below - do not argue with them, do not fix
them halfway, and do not introduce new problems while fixing them.

CURRENT PACKAGE:
{json.dumps(package, indent=2)}

EDITORIAL ISSUES TO RESOLVE:
{issues}
{research_block}
{STYLE_GUIDE}

Return the complete revised package in the same structure: same approximate
narration length, same scene-count range, footage_keywords still concrete and
visual, title under 90 characters, tags 8-15, thumbnail_text at most 4 words."""
    data = _create(cfg, prompt, SCRIPT_SCHEMA)
    data["title"] = data["title"][:100]
    data["tags"] = data["tags"][:15]
    return data


def generate_polished_script(cfg: dict, topic: str, research: str | None = None) -> dict:
    """Generate, then critique-and-revise until the package clears the bar."""
    package = generate_script(cfg, topic, research)
    if not cfg["api"].get("quality_gate", True):
        return package

    min_quality = cfg["api"].get("min_quality", 8)
    max_revisions = cfg["api"].get("max_revisions", 2)
    for round_num in range(max_revisions + 1):
        review = critique_script(cfg, package, research)
        print(f"  quality gate: {review['score']}/10 "
              f"({len(review['issues'])} issue(s))")
        if review["score"] >= min_quality or round_num == max_revisions:
            break
        print("  revising...")
        package = revise_script(cfg, package, review, research)
    return package


def generate_topics(cfg: dict, past_titles: list[str], count: int = 10) -> list[str]:
    """Offline topic ideas (no web research) - fallback when research is off."""
    ch = cfg["channel"]
    past = "\n".join(f"- {t}" for t in past_titles[-40:]) or "(none yet)"

    prompt = f"""You plan content for a YouTube channel.

Channel niche: {ch['niche']}
Audience: {ch['audience']}

Videos already published (do not repeat or closely overlap these):
{past}

Propose {count} new video topics. Each topic must be a single specific,
concrete video idea with a built-in hook - the kind of premise a viewer clicks
because it promises one clear payoff. Avoid listicles, avoid vague themes,
avoid anything requiring breaking news to stay accurate."""

    return _create(cfg, prompt, TOPICS_SCHEMA, max_tokens=4096)["topics"]
