#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Theme.hpp"
#include "gui/Notifications.hpp"
#include "core/State.hpp"

#include <imgui.h>

namespace cm::gui::tabs
{
void Netrunner()
{
    namespace w = widgets;
    auto& s = State::Get();
    const auto& P = theme::Colors();

    w::SectionLabel("MEMORY (RAM)");
    if (w::BeginCard("ram"))
    {
        bool infRam = s.infiniteMemory.load();
        if (w::ToggleRow("Infinite RAM", &infRam, "Keep the memory pool full"))
            s.infiniteMemory = infRam;
        bool regen = s.ramRegenBoost.load();
        if (w::ToggleRow("Rapid RAM Regen", &regen, "Continuously top off memory"))
            s.ramRegenBoost = regen;

        const auto disp = s.GetDisplay();
        char ram[32];
        snprintf(ram, sizeof(ram), "%.0f / %.0f", disp.ram, disp.ramMax);
        ImGui::Spacing();
        w::StatBar("RAM", disp.ram, disp.ramMax, P.accent, disp.playerValid ? ram : "--");
    }
    w::EndCard();

    w::SectionLabel("QUICKHACKS");
    if (w::BeginCard("qh"))
    {
        bool cost = s.quickhackCostZero.load();
        if (w::ToggleRow("Zero RAM Cost", &cost, "Quickhacks cost no memory"))
            s.quickhackCostZero = cost;
        bool cd = s.quickhackNoCooldown.load();
        if (w::ToggleRow("No Cooldown", &cd, "Cast quickhacks back to back"))
            s.quickhackNoCooldown = cd;
        bool oc = s.overclockNoCooldown.load();
        if (w::ToggleRow("No Overclock Cooldown", &oc, "Cyberdeck overclock recharges instantly"))
            s.overclockNoCooldown = oc;
    }
    w::EndCard();

    if (w::BeginCard("note"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(P.textFaint));
        ImGui::TextWrapped("RAM and quickhack tuning apply real stat modifiers (gameRPGManager + StatsSystem), "
                           "removed cleanly when toggled off.");
        ImGui::PopStyleColor();
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
