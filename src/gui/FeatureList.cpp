#include "FeatureList.hpp"
#include "Widgets.hpp"
#include "Theme.hpp"
#include "core/Toggles.hpp"
#include "core/Config.hpp"
#include "core/State.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace cm::gui
{
namespace
{
std::string Lower(std::string v)
{
    for (auto& c : v)
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return v;
}

bool Matches(const toggles::Toggle& t, const std::string& needle)
{
    if (needle.empty())
        return true;
    return Lower(t.label).find(needle) != std::string::npos ||
           Lower(t.category).find(needle) != std::string::npos ||
           Lower(t.tooltip).find(needle) != std::string::npos ||
           Lower(t.id).find(needle) != std::string::npos;
}

// A small pin control drawn as a filled (pinned) / hollow (unpinned) star, so
// it needs no special font glyphs.
bool PinStar(const char* id, bool pinned)
{
    const auto& P = theme::Colors();
    ImGui::PushID(id);
    const float sz = ImGui::GetFrameHeight();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##pin", ImVec2(sz, sz));
    const bool clicked = ImGui::IsItemClicked();
    const bool hov = ImGui::IsItemHovered();
    ImVec2 c(p.x + sz * 0.5f, p.y + sz * 0.5f);
    const float r = sz * 0.30f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pts[10];
    for (int i = 0; i < 10; ++i)
    {
        const float ang = -3.14159265f / 2.0f + i * 3.14159265f / 5.0f;
        const float rad = (i % 2 == 0) ? r : r * 0.45f;
        pts[i] = ImVec2(c.x + cosf(ang) * rad, c.y + sinf(ang) * rad);
    }
    if (pinned)
        dl->AddConvexPolyFilled(pts, 10, P.accent);
    else
        dl->AddPolyline(pts, 10, hov ? P.text : P.textFaint, ImDrawFlags_Closed, 1.4f);
    ImGui::PopID();
    return clicked;
}
} // namespace

int DrawFeatureList(const char* filter, bool favoritesOnly)
{
    namespace w = widgets;
    const std::string needle = Lower(filter ? filter : "");

    std::string lastCat;
    int drawn = 0;
    bool cardOpen = false;
    auto endCard = [&]() { if (cardOpen) { w::EndCard(); cardOpen = false; } };

    for (const auto& t : toggles::All())
    {
        if (favoritesOnly && !config::IsFavorite(t.id))
            continue;
        if (!Matches(t, needle))
            continue;

        if (t.category != lastCat)
        {
            endCard();
            lastCat = t.category;
            w::SectionLabel(t.category);
            cardOpen = w::BeginCard(t.category);
        }

        ImGui::PushID(t.id);
        if (PinStar("pin", config::IsFavorite(t.id)))
            config::ToggleFavorite(t.id);
        ImGui::SameLine(0.0f, 8.0f);
        bool v = t.flag->load();
        if (w::ToggleRow(t.label, &v, t.tooltip, t.experimental))
        {
            t.flag->store(v);
            config::Save();
        }
        ImGui::PopID();
        ++drawn;
    }
    endCard();

    if (drawn == 0)
    {
        const auto& P = theme::Colors();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textFaint),
                           favoritesOnly ? "  No favorites yet - tap the star on any feature to pin it."
                                         : "  No features match your search.");
    }
    return drawn;
}
} // namespace cm::gui
