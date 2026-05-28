#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Notifications.hpp"
#include "core/State.hpp"
#include "core/Config.hpp"

namespace cm::gui::tabs
{
void Player()
{
    auto& s = State::Get();
    namespace w = widgets;

    w::SectionLabel("SURVIVAL");
    if (w::BeginCard("survival"))
    {
        bool god = s.godMode.load();
        if (w::ToggleRow("God Mode", &god, "Health is refilled every frame"))
        {
            s.godMode = god;
            config::Save();
        }
        bool stam = s.infiniteStamina.load();
        if (w::ToggleRow("Infinite Stamina", &stam, "Never run out of sprint/dash"))
        {
            s.infiniteStamina = stam;
            config::Save();
        }
        bool oxy = s.infiniteOxygen.load();
        if (w::ToggleRow("Infinite Oxygen", &oxy, "Stay underwater indefinitely"))
        {
            s.infiniteOxygen = oxy;
            config::Save();
        }
        bool mem = s.infiniteMemory.load();
        if (w::ToggleRow("Infinite RAM", &mem, "Never run out of quickhack memory"))
        {
            s.infiniteMemory = mem;
            config::Save();
        }
        bool fall = s.noFallDamage.load();
        if (w::ToggleRow("No Fall Damage", &fall, "Take zero damage from falls (no full invulnerability)"))
        {
            s.noFallDamage = fall;
            config::Save();
        }
        bool rag = s.noRagdoll.load();
        if (w::ToggleRow("No Ragdoll", &rag, "Don't get knocked down/over by cars or hits"))
        {
            s.noRagdoll = rag;
            config::Save();
        }
        bool heals = s.infiniteHeals.load();
        if (w::ToggleRow("Infinite Heals", &heals, "Inhalers/health items recharge instantly"))
        {
            s.infiniteHeals = heals;
            config::Save();
        }
        if (w::ButtonRow("Restore Now", "Heal", "Instantly refill health, stamina and oxygen"))
        {
            s.Push({ActionType::HealPlayer});
            notify::Info("Restoring player...");
        }
    }
    w::EndCard();

    w::SectionLabel("CHARACTER");
    if (w::BeginCard("character"))
    {
        if (w::ButtonRow("Max All Stats", "Max", "Set all attributes to 20 and grant attribute + perk points",
                         true))
        {
            s.Push({ActionType::MaxAllStats});
            notify::Success("Maxing attributes...");
        }
    }
    w::EndCard();

    w::SectionLabel("MOVEMENT");
    if (w::BeginCard("movement"))
    {
        bool ms = s.moveSpeedEnabled.load();
        if (w::ToggleRow("Movement Speed", &ms, "Override base locomotion speed", true))
        {
            s.moveSpeedEnabled = ms;
            config::Save();
        }
        float mult = s.moveSpeedMult.load();
        if (w::SliderRow("Speed Multiplier", &mult, 0.5f, 5.0f, "%.2fx"))
        {
            s.moveSpeedMult = mult;
            config::Save();
        }
        bool cc = s.carryCapacityEnabled.load();
        if (w::ToggleRow("Carry Capacity", &cc, "Raise maximum carry weight", true))
        {
            s.carryCapacityEnabled = cc;
            config::Save();
        }
        float cap = s.carryCapacity.load();
        if (w::SliderRow("Max Weight", &cap, 100.0f, 5000.0f, "%.0f"))
        {
            s.carryCapacity = cap;
            config::Save();
        }

        bool sj = s.superJumpEnabled.load();
        if (w::ToggleRow("Super Jump", &sj, "Jump much higher", true))
        {
            s.superJumpEnabled = sj;
            config::Save();
        }
        float jh = s.superJumpMult.load();
        if (w::SliderRow("Jump Height", &jh, 1.5f, 6.0f, "%.1fx"))
        {
            s.superJumpMult = jh;
            config::Save();
        }

        bool hr = s.healthRegenBoost.load();
        if (w::ToggleRow("Fast Health Regen", &hr, "Rapid in/out-of-combat health regeneration"))
        {
            s.healthRegenBoost = hr;
            config::Save();
        }
    }
    w::EndCard();

    w::SectionLabel("NO CLIP");
    if (w::BeginCard("noclip"))
    {
        bool nc = s.noClipEnabled.load();
        if (w::ToggleRow("No Clip (Free-Fly)", &nc, "Fly through walls. Close the menu, then: WASD move, "
                                                    "Q/E turn, Space up, Ctrl down, Shift boost"))
        {
            s.noClipEnabled = nc;
            config::Save();
        }
        float sp = s.noClipSpeed.load();
        if (w::SliderRow("Fly Speed", &sp, 0.1f, 3.0f, "%.2f"))
        {
            s.noClipSpeed = sp;
            config::Save();
        }
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
