#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Theme.hpp"
#include "gui/Notifications.hpp"
#include "core/State.hpp"
#include "core/Profiles.hpp"

#include <imgui.h>

#include <cstring>
#include <vector>

namespace cm::gui::tabs
{
void Profiles()
{
    namespace w = widgets;
    const auto& P = theme::Colors();

    static std::vector<std::string> saved;
    static bool scanned = false;
    auto rescan = [&]() { saved = profiles::List(); scanned = true; };
    if (!scanned)
        rescan();

    w::SectionLabel("PERSONAS");
    if (w::BeginCard("personas"))
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textDim), "Instant loadouts");
        ImGui::Spacing();
        const auto& personas = profiles::Personas();
        for (int i = 0; i < static_cast<int>(personas.size()); ++i)
        {
            if (w::ButtonRow(personas[i].name, "Apply", personas[i].description))
            {
                profiles::ApplyPersona(i);
                notify::Success(std::string("Persona: ") + personas[i].name);
            }
        }
    }
    w::EndCard();

    w::SectionLabel("SAVE CURRENT LOADOUT");
    if (w::BeginCard("save"))
    {
        static char nameBuf[64] = "my_loadout";
        w::InputTextRow("Profile Name", nameBuf, sizeof(nameBuf), "Saved to %LOCALAPPDATA%\\CyberBallz\\profiles");
        if (w::ButtonRow("Save Profile", "Save", "Capture every toggle and value"))
        {
            if (profiles::Save(nameBuf))
            {
                notify::Success(std::string("Saved '") + nameBuf + "'");
                rescan();
            }
            else
            {
                notify::Error("Failed to save profile");
            }
        }
        if (w::ButtonRow("Share Profiles", "Open Folder",
                         "Open the profiles folder to copy out or drop in .json files"))
        {
            profiles::OpenFolder();
            notify::Info("Opened profiles folder");
        }
    }
    w::EndCard();

    w::SectionLabel("SAVED PROFILES");
    if (w::BeginCard("saved"))
    {
        if (w::GhostButton("Rescan", ImVec2(80, 0)))
            rescan();
        ImGui::Spacing();

        if (saved.empty())
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textFaint), "No saved profiles yet.");
        }
        for (const auto& name : saved)
        {
            ImGui::PushID(name.c_str());
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddText(p, P.text, name.c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 168.0f);
            if (w::GhostButton("Load", ImVec2(74, 0)))
            {
                if (profiles::Load(name))
                    notify::Success(std::string("Loaded '") + name + "'");
                else
                    notify::Error("Failed to load");
            }
            ImGui::SameLine();
            if (w::GhostButton("Delete", ImVec2(80, 0)))
            {
                profiles::Delete(name);
                notify::Info(std::string("Deleted '") + name + "'");
                rescan();
            }
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0, 4));
        }
    }
    w::EndCard();

    w::SectionLabel("GLOBAL HOTKEYS");
    if (w::BeginCard("hotkeys"))
    {
        const auto& Pc = theme::Colors();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(Pc.textDim));
        ImGui::TextUnformatted("Work with the menu closed:");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        struct HK { const char* key; const char* what; };
        static const HK keys[] = {
            {"F5", "Toggle God Mode"}, {"F6", "Restore Player"}, {"F7", "Toggle Undetectable"},
            {"F8", "Clear Heat"},      {"F9", "Toggle Infinite Ammo"},
        };
        for (const auto& k : keys)
        {
            w::Badge(k.key, Pc.accent);
            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Pc.text), "%s", k.what);
        }
    }
    w::EndCard();

    w::SectionLabel("SAFETY");
    if (w::BeginCard("safety"))
    {
        if (w::ButtonRow("Safe Mode", "All Off", "Turn off every toggle instantly"))
        {
            profiles::ClearAll();
            notify::Info("All toggles off");
        }
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
