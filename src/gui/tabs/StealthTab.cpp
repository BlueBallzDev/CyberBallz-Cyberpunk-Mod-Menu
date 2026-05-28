#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Theme.hpp"
#include "gui/Notifications.hpp"
#include "core/State.hpp"

#include <imgui.h>
#include <string>

namespace cm::gui::tabs
{
void Stealth()
{
    namespace w = widgets;
    auto& s = State::Get();
    const auto& P = theme::Colors();

    w::SectionLabel("GHOST OPS");
    if (w::BeginCard("ghost"))
    {
        bool undetect = s.undetectable.load();
        if (w::ToggleRow("Undetectable", &undetect, "Cloak status - enemies lose track of you"))
            s.undetectable = undetect;
        bool camo = s.opticalCamoInfinite.load();
        if (w::ToggleRow("Infinite Optical Camo", &camo, "No camo drain or cooldown"))
            s.opticalCamoInfinite = camo;
        bool alert = s.alertFreeze.load();
        if (w::ToggleRow("Alert Freeze", &alert, "Lock the current alarm state", true))
            s.alertFreeze = alert;
    }
    w::EndCard();

    w::SectionLabel("WANTED LEVEL");
    if (w::BeginCard("heat"))
    {
        bool freeze = s.heatFreeze.load();
        if (w::ToggleRow("Freeze Wanted Level", &freeze, "Lock the police system so heat can't change"))
            s.heatFreeze = freeze;

        if (w::ButtonRow("Clear Wanted Level", "Clear", "Drop all NCPD attention"))
        {
            s.Push({ActionType::ClearHeat});
            notify::Info("Clearing wanted level...");
        }

        int target = s.wantedLevelTarget.load();
        if (w::IntSliderRow("Wanted Level", &target, 0, 5, "Level to hold (0 = never wanted)"))
            s.wantedLevelTarget = target;
        bool hold = s.holdWantedLevel.load();
        if (w::ToggleRow("Hold Wanted Level", &hold,
                         "Continuously keep the wanted level at the value above. At 0 the police never "
                         "escalate - no heat even if you shoot near them."))
            s.holdWantedLevel = hold;
    }
    w::EndCard();

    if (w::BeginCard("note"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(P.textFaint));
        ImGui::TextWrapped("Detection and prevention bindings are validated in-game; experimental controls log "
                           "their attempts to Diagnostics.");
        ImGui::PopStyleColor();
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
