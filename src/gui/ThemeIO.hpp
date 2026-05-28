#pragma once
#include "Theme.hpp"

#include <string>
#include <vector>

// Loads JSON theme files into a LoadedTheme, and enumerates themes found in the
// configured theme directories.
namespace cm::gui::themeio
{
struct ThemeEntry
{
    std::string name;     // display name (folder or file stem)
    std::string jsonPath; // full path to the .json
};

// Discover themes under the configured theme dir + the local themes folder.
std::vector<ThemeEntry> ScanThemes();

// Parse a JSON theme file into a LoadedTheme. Returns false on failure.
bool LoadTheme(const std::string& jsonPath, theme::LoadedTheme& out);

// Apply the theme named in config (or built-in if none / on failure).
void ApplyConfiguredTheme();
} // namespace cm::gui::themeio
