#pragma once
#include <imgui.h>

namespace cm::gui::widgets
{
enum class Icon
{
    Dashboard,
    Player,
    Combat,
    Stealth,
    Netrunner,
    Buffs,
    Inventory,
    Vehicle,
    World,
    Teleport,
    Profiles,
    Settings,
};

// Smoothly animate a per-id float toward target (frame-rate independent).
float Anim(ImGuiID id, float target, float speed = 12.0f);

// Vector icon drawn into a draw list, fitting a box of `size` around `center`.
void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 center, float size, ImU32 color);

// Sidebar
bool NavItem(const char* label, Icon icon, bool selected);

// Section / layout
void SectionLabel(const char* text);          // small uppercase divider label
bool BeginCard(const char* id, float padding = 18.0f);
void EndCard();
void Tooltip(const char* text);

// Controls (full-width rows)
bool ToggleRow(const char* label, bool* v, const char* subtitle = nullptr, bool experimental = false);
bool SliderRow(const char* label, float* v, float vmin, float vmax, const char* fmt = "%.2f",
               const char* subtitle = nullptr);
bool IntSliderRow(const char* label, int* v, int vmin, int vmax, const char* subtitle = nullptr);
bool ButtonRow(const char* label, const char* buttonText, const char* subtitle = nullptr,
               bool experimental = false);
bool InputTextRow(const char* label, char* buf, size_t bufSize, const char* subtitle = nullptr);

// Standalone
bool AccentButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool GhostButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool KeyCaptureButton(const char* label, int* vkey);
void StatBar(const char* label, float value, float maxValue, ImU32 color, const char* overlay = nullptr);
void Badge(const char* text, ImU32 color);
} // namespace cm::gui::widgets
