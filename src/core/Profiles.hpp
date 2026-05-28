#pragma once
#include <string>
#include <vector>

// Operator Profiles: named loadouts of the menu's continuous toggles + values,
// saved as JSON under %LOCALAPPDATA%/CyberBallz/profiles/. Plus built-in
// "persona" presets applied directly to State. Pure data layer - the UI lives
// in the Profiles tab.
namespace cm::profiles
{
// Profile files on disk (stem names, no extension).
std::vector<std::string> List();

// Save the current State toggles/values to <name>.json. Returns false on I/O error.
bool Save(const std::string& name);

// Load <name>.json and apply it to State. Returns false if missing/invalid.
bool Load(const std::string& name);

// Delete a saved profile.
bool Delete(const std::string& name);

// Built-in personas applied directly to State (no file needed).
struct Persona
{
    const char* name;
    const char* description;
};
const std::vector<Persona>& Personas();
void ApplyPersona(int index);

// Reset all continuous toggles to their defaults (Safe Mode / "all off").
void ClearAll();

// Open the profiles folder in Explorer so users can copy/share .json files
// (export) or drop shared ones in (import, then Rescan).
void OpenFolder();
} // namespace cm::profiles
