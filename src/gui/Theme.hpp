#pragma once
#include <imgui.h>

#include <string>

// Dark theme: near-black panels, a single vivid accent, soft rounding, and a
// clean modern typeface (Segoe UI). All colors derive from the active accent so
// the whole UI re-skins from one preset.
namespace cm::gui::theme
{
struct Palette
{
    ImU32 accent;        // primary accent
    ImU32 accentDim;     // muted accent (idle indicators)
    ImU32 accentSoft;    // translucent accent (hover fills)
    ImU32 windowBg;      // root window background
    ImU32 sidebarBg;     // navigation rail background
    ImU32 panel;         // card / section background
    ImU32 panelHover;    // hovered card / control
    ImU32 panelActive;   // pressed control
    ImU32 border;        // hairline borders
    ImU32 text;          // primary text
    ImU32 textDim;       // secondary text
    ImU32 textFaint;     // tertiary text / hints
    ImU32 success;       // ok / on
    ImU32 danger;        // destructive
    ImU32 track;         // toggle/slider track
};

struct Fonts
{
    ImFont* body = nullptr;    // default body text
    ImFont* caption = nullptr; // footer / hints (small)
    ImFont* header = nullptr;  // tab titles
    ImFont* title = nullptr;   // brand logo
};

struct AccentPreset
{
    const char* name;
    ImU32 color;
};

// A theme loaded from a JSON theme file: a full palette + ImGui metrics
// (+ optional background image path). Populated by themeio::LoadTheme.
struct LoadedTheme
{
    bool valid = false;
    std::string name;
    Palette palette{};

    // Style metrics pulled from the theme.
    float windowRounding = 6.0f;
    float childRounding = 6.0f;
    float frameRounding = 5.0f;
    float popupRounding = 6.0f;
    float tabRounding = 6.0f;
    float scrollbarRounding = 9.0f;
    float scrollbarSize = 10.0f;
    float childBorderSize = 1.0f;
    float frameBorderSize = 0.0f;
    float popupBorderSize = 1.0f;
    ImVec2 framePadding = ImVec2(12, 7);
    ImVec2 itemSpacing = ImVec2(8, 6);
    ImVec2 itemInnerSpacing = ImVec2(6, 5);

    // Optional animated/static background image (sibling of the .json).
    std::string backgroundPath;
};

void Apply();              // build fonts + style (call once, after ImGui ctx)
void Refresh();            // re-apply palette + style (after accent/scale change)
const Palette& Colors();   // current palette
const Fonts& GetFonts();
void SetAccent(int presetIndex);
void SetUiScale(float scale);

// Theme loading: a loaded theme replaces the built-in palette + metrics.
void ApplyLoadedTheme(const LoadedTheme& theme);
void UseBuiltIn();
bool IsThemeLoaded();
const std::string& LoadedThemeName();
const std::string& BackgroundPath();
int AccentCount();
const AccentPreset& AccentAt(int index);

// Small color helpers.
ImU32 WithAlpha(ImU32 color, float alpha);
ImU32 Lerp(ImU32 a, ImU32 b, float t);
} // namespace cm::gui::theme
