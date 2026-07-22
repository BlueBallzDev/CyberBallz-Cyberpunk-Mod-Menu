"""Web-search-grounded research: trend-driven topic selection and per-topic
fact gathering, so scripts are current, accurate, and specific."""

import json

import anthropic

WEB_SEARCH_TOOL = {"type": "web_search_20260209", "name": "web_search", "max_uses": 8}

TOPIC_PICK_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "required": ["topic"],
    "properties": {"topic": {"type": "string"}},
}


def _searched_completion(cfg: dict, prompt: str) -> str:
    """Run a web-search-enabled request, resuming pause_turn until complete."""
    client = anthropic.Anthropic()
    messages = [{"role": "user", "content": prompt}]
    while True:
        response = client.messages.create(
            model=cfg["api"]["model"],
            max_tokens=16000,
            thinking={"type": "adaptive"},
            tools=[WEB_SEARCH_TOOL],
            messages=messages,
        )
        if response.stop_reason == "pause_turn":
            messages.append({"role": "assistant", "content": response.content})
            continue
        return "".join(b.text for b in response.content if b.type == "text")


def research_trending_topic(cfg: dict, past_titles: list[str],
                            performance: str | None = None) -> dict:
    """Search the web for what's pulling attention in the niche right now and
    pick the single best next topic. Returns {"topic": str, "brief": str}."""
    ch = cfg["channel"]
    past = "\n".join(f"- {t}" for t in past_titles[-40:]) or "(none yet)"
    performance_block = ""
    if performance:
        performance_block = f"""
AUDIENCE SIGNAL - how this channel's videos have actually performed:
{performance}
Weigh this heavily: pick topics that rhyme with what held viewers, and avoid
the patterns that lost them.
"""

    brief = _searched_completion(cfg, f"""You are the research lead for a YouTube channel.

Channel niche: {ch['niche']}
Audience: {ch['audience']}

Videos already published (the new topic must not overlap these):
{past}
{performance_block}

Search the web to find what is genuinely pulling attention in this niche right
now: recent developments and announcements, questions people are suddenly
asking, stories rising in coverage, and evergreen subjects with a fresh news
hook. Judge candidates on click appeal AND watch-time potential (is there a
real story arc, or just one fact?).

Then commit to the SINGLE best next video topic and write a research brief:

1. CHOSEN TOPIC — one line.
2. WHY NOW — why this will earn clicks and retention this week specifically.
3. VERIFIED FACTS — 8 to 12 concrete facts, numbers, names, and dates you
   confirmed in your searches, each with its source named inline. Only include
   facts you actually verified; these become the script's factual backbone.
4. MISCONCEPTION — the thing most viewers get wrong about this (great hook
   material).
5. ANGLE — the take competitors' videos on this subject are missing.""")

    client = anthropic.Anthropic()
    response = client.messages.create(
        model=cfg["api"]["model"],
        max_tokens=1024,
        thinking={"type": "adaptive"},
        output_config={"format": {"type": "json_schema", "schema": TOPIC_PICK_SCHEMA}},
        messages=[{
            "role": "user",
            "content": "Extract the CHOSEN TOPIC line from this research brief, "
                       f"as a short video-topic phrase:\n\n{brief}",
        }],
    )
    text = next(b.text for b in response.content if b.type == "text")
    return {"topic": json.loads(text)["topic"], "brief": brief}


def research_topic_facts(cfg: dict, topic: str) -> str:
    """Gather a verified fact sheet for a known topic before scripting."""
    ch = cfg["channel"]
    return _searched_completion(cfg, f"""You are fact-checking and researching for a YouTube video.

Channel niche: {ch['niche']}
Video topic: {topic}

Search the web and produce a research brief for the scriptwriter:

1. VERIFIED FACTS — 8 to 12 concrete facts, numbers, names, and dates you
   confirmed in your searches, each with its source named inline. Prefer
   surprising, specific details over general background.
2. RECENT DEVELOPMENTS — anything from the last year that changes or updates
   the common understanding of this topic.
3. MISCONCEPTION — what most people get wrong about this.
4. THE STORY — the most compelling narrative arc through this material
   (tension, turn, payoff), in 2-3 sentences.

Only include facts you actually verified in your searches. If a commonly
repeated claim did not check out, say so explicitly - that is valuable.""")
