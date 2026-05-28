#pragma once
#include <Windows.h>

// Window subclassing for input: routes messages to ImGui, toggles the menu on
// the configured hotkey, and swallows game input while the menu is open.
namespace cm::render::input
{
void Install(HWND hwnd);
void Uninstall();
} // namespace cm::render::input
