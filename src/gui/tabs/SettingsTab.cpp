#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Theme.hpp"
#include "gui/ThemeIO.hpp"
#include "gui/Background.hpp"
#include "gui/Notifications.hpp"
#include "core/Config.hpp"
#include "core/State.hpp"

#include <imgui_internal.h>

#include <cstring>
#include <vector>

namespace cm::gui::tabs
{
void Settings()
{
    namespace w = widgets;
    auto& prefs = config::Prefs();
    const auto& P = theme::Colors();

    w::SectionLabel("LOADOUT");
    if (w::BeginCard("loadout"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(P.textDim));
        ImGui::TextWrapped("Toggles and sliders do NOT save automatically. Press Save to store your current "
                           "loadout so it restores next launch.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        if (w::AccentButton("Save Loadout", ImVec2(-1.0f, 32.0f)))
        {
            config::SaveLoadout();
            notify::Success("Loadout saved");
        }
    }
    w::EndCard();

    w::SectionLabel("APPEARANCE");
    if (w::BeginCard("appearance"))
    {
        ImGui::TextUnformatted("Accent Color");
        ImGui::Spacing();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float sz = 30.0f, gap = 10.0f;
        // Wrap the swatches into a grid based on the card width.
        const float avail = ImGui::GetContentRegionAvail().x;
        int cols = static_cast<int>((avail + gap) / (sz + gap));
        if (cols < 1)
            cols = 1;
        const int count = theme::AccentCount();
        for (int i = 0; i < count; ++i)
        {
            ImGui::PushID(i);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##acc", ImVec2(sz, sz));
            const bool selected = (prefs.accentPreset == i);
            if (ImGui::IsItemClicked())
            {
                theme::SetAccent(i);
                config::Save();
                notify::Success(std::string("Accent: ") + theme::AccentAt(i).name);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", theme::AccentAt(i).name);
            ImVec2 c(p.x + sz * 0.5f, p.y + sz * 0.5f);
            const float hov = ImGui::IsItemHovered() ? 1.0f : 0.0f;
            dl->AddCircleFilled(c, sz * 0.36f + hov, theme::AccentAt(i).color, 24);
            if (selected)
                dl->AddCircle(c, sz * 0.48f, theme::AccentAt(i).color, 24, 2.0f);
            ImGui::PopID();
            // SameLine unless this is the last in a row or the very last swatch.
            if ((i % cols) != (cols - 1) && i < count - 1)
                ImGui::SameLine(0, gap);
        }
        ImGui::Dummy(ImVec2(0, 8));

        // Edit a pending value live; only apply (restyle + resize) on release so
        // the window doesn't move out from under the cursor while dragging.
        static float pendingScale = prefs.uiScale;
        static bool dragging = false;
        w::SliderRow("UI Scale", &pendingScale, 0.8f, 1.5f, "%.2fx");
        if (ImGui::IsItemActive())
            dragging = true;
        if (dragging && !ImGui::IsItemActive())
        {
            dragging = false;
            theme::SetUiScale(pendingScale);
            config::Save();
            notify::Info("UI scale applied");
        }
    }
    w::EndCard();

    w::SectionLabel("HUD");
    if (w::BeginCard("hud"))
    {
        bool wm = prefs.showWatermark;
        if (w::ToggleRow("Watermark", &wm, "Show the CyberBallz tag on screen"))
        {
            prefs.showWatermark = wm;
            config::Save();
        }
        bool fps = prefs.showFps;
        if (w::ToggleRow("FPS Counter", &fps, "Show framerate in the watermark"))
        {
            prefs.showFps = fps;
            config::Save();
        }
        bool diag = prefs.showDiagnostics;
        if (w::ToggleRow("Diagnostics Panel", &diag,
                         "Show the binding self-test panel on the Dashboard (for troubleshooting)"))
        {
            prefs.showDiagnostics = diag;
            config::Save();
        }
    }
    w::EndCard();

    w::SectionLabel("CONTROLS");
    if (w::BeginCard("controls"))
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddText(p, P.text, "Toggle Menu");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150.0f);
        if (w::KeyCaptureButton("##toggle", &prefs.toggleKey))
        {
            config::Save();
            notify::Info("Toggle key updated");
        }
    }
    w::EndCard();

    w::SectionLabel("CONFIGURATION");
    if (w::BeginCard("config"))
    {
        bool startOpen = prefs.startOpen;
        if (w::ToggleRow("Open on Launch", &startOpen, "Show the menu when the game loads"))
        {
            prefs.startOpen = startOpen;
            config::Save();
        }
        if (w::ButtonRow("Settings File", "Save", "Write the current configuration to disk"))
        {
            config::Save();
            notify::Success("Configuration saved");
        }
    }
    w::EndCard();

    w::SectionLabel("ABOUT");
    if (w::BeginCard("about"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(P.textDim));
        ImGui::TextWrapped("CyberBallz - a sleek mod menu for Cyberpunk 2077. Made by CyberBallz.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textFaint), "Version 1.0  -  config in %%LOCALAPPDATA%%\\CyberBallz");
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
