#pragma once

// Renders the developer console + RTTI explorer overlay (toggled with backtick).
// Drawn every frame by the Renderer, after the main menu. Reads/writes the
// thread-safe bridge in core/DebugConsole.
namespace cm::gui
{
void DrawDebugConsole();
} // namespace cm::gui
