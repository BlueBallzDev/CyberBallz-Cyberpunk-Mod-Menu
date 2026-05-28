#pragma once
#include <cstdint>
#include <string>

// Lightweight key=value persistence for menu settings, stored at
// %LOCALAPPDATA%\CyberBallz\config.ini. Dependency-free.
namespace cm::config
{
// Persisted UI preferences not part of the game State.
struct UiPrefs
{
    int toggleKey = 0x2D;   // VK_INSERT
    int accentPreset = 0;   // index into the theme accent palette
    float uiScale = 1.0f;
    bool rememberOpen = false;
    bool startOpen = false;
    bool overlayEnabled = true; // install the DX12 overlay hook (off = safe/inert)
    bool showWatermark = true;  // on-screen "CyberBallz" watermark
    bool showFps = true;        // FPS counter in the watermark
    bool showDiagnostics = false; // Diagnostics panel on the Dashboard (troubleshooting)

    // Theming
    std::string themeDir; // folder to scan for theme files (set in Settings)
    std::string themeFile;                                // active theme .json (empty = built-in)
    bool animatedBackground = true;                       // draw the theme's background image

    // Pinned feature ids (toggles::Toggle::id), comma-separated in the ini.
    std::string favorites;
};

// Favorites helpers (operate on Prefs().favorites).
bool IsFavorite(const std::string& id);
void ToggleFavorite(const std::string& id);

UiPrefs& Prefs();

void Load();
// Save UI preferences only (theme, scale, hotkey, favorites, ...). Auto-called
// when a preference changes. Does NOT write feature toggles or slider values.
void Save();
// Save the feature loadout (every toggle + slider value) to loadout.ini. Only
// called when the user explicitly presses Save - toggles never auto-persist.
void SaveLoadout();
} // namespace cm::config
