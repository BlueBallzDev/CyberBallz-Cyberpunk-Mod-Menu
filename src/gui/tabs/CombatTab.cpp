#include "gui/Tabs.hpp"
#include "gui/Widgets.hpp"
#include "gui/Theme.hpp"
#include "gui/Notifications.hpp"
#include "core/State.hpp"

#include <imgui.h>

namespace cm::gui::tabs
{
void Combat()
{
    namespace w = widgets;
    auto& s = State::Get();
    const auto& P = theme::Colors();

    w::SectionLabel("DAMAGE");
    if (w::BeginCard("dmg"))
    {
        bool dmgOut = s.damageMultEnabled.load();
        if (w::ToggleRow("Damage Multiplier", &dmgOut, "Scale your drawn weapon's damage"))
            s.damageMultEnabled = dmgOut;
        if (dmgOut)
        {
            float v = s.damageMult.load();
            if (w::SliderRow("  Multiplier", &v, 1.0f, 25.0f, "%.1fx"))
                s.damageMult = v;
        }

        bool ohk = s.oneHitKill.load();
        if (w::ToggleRow("One-Hit Kill", &ohk, "Massive weapon damage - any hit kills (ranged weapons)"))
            s.oneHitKill = ohk;
    }
    w::EndCard();

    w::SectionLabel("WEAPONS");
    if (w::BeginCard("wep"))
    {
        bool ammo = s.infiniteAmmoNoReload.load();
        if (w::ToggleRow("Infinite Ammo / No Reload", &ammo, "Infinite reserve ammo + instant reload"))
            s.infiniteAmmoNoReload = ammo;
        bool recoil = s.noRecoil.load();
        if (w::ToggleRow("No Recoil", &recoil, "Zero recoil on your drawn weapon"))
            s.noRecoil = recoil;
        bool spread = s.noSpread.load();
        if (w::ToggleRow("No Spread", &spread, "Perfect accuracy - zero spread on your drawn weapon"))
            s.noSpread = spread;
        bool tech = s.techChargeFast.load();
        if (w::ToggleRow("Instant Tech Charge", &tech, "Tech weapons charge instantly"))
            s.techChargeFast = tech;
        bool rapid = s.rapidFireEnabled.load();
        if (w::ToggleRow("Rapid Fire", &rapid, "Greatly increase your weapon's fire rate"))
            s.rapidFireEnabled = rapid;
        if (rapid)
        {
            float v = s.rapidFireMult.load();
            if (w::SliderRow("  Fire Rate", &v, 1.0f, 10.0f, "%.1fx"))
                s.rapidFireMult = v;
        }
    }
    w::EndCard();

    w::SectionLabel("COMBAT BUFFS");
    if (w::BeginCard("buffs"))
    {
        bool cc = s.critChanceEnabled.load();
        if (w::ToggleRow("Crit Chance Boost", &cc, "Add to your critical-hit chance"))
            s.critChanceEnabled = cc;
        if (cc)
        {
            float v = s.critChanceMult.load();
            if (w::SliderRow("  +Crit Chance", &v, 0.0f, 1.0f, "+%.2f"))
                s.critChanceMult = v;
        }
        bool cd = s.critDamageEnabled.load();
        if (w::ToggleRow("Crit Damage Boost", &cd, "Add to your critical-hit damage"))
            s.critDamageEnabled = cd;
        if (cd)
        {
            float v = s.critDamageMult.load();
            if (w::SliderRow("  +Crit Damage", &v, 0.0f, 10.0f, "%.1fx"))
                s.critDamageMult = v;
        }
        bool hs = s.headshotDamageEnabled.load();
        if (w::ToggleRow("Headshot Damage Boost", &hs, "Add to your headshot multiplier"))
            s.headshotDamageEnabled = hs;
        if (hs)
        {
            float v = s.headshotDamageMult.load();
            if (w::SliderRow("  +Headshot", &v, 0.0f, 10.0f, "%.1fx"))
                s.headshotDamageMult = v;
        }
        bool ap = s.armorPenEnabled.load();
        if (w::ToggleRow("Armor Penetration", &ap, "Your weapons ignore enemy armor"))
            s.armorPenEnabled = ap;
    }
    w::EndCard();

    w::SectionLabel("TIME DILATION");
    if (w::BeginCard("dilation"))
    {
        bool sande = s.sandevistanInfinite.load();
        if (w::ToggleRow("Infinite Sandevistan", &sande, "No recharge cooldown"))
            s.sandevistanInfinite = sande;
        bool keren = s.kerenzikovNoCooldown.load();
        if (w::ToggleRow("No Kerenzikov Cooldown", &keren, "Kerenzikov recharges instantly"))
            s.kerenzikovNoCooldown = keren;
        bool berserk = s.berserkNoCooldown.load();
        if (w::ToggleRow("No Berserk Cooldown", &berserk, "Berserk recharges instantly"))
            s.berserkNoCooldown = berserk;
    }
    w::EndCard();

    w::SectionLabel("GADGETS");
    if (w::BeginCard("gadgets"))
    {
        bool gren = s.infiniteGrenades.load();
        if (w::ToggleRow("Infinite Grenades", &gren, "Grenades recharge instantly"))
            s.infiniteGrenades = gren;
        bool proj = s.infiniteProjectiles.load();
        if (w::ToggleRow("Infinite Projectiles", &proj, "Projectile launcher never depletes"))
            s.infiniteProjectiles = proj;
    }
    w::EndCard();

    if (w::BeginCard("note"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(P.textFaint));
        ImGui::TextWrapped("Combat tuning uses weapon/stat bindings that are validated in-game. Items marked "
                           "experimental log their attempts to the Diagnostics panel.");
        ImGui::PopStyleColor();
    }
    w::EndCard();
}
} // namespace cm::gui::tabs
