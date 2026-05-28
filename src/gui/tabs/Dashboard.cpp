#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Theme.hpp"
#include "gui/Notifications.hpp"
#include "gui/FeatureList.hpp"
#include "core/State.hpp"
#include "core/Config.hpp"

#include <imgui.h>

namespace cm::gui::tabs
{
void Dashboard()
{
    auto& s = State::Get();
    namespace w = widgets;
    const auto& P = theme::Colors();

    // Pinned favorites (only when the user has pinned something)
    if (!config::Prefs().favorites.empty())
    {
        w::SectionLabel("PINNED");
        DrawFeatureList(nullptr, true);
        ImGui::Dummy(ImVec2(0, 6));
    }

    // Install / binding diagnostics - hidden by default, toggled in Settings.
    if (config::Prefs().showDiagnostics)
    {
        const auto diag = s.GetDiagnostics();
        w::SectionLabel("DIAGNOSTICS");
        if (w::BeginCard("diag"))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            auto row = [&](const char* label, bool ok, const char* okText, const char* badText)
            {
                ImVec2 p = ImGui::GetCursorScreenPos();
                const float h = ImGui::GetTextLineHeight();
                dl->AddCircleFilled(ImVec2(p.x + 5.0f, p.y + h * 0.5f), 4.0f, ok ? P.success : P.danger);
                ImGui::Indent(18.0f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.text), "%s", label);
                if (okText)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ok ? P.success : P.danger), "- %s",
                                       ok ? okText : badText);
                }
                ImGui::Unindent(18.0f);
            };

            row("RED4ext plugin loaded", diag.loaded, "yes", "no");
            row("DirectX 12 overlay hook", diag.dxHookOk, "installed", "failed");
            row("Game-thread update", diag.gameStateOk, "registered", "failed");
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textDim), "Game build: %s",
                               diag.gameVersion.empty() ? "unknown" : diag.gameVersion.c_str());

            ImGui::Spacing();
            if (diag.selfTestRan)
            {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.accent), "Game bindings available: %d / %d",
                                   diag.okCount, diag.total);
                for (const auto& c : diag.checks)
                    row(c.name.c_str(), c.ok, nullptr, nullptr);
            }
            else
            {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textDim),
                                   "Load into a save to run the binding self-test.");
            }
        }
        w::EndCard();
    }

    w::SectionLabel("QUICK TOGGLES");
    if (w::BeginCard("quick"))
    {
        bool god = s.godMode.load();
        if (w::ToggleRow("God Mode", &god, "Continuously refill health"))
        {
            s.godMode = god;
            notify::Success(god ? "God Mode enabled" : "God Mode disabled");
            config::Save();
        }
        bool stam = s.infiniteStamina.load();
        if (w::ToggleRow("Infinite Stamina", &stam))
        {
            s.infiniteStamina = stam;
            config::Save();
        }
    }
    w::EndCard();

    w::SectionLabel("QUICK ACTIONS");
    if (w::BeginCard("actions"))
    {
        if (w::ButtonRow("Restore Player", "Heal", "Top up health, stamina and oxygen"))
        {
            s.Push({ActionType::HealPlayer});
            notify::Info("Restoring player...");
        }
        if (w::ButtonRow("Add Eddies", "+ \xe2\x82\xac$100k", "Drop 100,000 eddies into your inventory"))
        {
            s.Push({ActionType::AddMoney, 100000.0});
            notify::Success("Queued +100,000 eddies");
        }
        if (w::ButtonRow("Max All Stats", "Max", "Attributes to 20 + attribute/perk points", true))
        {
            s.Push({ActionType::MaxAllStats});
            notify::Success("Maxing attributes...");
        }
        if (w::ButtonRow("Unlock All Vehicles", "Unlock All", "Unlock every vehicle record"))
        {
            s.Push({ActionType::UnlockAllVehicles});
            notify::Success("Unlocking all vehicles...");
        }
    }
    w::EndCard();

    w::SectionLabel("SESSION");
    if (w::BeginCard("session"))
    {
        const auto d = s.GetDisplay();
        const auto& P = theme::Colors();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(d.gameLoaded ? P.success : P.textDim));
        ImGui::TextUnformatted(d.gameLoaded ? (d.playerValid ? "Connected - player in world"
                                                             : "Connected - no player puppet")
                                            : "Waiting for an active game session...");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        w::StatBar("HEALTH", d.health, d.healthMax, P.success, nullptr);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textDim), "Position  X %.1f  Y %.1f  Z %.1f",
                           d.posX, d.posY, d.posZ);
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
