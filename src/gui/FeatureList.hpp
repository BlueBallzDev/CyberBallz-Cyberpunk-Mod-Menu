#pragma once

// Renders feature toggles straight from the central registry (core/Toggles),
// each with a pin/favorite control. Used by the live search results and the
// Favorites view so both stay in sync with no per-tab wiring.
namespace cm::gui
{
// filter: case-insensitive substring matched against label/category/tooltip/id
//         (nullptr or "" = no filter). favoritesOnly: show only pinned toggles.
// Returns the number of rows drawn.
int DrawFeatureList(const char* filter, bool favoritesOnly);
} // namespace cm::gui
