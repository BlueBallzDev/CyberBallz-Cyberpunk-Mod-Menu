#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Notifications.hpp"
#include "core/State.hpp"
#include "core/Config.hpp"

namespace cm::gui::tabs
{
void Vehicles()
{
    auto& s = State::Get();
    namespace w = widgets;

    w::SectionLabel("ACTIVE VEHICLE");
    if (w::BeginCard("active"))
    {
        if (w::ButtonRow("Summon Vehicle", "Summon", "Call your preferred vehicle to you"))
        {
            s.Push({ActionType::SummonVehicle});
            notify::Info("Summoning vehicle...");
        }
        if (w::ButtonRow("Repair Vehicle", "Repair", "Fully repair the vehicle you are in", true))
        {
            s.Push({ActionType::RepairVehicle});
            notify::Info("Queued vehicle repair");
        }
        bool vg = s.vehicleGodMode.load();
        if (w::ToggleRow("Vehicle God Mode", &vg, "Protect the mounted vehicle from damage", true))
        {
            s.vehicleGodMode = vg;
            config::Save();
        }
    }
    w::EndCard();

    w::SectionLabel("GARAGE");
    if (w::BeginCard("garage"))
    {
        if (w::ButtonRow("Unlock All Vehicles", "Unlock All", "Enumerate every vehicle record and unlock it"))
        {
            s.Push({ActionType::UnlockAllVehicles});
            notify::Success("Unlocking all vehicles...");
        }
        static char record[160] = "Vehicle.v_standard2_archer_quartz_player";
        w::InputTextRow("Vehicle Record", record, sizeof(record), "TweakDB vehicle record id");
        if (w::ButtonRow("Unlock by Record", "Unlock", "Enable the specific vehicle record above"))
        {
            Action a{ActionType::SpawnVehicle};
            a.text = record;
            s.Push(a);
            notify::Success("Vehicle unlocked");
        }
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
