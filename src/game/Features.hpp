#pragma once

// Game-thread feature pipeline. Call OnGameUpdate() once per frame from the
// RED4ext "Running" game state. It:
//   1. publishes a DisplayInfo snapshot for the UI,
//   2. applies continuous toggles (god mode, time scale, ...),
//   3. drains and executes one-shot actions queued by the UI.
namespace cm::features
{
void OnGameUpdate();

// Run the binding self-test + per-feature state read-back and write a Markdown
// report to %LOCALAPPDATA%\CyberBallz\selfcheck_<time>.md. Must be called on the
// game thread (it touches the scripting VM). Triggered by the `selfcheck`
// console command and the Dashboard button.
void RunSelfCheck();
} // namespace cm::features
