#pragma once
#include <string>
#include <vector>

// In-game debug console + RTTI explorer bridge. The UI (render thread) submits
// command strings and reads the output log; the game thread drains and executes
// them in Pump() (so RTTI access is always on the game thread). Toggle with the
// backtick (`) key, independent of the main menu.
namespace cm::debug
{
void Submit(const std::string& command); // render thread: queue a command (echoed)
void Print(const std::string& line);     // any thread: append a line to the log
std::vector<std::string> Output();        // render thread: snapshot of the log
void ClearOutput();

void Pump(); // game thread: execute queued commands (RTTI-safe)

bool IsOpen();
void Toggle();
void SetOpen(bool open);
} // namespace cm::debug
