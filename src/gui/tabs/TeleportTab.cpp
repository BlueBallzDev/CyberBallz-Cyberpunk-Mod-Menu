#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Theme.hpp"
#include "gui/Notifications.hpp"
#include "core/State.hpp"

#include <imgui.h>
#include <cstdio>

namespace cm::gui::tabs
{
void Teleport()
{
    auto& s = State::Get();
    namespace w = widgets;

    w::SectionLabel("WAYPOINT");
    if (w::BeginCard("tp_wp"))
    {
        if (w::ButtonRow("Teleport to Waypoint", "Teleport", "Jump to your placed map marker"))
        {
            s.Push({ActionType::TeleportToWaypoint});
            notify::Info("Teleporting to waypoint...");
        }
        if (w::ButtonRow("Teleport to Objective", "Teleport", "Jump to the tracked quest objective"))
        {
            s.Push({ActionType::TeleportToObjective});
            notify::Info("Teleporting to objective...");
        }
    }
    w::EndCard();

    w::SectionLabel("SAVED LOCATIONS");
    if (w::BeginCard("tp_saved"))
    {
        const auto& P = theme::Colors();
        for (int i = 0; i < State::kLocationSlots; ++i)
        {
            ImGui::PushID(i);
            const auto loc = s.GetLocation(i);

            // Line 1: slot name + coordinates (no right-edge button to collide with).
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.text), "Slot %d", i + 1);
            ImGui::SameLine(0.0f, 12.0f);
            if (loc.valid)
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textDim), "X %.0f   Y %.0f   Z %.0f", loc.x,
                                   loc.y, loc.z);
            else
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textFaint), "empty");

            // Line 2: action buttons, all sized to fit inside the card.
            if (loc.valid)
            {
                if (w::AccentButton("Teleport", ImVec2(104, 28)))
                {
                    Action a{ActionType::TeleportToLocation};
                    a.quantity = i;
                    s.Push(a);
                    notify::Info("Teleporting...");
                }
                ImGui::SameLine(0.0f, 8.0f);
                if (w::GhostButton("Update", ImVec2(92, 28)))
                {
                    Action a{ActionType::SaveLocation};
                    a.quantity = i;
                    s.Push(a);
                    notify::Success("Slot updated to current location");
                }
                ImGui::SameLine(0.0f, 8.0f);
                if (w::GhostButton("Clear", ImVec2(80, 28)))
                {
                    s.SetLocation(i, {}); // default = invalid -> empties the slot
                    notify::Info("Slot cleared");
                }
            }
            else
            {
                if (w::AccentButton("Save Here", ImVec2(120, 28)))
                {
                    Action a{ActionType::SaveLocation};
                    a.quantity = i;
                    s.Push(a);
                    notify::Success("Saved current location");
                }
            }

            ImGui::Dummy(ImVec2(0, 10)); // breathing room between slots
            ImGui::PopID();
        }
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
