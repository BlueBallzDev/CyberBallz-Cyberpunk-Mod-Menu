#pragma once
#include <atomic>
#include <string>
#include <vector>

// Central registry of every on/off feature toggle, with metadata. One source of
// truth used by the console (`feature <id> on`), the live search bar, the
// favorites system, and the feature registry. Each entry points at the atomic
// that already lives in cm::State.
namespace cm::toggles
{
struct Toggle
{
    const char* id;          // stable identifier, e.g. "godMode"
    const char* label;       // display name, e.g. "God Mode"
    const char* category;    // tab/category, e.g. "Player"
    const char* tooltip;     // one-line description
    std::atomic<bool>* flag; // points into cm::State
    bool experimental;       // binding still being validated in-game
};

// All registered toggles (built once from cm::State).
const std::vector<Toggle>& All();

// Find by id or label, case-insensitive. nullptr if not found.
const Toggle* Find(const std::string& idOrLabel);

// Numeric (slider) values that should persist / save into profiles, paired with
// their atomic in cm::State. One source of truth for config + profiles.
struct FloatVal
{
    const char* id;
    std::atomic<float>* value;
};
const std::vector<FloatVal>& Floats();
} // namespace cm::toggles
