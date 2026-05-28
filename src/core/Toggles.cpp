#include "Toggles.hpp"
#include "State.hpp"

#include <algorithm>

namespace cm::toggles
{
const std::vector<Toggle>& All()
{
    static std::vector<Toggle> list = [] {
        auto& s = State::Get();
        return std::vector<Toggle>{
            // Player
            {"godMode", "God Mode", "Player", "Health stays full; no damage", &s.godMode, false},
            {"infiniteStamina", "Infinite Stamina", "Player", "Stamina never drains", &s.infiniteStamina, false},
            {"infiniteOxygen", "Infinite Oxygen", "Player", "Never drown", &s.infiniteOxygen, false},
            {"infiniteMemory", "Infinite RAM", "Player", "Quickhack memory stays full", &s.infiniteMemory, false},
            {"noFallDamage", "No Fall Damage", "Player", "Take no damage from falls (fall-damage reduction)", &s.noFallDamage, false},
            {"noRagdoll", "No Ragdoll", "Player", "Don't get knocked down/over by cars or hits", &s.noRagdoll, false},
            {"moveSpeed", "Move Speed", "Player", "Movement speed multiplier", &s.moveSpeedEnabled, false},
            {"carryCapacity", "Carry Capacity", "Player", "Raise carry weight limit", &s.carryCapacityEnabled, false},
            {"superJump", "Super Jump", "Player", "Jump much higher", &s.superJumpEnabled, false},
            {"healthRegen", "Fast Health Regen", "Player", "Rapid in/out-of-combat health regeneration", &s.healthRegenBoost, false},
            {"noClip", "No Clip", "Player", "Free-fly through walls: WASD move, Q/E turn, Space/Ctrl up-down, Shift boost", &s.noClipEnabled, false},
            // Combat
            {"damageMult", "Damage Multiplier", "Combat", "Scale your drawn weapon's damage", &s.damageMultEnabled, false},
            {"oneHitKill", "One-Hit Kill", "Combat", "Massive weapon damage - any hit kills (ranged)", &s.oneHitKill, false},
            {"infiniteAmmo", "Infinite Ammo", "Combat", "No depletion or reload", &s.infiniteAmmoNoReload, false},
            {"noRecoil", "No Recoil", "Combat", "Zero weapon recoil", &s.noRecoil, false},
            {"noSpread", "No Spread", "Combat", "Perfect accuracy (zero spread)", &s.noSpread, false},
            {"sandevistan", "Infinite Sandevistan", "Combat", "No recharge cooldown", &s.sandevistanInfinite, false},
            {"infiniteGrenades", "Infinite Grenades", "Combat", "Grenades recharge instantly", &s.infiniteGrenades, false},
            {"infiniteProjectiles", "Infinite Projectiles", "Combat", "Projectile launcher never depletes", &s.infiniteProjectiles, false},
            {"berserkCd", "No Berserk Cooldown", "Combat", "Berserk recharges instantly", &s.berserkNoCooldown, false},
            {"kerenzikovCd", "No Kerenzikov Cooldown", "Combat", "Kerenzikov recharges instantly", &s.kerenzikovNoCooldown, false},
            {"critChance", "Crit Chance Boost", "Combat", "Adds to your critical-hit chance", &s.critChanceEnabled, false},
            {"critDamage", "Crit Damage Boost", "Combat", "Adds to your critical-hit damage", &s.critDamageEnabled, false},
            {"headshotDmg", "Headshot Damage Boost", "Combat", "Adds to your headshot damage multiplier", &s.headshotDamageEnabled, false},
            {"armorPen", "Armor Penetration", "Combat", "Your weapons ignore enemy armor", &s.armorPenEnabled, false},
            {"techChargeFast", "Instant Tech Charge", "Combat", "Tech weapons charge instantly", &s.techChargeFast, false},
            {"rapidFire", "Rapid Fire", "Combat", "Greatly increase your weapon's fire rate", &s.rapidFireEnabled, false},
            // Player
            {"infiniteHeals", "Infinite Heals", "Player", "Inhalers/health items recharge instantly", &s.infiniteHeals, false},
            // Stealth & Heat
            {"undetectable", "Undetectable", "Stealth", "Cloak - enemies lose track", &s.undetectable, false},
            {"opticalCamo", "Infinite Optical Camo", "Stealth", "No camo drain/cooldown", &s.opticalCamoInfinite, false},
            {"alertFreeze", "Alert Freeze", "Stealth", "Lock the current alert state", &s.alertFreeze, true},
            {"heatFreeze", "Freeze Wanted Level", "Stealth", "Lock the police system so heat can't change", &s.heatFreeze, false},
            {"holdWanted", "Hold Wanted Level", "Stealth", "Keep wanted at the set level; 0 = never wanted", &s.holdWantedLevel, false},
            // Netrunner
            {"quickhackCost", "Zero RAM Cost", "Netrunner", "Quickhacks cost no memory", &s.quickhackCostZero, false},
            {"quickhackCd", "No Quickhack CD", "Netrunner", "No quickhack cooldown", &s.quickhackNoCooldown, false},
            {"ramRegen", "Rapid RAM Regen", "Netrunner", "Memory refills fast", &s.ramRegenBoost, false},
            {"overclockCd", "No Overclock Cooldown", "Netrunner", "Cyberdeck overclock recharges instantly", &s.overclockNoCooldown, false},
            // Vehicles
            {"vehicleGod", "Vehicle God Mode", "Vehicles", "Mounted vehicle is invulnerable", &s.vehicleGodMode, false},
            {"vehicleBoost", "Infinite Boost", "Vehicles", "Unlimited nitro", &s.vehicleBoost, true},
            // World
            {"freezeTime", "Freeze Time", "World", "Pause the world clock", &s.freezeTime, false},
            {"timeScale", "Time Scale", "World", "Slow-mo / fast-forward", &s.timeScaleEnabled, true},
            {"fov", "Override FOV", "World", "Force a custom field of view", &s.fovEnabled, false},
            {"autoSkipCutscenes", "Auto-Skip Cutscenes", "World", "Automatically skip cutscenes as they start", &s.autoSkipCutscenes, false},
            {"photoFreeze", "Photo Freeze", "World", "Freeze time for screenshots", &s.photoFreeze, false},
            {"pedDensity", "Ped Density", "World", "Crowd density multiplier", &s.pedDensityEnabled, true},
            {"trafficDensity", "Traffic Density", "World", "Traffic density multiplier", &s.trafficDensityEnabled, true},
            // Economy
            {"freeShopping", "Free Shopping", "Inventory", "Purchases cost nothing", &s.freeShopping, true},
            {"sellMult", "Sell Multiplier", "Inventory", "Scale vendor payouts", &s.sellMultEnabled, true},
        };
    }();
    return list;
}

const std::vector<FloatVal>& Floats()
{
    static std::vector<FloatVal> list = [] {
        auto& s = State::Get();
        return std::vector<FloatVal>{
            {"moveSpeedMult", &s.moveSpeedMult},       {"carryCapacity", &s.carryCapacity},
            {"superJumpMult", &s.superJumpMult},       {"noClipSpeed", &s.noClipSpeed},
            {"damageMult", &s.damageMult},
            {"critChanceMult", &s.critChanceMult},     {"critDamageMult", &s.critDamageMult},
            {"headshotDamageMult", &s.headshotDamageMult}, {"rapidFireMult", &s.rapidFireMult},
            {"timeScale", &s.timeScale},               {"fov", &s.fov},
            {"pedDensityMult", &s.pedDensityMult},     {"trafficDensityMult", &s.trafficDensityMult},
            {"sellMult", &s.sellMult},
        };
    }();
    return list;
}

const Toggle* Find(const std::string& idOrLabel)
{
    auto lower = [](std::string v) {
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        return v;
    };
    const std::string needle = lower(idOrLabel);
    for (const auto& t : All())
        if (lower(t.id) == needle || lower(t.label) == needle)
            return &t;
    return nullptr;
}
} // namespace cm::toggles
