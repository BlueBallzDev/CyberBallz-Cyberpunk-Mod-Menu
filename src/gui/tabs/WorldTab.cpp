#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Theme.hpp"
#include "gui/Notifications.hpp"
#include "core/State.hpp"
#include "core/Config.hpp"

#include <imgui.h>

#include <cstring>

namespace cm::gui::tabs
{
namespace
{
struct Weather
{
    const char* name;
    const char* id;
};
const Weather kWeather[] = {
    {"Sunny", "24h_weather_sunny"},
    {"Clear Skies", "24h_weather_clear"},
    {"Light Clouds", "24h_weather_light_clouds"},
    {"Cloudy", "24h_weather_cloudy"},
    {"Heavy Clouds", "24h_weather_heavy_clouds"},
    {"Fog", "24h_weather_fog"},
    {"Rain", "24h_weather_rain"},
    {"Toxic Rain", "24h_weather_toxic_rain"},
    {"Pollution", "24h_weather_pollution"},
    {"Sandstorm", "24h_weather_sandstorm"},
};
} // namespace

void World()
{
    auto& s = State::Get();
    namespace w = widgets;

    w::SectionLabel("TIME");
    if (w::BeginCard("time"))
    {
        static float hours = 12.0f;
        w::SliderRow("Time of Day", &hours, 0.0f, 23.99f, "%.1f h");
        if (w::ButtonRow("Set Clock", "Apply", "Jump the in-game clock to the time above"))
        {
            s.Push({ActionType::SetTimeHours, hours});
            notify::Success("Clock updated");
        }
        bool freeze = s.freezeTime.load();
        if (w::ToggleRow("Freeze Time", &freeze, "Pause the world clock"))
        {
            s.freezeTime = freeze;
            config::Save();
        }
        bool ts = s.timeScaleEnabled.load();
        if (w::ToggleRow("Time Scale", &ts, "Slow-mo / fast-forward the world", true))
        {
            s.timeScaleEnabled = ts;
            config::Save();
        }
        float scale = s.timeScale.load();
        if (w::SliderRow("Scale Factor", &scale, 0.1f, 2.0f, "%.2fx"))
        {
            s.timeScale = scale;
            config::Save();
        }
    }
    w::EndCard();

    w::SectionLabel("WEATHER");
    if (w::BeginCard("weather"))
    {
        // CDPR removed the weather-interface accessor on 2.x. Codeware re-adds the
        // SetWeather method but not a way to obtain the interface, and nothing in
        // RTTI hands one back - so weather control cannot be driven on this build.
        // The self-test reports this; surface it honestly instead of faking it.
        const auto diag = s.GetDiagnostics();
        bool weatherOk = false;
        for (const auto& c : diag.checks)
            if (c.name.find("Weather") != std::string::npos) { weatherOk = c.ok; break; }

        if (diag.selfTestRan && !weatherOk)
        {
            const auto& Pw = theme::Colors();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(Pw.danger));
            ImGui::TextWrapped("Weather control isn't available on this game build. CDPR removed the weather-system "
                               "accessor in 2.x, and nothing (including Codeware) exposes a way to reach it, so it "
                               "can't be set without risking instability. This unlocks automatically if a future "
                               "game patch or Codeware update restores the accessor.");
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        const bool enabled = weatherOk || !diag.selfTestRan;
        static int sel = 0;
        ImGui::TextUnformatted("Preset");
        ImGui::Spacing();
        ImGui::BeginDisabled(!enabled);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##weather", kWeather[sel].name))
        {
            for (int i = 0; i < IM_ARRAYSIZE(kWeather); ++i)
            {
                if (ImGui::Selectable(kWeather[i].name, sel == i))
                    sel = i;
            }
            ImGui::EndCombo();
        }
        ImGui::Spacing();
        if (w::AccentButton("Apply Weather", ImVec2(-1.0f, 32.0f)))
        {
            Action a{ActionType::SetWeather};
            a.text = kWeather[sel].id;
            s.Push(a);
            notify::Success(std::string("Weather set to ") + kWeather[sel].name);
        }
        ImGui::EndDisabled();
    }
    w::EndCard();

    w::SectionLabel("CAMERA");
    if (w::BeginCard("camera"))
    {
        bool fov = s.fovEnabled.load();
        if (w::ToggleRow("Override FOV", &fov, "Force a custom field of view", true))
        {
            s.fovEnabled = fov;
            config::Save();
        }
        float f = s.fov.load();
        if (w::SliderRow("Field of View", &f, 60.0f, 120.0f, "%.0f"))
        {
            s.fov = f;
            config::Save();
        }
    }
    w::EndCard();

    w::SectionLabel("PUPPET LAB");
    if (w::BeginCard("puppetlab"))
    {
        ImGui::TextWrapped("Spawn an NPC, vehicle or prop in front of you by TweakDB record (needs Codeware).");
        ImGui::Spacing();

        struct Spawnable { const char* name; const char* id; };
        static const Spawnable kSpawn[] = {
            // Vehicles (confirmed-valid records) - reliable spawn test.
            {"Vehicle - Quadra Type-66", "Vehicle.v_sport2_quadra_type66_player"},
            {"Vehicle - Herrera Outlaw", "Vehicle.v_sport1_herrera_outlaw_player"},
            {"Vehicle - Chevalier Emperor", "Vehicle.v_standard3_chevalier_emperor_player"},
            {"Vehicle - Arch Motorcycle", "Vehicle.v_sportbike2_arch_player"},
            {"Vehicle - Quadra Type-66 (Cortes)", "Vehicle.v_standard2_villefort_cortes_player"},
            // NPCs (record names verified at spawn - watch the log for ok=1).
            {"NPC - Judy", "Character.Judy"},
            {"NPC - Panam", "Character.Panam"},
            {"NPC - Takemura", "Character.Goro_Takemura"},
        };
        static int sel = 0;
        ImGui::TextUnformatted("Preset");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##spawnpreset", kSpawn[sel].name))
        {
            for (int i = 0; i < IM_ARRAYSIZE(kSpawn); ++i)
                if (ImGui::Selectable(kSpawn[i].name, sel == i))
                    sel = i;
            ImGui::EndCombo();
        }
        ImGui::Spacing();

        static char customId[96] = "";
        w::InputTextRow("Custom record", customId, sizeof(customId),
                        "Any TweakDB record, e.g. Character.X or Vehicle.X (overrides preset if set)");
        ImGui::Spacing();

        if (w::AccentButton("Spawn", ImVec2(-1.0f, 32.0f)))
        {
            Action a{ActionType::SpawnNpc};
            a.text = (customId[0] != '\0') ? std::string(customId) : std::string(kSpawn[sel].id);
            s.Push(a);
            notify::Info(std::string("Spawning ") + a.text + "...");
        }
        ImGui::Spacing();
        if (w::ButtonRow("Remove Last Spawn", "Despawn", "Delete the most recently spawned entity"))
        {
            s.Push({ActionType::DespawnLastNpc});
            notify::Info("Removing last spawn...");
        }
        if (w::ButtonRow("Remove All Spawns", "Clear", "Delete every entity you've spawned"))
        {
            s.Push({ActionType::DespawnAllNpcs});
            notify::Info("Clearing all spawns...");
        }
    }
    w::EndCard();

    w::SectionLabel("GAME");
    if (w::BeginCard("game"))
    {
        if (w::ButtonRow("Tutorials", "Disable", "Turn off tutorial popups and hints"))
        {
            s.Push({ActionType::DisableTutorials});
            notify::Success("Tutorials disabled");
        }

        bool autoskip = s.autoSkipCutscenes.load();
        if (w::ToggleRow("Auto-Skip Cutscenes", &autoskip, "Automatically skip cutscenes as they start"))
            s.autoSkipCutscenes = autoskip;
        if (w::ButtonRow("Skip Cutscene", "Skip", "Skip the cutscene playing right now"))
        {
            s.Push({ActionType::SkipCutscene});
            notify::Info("Skipping cutscene...");
        }
    }
    w::EndCard();

    w::SectionLabel("QUEST TOOLKIT");
    if (w::BeginCard("quest"))
    {
        if (w::ButtonRow("Teleport to Objective", "Teleport", "Jump to the tracked quest marker"))
        {
            s.Push({ActionType::TeleportToObjective});
            notify::Info("Teleporting to objective...");
        }

        ImGui::Spacing();
        static char factBuf[96] = "";
        static int factVal = 1;
        w::InputTextRow("Quest Fact", factBuf, sizeof(factBuf), "Fact name the game checks (power tool)");
        w::IntSliderRow("Value", &factVal, 0, 10, "Integer value to set");
        if (w::ButtonRow("Set Fact", "Set", "Write a quest-system fact (advanced)"))
        {
            if (factBuf[0])
            {
                Action a{ActionType::SetQuestFact};
                a.text = factBuf;
                a.quantity = factVal;
                s.Push(a);
                notify::Success("Quest fact set");
            }
            else
            {
                notify::Error("Enter a fact name first");
            }
        }
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
