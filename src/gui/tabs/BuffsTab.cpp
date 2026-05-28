#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Theme.hpp"
#include "gui/Notifications.hpp"
#include "core/State.hpp"

#include <imgui.h>

#include <cstring>

namespace cm::gui::tabs
{
namespace
{
struct Preset
{
    const char* label;
    const char* effect;
};
// Common StatusEffect records (apply via the confirmed StatusEffectSystem path).
const Preset kPresets[] = {
    {"Berserk", "BaseStatusEffect.Berserk"},
    {"Sandevistan", "BaseStatusEffect.Sandevistan"},
    {"Overclock", "BaseStatusEffect.Overclock"},
    {"Regeneration", "BaseStatusEffect.FastTravelHealthRegen"},
    {"Invulnerable", "BaseStatusEffect.Invulnerable"},
    {"Optical Camo", "BaseStatusEffect.Cloaked"},
};
} // namespace

void Buffs()
{
    namespace w = widgets;
    auto& s = State::Get();
    const auto& P = theme::Colors();

    w::SectionLabel("APPLY STATUS EFFECT");
    if (w::BeginCard("apply"))
    {
        static char buf[160];
        static bool init = false;
        if (!init)
        {
            strncpy_s(buf, s.statusEffectId.c_str(), _TRUNCATE);
            init = true;
        }
        w::InputTextRow("Effect Record", buf, sizeof(buf), "e.g. BaseStatusEffect.Berserk");

        if (w::ButtonRow("Apply to Player", "Apply", "Add this status effect to V"))
        {
            Action a{ActionType::ApplyStatusToPlayer};
            a.text = buf;
            s.Push(a);
            notify::Success("Applying status effect");
        }
        if (w::ButtonRow("Remove from Player", "Remove", "Strip this status effect"))
        {
            Action a{ActionType::RemoveStatusFromPlayer};
            a.text = buf;
            s.Push(a);
            notify::Info("Removing status effect");
        }
        if (w::ButtonRow("Cleanse Debuffs", "Clear", "Remove common negative effects"))
        {
            s.Push({ActionType::ClearAllStatusEffects});
            notify::Info("Cleansing debuffs");
        }
    }
    w::EndCard();

    w::SectionLabel("PRESETS");
    if (w::BeginCard("presets"))
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textDim), "One-click status cocktails");
        ImGui::Spacing();
        int col = 0;
        for (const auto& pset : kPresets)
        {
            if (col > 0)
                ImGui::SameLine();
            if (w::GhostButton(pset.label, ImVec2(120, 30)))
            {
                Action a{ActionType::ApplyStatusToPlayer};
                a.text = pset.effect;
                s.Push(a);
                notify::Success(pset.label);
            }
            if (++col >= 3)
                col = 0;
        }
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
