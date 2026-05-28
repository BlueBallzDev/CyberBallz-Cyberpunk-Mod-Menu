#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Theme.hpp"
#include "gui/Notifications.hpp"
#include "core/State.hpp"

#include <imgui.h>
#include <cstring>
#include <string>

namespace cm::gui::tabs
{
namespace
{
struct ItemEntry
{
    const char* label; // searchable display name
    const char* id;    // TweakDB record
};
// Curated quick-pick of common, known-good items. The manual ID field below
// still accepts any TweakDB record for anything not listed here.
const ItemEntry kItems[] = {
    {"Eddies (money)", "Items.money"},
    {"Common Components", "Items.CommonMaterial1"},
    {"Uncommon Components", "Items.UncommonMaterial1"},
    {"Rare Components", "Items.RareMaterial1"},
    {"Epic Components", "Items.EpicMaterial1"},
    {"Legendary Components", "Items.LegendaryMaterial1"},
    {"Quickhack Components (Uncommon)", "Items.QuickHackUncommonMaterial1"},
    {"Quickhack Components (Rare)", "Items.QuickHackRareMaterial1"},
    {"Quickhack Components (Epic)", "Items.QuickHackEpicMaterial1"},
    {"Quickhack Components (Legendary)", "Items.QuickHackLegendaryMaterial1"},
    {"Handgun Ammo", "Ammo.HandgunAmmo"},
    {"Rifle Ammo", "Ammo.RifleAmmo"},
    {"Shotgun Ammo", "Ammo.ShotgunAmmo"},
    {"Sniper Ammo", "Ammo.SniperRifleAmmo"},
    {"MaxDOC Inhaler (heal)", "Items.FirstAidWhiffV0"},
    {"Bounce Back (heal)", "Items.BonesMcCoy70V0"},
    {"Bounce Back Mk.3 (heal)", "Items.BonesMcCoy70V2"},
    {"Carry Capacity Shard", "Items.CarryCapacityBackpack"},
};
} // namespace

void Inventory()
{
    auto& s = State::Get();
    namespace w = widgets;
    const auto& P = theme::Colors();

    w::SectionLabel("MONEY");
    if (w::BeginCard("money"))
    {
        if (w::ButtonRow("Add Eddies", "+ \xe2\x82\xac$10k", "Add 10,000 eddies"))
        {
            s.Push({ActionType::AddMoney, 10000.0});
            notify::Success("Queued +10,000 eddies");
        }
        if (w::ButtonRow("Add Eddies", "+ \xe2\x82\xac$100k", "Add 100,000 eddies"))
        {
            s.Push({ActionType::AddMoney, 100000.0});
            notify::Success("Queued +100,000 eddies");
        }
        if (w::ButtonRow("Add Eddies", "+ \xe2\x82\xac$1M", "Add 1,000,000 eddies"))
        {
            s.Push({ActionType::AddMoney, 1000000.0});
            notify::Success("Queued +1,000,000 eddies");
        }
    }
    w::EndCard();

    w::SectionLabel("ITEMS");
    if (w::BeginCard("items"))
    {
        static char itemId[128] = "Items.money";
        static char filter[64] = "";
        static int qty = 1;

        // Searchable quick-pick: type a keyword, click a result to select it.
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##itemsearch", "Search items  (money, ammo, components, heal...)", filter,
                                 sizeof(filter));

        auto lower = [](std::string v) {
            for (auto& c : v) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            return v;
        };
        const std::string needle = lower(filter);

        if (ImGui::BeginChild("##itemlist", ImVec2(0, 132), true))
        {
            for (const auto& it : kItems)
            {
                if (!needle.empty() && lower(it.label).find(needle) == std::string::npos &&
                    lower(it.id).find(needle) == std::string::npos)
                    continue;
                const bool sel = (std::strcmp(itemId, it.id) == 0);
                if (ImGui::Selectable(it.label, sel, 0, ImVec2(0, 22)))
                    strncpy_s(itemId, it.id, _TRUNCATE);
            }
        }
        ImGui::EndChild();

        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(P.textDim), "Selected:  %s", itemId);
        w::InputTextRow("Item ID", itemId, sizeof(itemId), "Or type any TweakDB record id");
        w::IntSliderRow("Quantity", &qty, 1, 10000);
        if (w::ButtonRow("Give Item", "Give", "Add the selected item to your inventory"))
        {
            Action a{ActionType::GiveItem};
            a.text = itemId;
            a.quantity = qty;
            s.Push(a);
            notify::Success("Queued item grant");
        }
    }
    w::EndCard();

    w::SectionLabel("CRAFTING");
    if (w::BeginCard("crafting"))
    {
        if (w::ButtonRow("Crafting Components", "Max Out", "Top up all common-legendary materials"))
        {
            s.Push({ActionType::MaxCraftingComponents});
            notify::Success("Queued crafting materials");
        }
    }
    w::EndCard();

    w::SectionLabel("AMMO");
    if (w::BeginCard("ammo"))
    {
        if (w::ButtonRow("All Ammo", "Max", "Add a large stack of every ammo type"))
        {
            static const char* kAmmo[] = {"Ammo.HandgunAmmo", "Ammo.RifleAmmo", "Ammo.ShotgunAmmo",
                                          "Ammo.SniperRifleAmmo"};
            for (auto* ammo : kAmmo)
            {
                Action a{ActionType::GiveItem};
                a.text = ammo;
                a.quantity = 9999;
                s.Push(a);
            }
            notify::Success("Queued max ammo");
        }
    }
    w::EndCard();

    w::SectionLabel("PROGRESSION");
    if (w::BeginCard("progression"))
    {
        if (w::ButtonRow("Street Cred", "+ XP", "Add street cred experience", true))
        {
            Action a{ActionType::AddStreetCredXP};
            a.quantity = 10000;
            s.Push(a);
            notify::Info("Queued street cred XP");
        }
        if (w::ButtonRow("Character Level", "+ XP", "Add character experience", true))
        {
            Action a{ActionType::AddLevelXP};
            a.quantity = 10000;
            s.Push(a);
            notify::Info("Queued level XP");
        }
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
