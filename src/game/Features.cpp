#include "Features.hpp"
#include "GameAPI.hpp"
#include "core/State.hpp"
#include "core/Logger.hpp"
#include "core/Toggles.hpp"
#include "core/DebugConsole.hpp"

#include <RED4ext/CName.hpp>
#include <RED4ext/NativeTypes.hpp>
#include <RED4ext/RTTISystem.hpp>
#include <RED4ext/TweakDB.hpp>
#include <RED4ext/Scripting/Functions.hpp>
#include <RED4ext/Scripting/Natives/entEntityID.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/StatsObjectID.hpp>
#include <RED4ext/Scripting/Natives/Vector4.hpp>
#include <RED4ext/Scripting/Natives/Quaternion.hpp>
#include <RED4ext/Scripting/Natives/Generated/EulerAngles.hpp>
#include <RED4ext/Scripting/Natives/GameTime.hpp>

#include <Windows.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cm::features
{
using Player = RED4ext::Handle<RED4ext::IScriptable>;

namespace
{
// Reflection constants (from the game's generated reflection data)
enum class StatPool : int32_t
{
    Health = 17,
    Memory = 22,
    Oxygen = 25,
    Stamina = 35,
};
enum class StatType : int32_t
{
    CarryCapacity = 400,
    MaxSpeed = 969,
};
enum class StatModType : int32_t
{
    Additive = 0,
    Multiplier = 2,
};
enum class Proficiency : int32_t
{
    Level = 14,
    StreetCred = 17,
};

constexpr const char* kInvulnEffect = "BaseStatusEffect.Invulnerable";

// Resolve a stat-pool's gamedataStatPoolType value by name (cached on first
// use), so a value shift between game patches can't silently break health/
// stamina reads. Falls back to the known constant if reflection misses.
int32_t PoolValue(StatPool pool)
{
    switch (pool)
    {
    case StatPool::Health:  { static int32_t v = static_cast<int32_t>(game::ResolveEnum("gamedataStatPoolType", "Health",  17)); return v; }
    case StatPool::Memory:  { static int32_t v = static_cast<int32_t>(game::ResolveEnum("gamedataStatPoolType", "Memory",  22)); return v; }
    case StatPool::Oxygen:  { static int32_t v = static_cast<int32_t>(game::ResolveEnum("gamedataStatPoolType", "Oxygen",  25)); return v; }
    case StatPool::Stamina: { static int32_t v = static_cast<int32_t>(game::ResolveEnum("gamedataStatPoolType", "Stamina", 35)); return v; }
    }
    return static_cast<int32_t>(pool);
}

// Small RTTI helpers
bool GetEntityID(const Player& obj, RED4ext::ent::EntityID& out)
{
    return game::CallAuto(obj, "GetEntityID", &out);
}

bool GetRecordID(const Player& obj, RED4ext::TweakDBID& out)
{
    return game::CallAuto(obj, "GetRecordID", &out);
}

bool MakeStatsID(const Player& player, RED4ext::game::StatsObjectID& out)
{
    RED4ext::ent::EntityID id;
    if (!GetEntityID(player, id))
        return false;
    out.entityHash = id.hash;
    return true;
}

// Resolve a game system. The reflected "GetXSystem;GameInstance" getters are
// not present as RTTI globals on current game builds, so map each one to its
// concrete + interface RTTI class and pull it straight off the GameInstance.
// Falls back to the legacy getter for anything not in the table.
RED4ext::Handle<RED4ext::IScriptable> Sys(const char* getter)
{
    struct Entry { const char* getter; const char* concrete; const char* iface; };
    static const Entry kSystems[] = {
        {"GetStatPoolsSystem;GameInstance",       "gameStatPoolsSystem",       "gameIStatPoolsSystem"},
        {"GetStatusEffectSystem;GameInstance",    "gameStatusEffectSystem",    "gameIStatusEffectSystem"},
        {"GetTransactionSystem;GameInstance",     "gameTransactionSystem",     "gameITransactionSystem"},
        {"GetTimeSystem;GameInstance",            "gameTimeSystem",            "gameITimeSystem"},
        {"GetVehicleSystem;GameInstance",         "gameVehicleSystem",         "gameIVehicleSystem"},
        {"GetTeleportationFacility;GameInstance", "gameTeleportationFacility", "gameITeleportationFacility"},
        {"GetJournalManager;GameInstance",        "gameJournalManager",        "gameIJournalManager"},
        {"GetMappinSystem;GameInstance",          "gamemappinsMappinSystem",   "gamemappinsIMappinSystem"},
        {"GetQuestsSystem;GameInstance",          "questQuestsSystem",         "questIQuestsSystem"},
        {"GetStatsSystem;GameInstance",           "gameStatsSystem",           "gameIStatsSystem"},
        {"GetWeatherSystem;GameInstance",         "worldWeatherScriptInterface", "worldRuntimeSystemWeather"},
        {"GetPreventionSpawnSystem;GameInstance", "gamePreventionSpawnSystem", "gameIPreventionSpawnSystem"},
        {"GetStimuliSystem;GameInstance",         "gameStimuliSystem",         "gameIStimuliSystem"},
    };
    for (const auto& e : kSystems)
    {
        if (std::strcmp(e.getter, getter) == 0)
        {
            if (auto h = game::GetSystemByClass(e.concrete))
                return h;
            if (e.iface)
                if (auto h = game::GetSystemByClass(e.iface))
                    return h;
            return game::GetGameSystem(getter); // last resort
        }
    }
    return game::GetGameSystem(getter);
}

// Stat pools (health/stamina/oxygen)
void SetStatPoolPct(const Player& obj, StatPool pool, float pct)
{
    auto sys = Sys("GetStatPoolsSystem;GameInstance");
    if (!sys || !obj)
        return;
    RED4ext::game::StatsObjectID id{};
    if (!MakeStatsID(obj, id))
        return;
    auto poolType = PoolValue(pool);
    bool percentage = true;
    bool ok = game::CallAuto(sys, "RequestSettingStatPoolValue", nullptr, id, poolType, pct, obj, percentage);
    static bool logged = false;
    if (!logged)
    {
        logged = true;
        CM_INFO("SetStatPoolPct: RequestSettingStatPoolValue -> %d (pool=%d pct=%.0f)", ok ? 1 : 0, poolType, pct);
    }
}

float GetStatPoolPct(const Player& obj, StatPool pool)
{
    auto sys = Sys("GetStatPoolsSystem;GameInstance");
    if (!sys || !obj)
        return 0.0f;
    RED4ext::game::StatsObjectID id{};
    if (!MakeStatsID(obj, id))
        return 0.0f;
    auto poolType = PoolValue(pool);
    bool approximate = false;
    float value = 0.0f;
    game::CallAuto(sys, "GetStatPoolValue", &value, id, poolType, approximate);
    return value;
}

// Player's max value for a stat (e.g. max health) from the StatsSystem.
float GetStatValue(const Player& obj, const char* statTypeName)
{
    auto ss = Sys("GetStatsSystem;GameInstance");
    if (!ss || !obj)
        return 0.0f;
    RED4ext::game::StatsObjectID id{};
    if (!MakeStatsID(obj, id))
        return 0.0f;
    const int32_t statType = static_cast<int32_t>(game::ResolveEnum("gamedataStatType", statTypeName, -1));
    if (statType < 0)
        return 0.0f;
    float value = 0.0f;
    game::CallAuto(ss, "GetStatValue", &value, id, statType);
    return value;
}

// Status effects (god mode = Invulnerable)
bool HasStatus(const Player& player, const char* effect)
{
    auto sys = Sys("GetStatusEffectSystem;GameInstance");
    RED4ext::ent::EntityID eid;
    if (!sys || !GetEntityID(player, eid))
        return false;
    RED4ext::TweakDBID id(effect);
    bool out = false;
    game::CallAuto(sys, "HasStatusEffect", &out, eid, id);
    return out;
}

void ApplyStatus(const Player& player, const char* effect)
{
    auto sys = Sys("GetStatusEffectSystem;GameInstance");
    RED4ext::ent::EntityID eid;
    if (!sys || !GetEntityID(player, eid))
        return;
    RED4ext::TweakDBID effectId(effect);
    RED4ext::TweakDBID source;
    GetRecordID(player, source);
    // ApplyStatusEffect(target, effectID, sourceName, instigator)
    bool ok = game::CallAuto(sys, "ApplyStatusEffect", nullptr, eid, effectId, source, eid);
    static int s_logN = 0;
    if (s_logN < 6)
    {
        ++s_logN;
        CM_INFO("ApplyStatusEffect: '%s' -> %d", effect, ok ? 1 : 0);
    }
}

void RemoveStatus(const Player& player, const char* effect)
{
    auto sys = Sys("GetStatusEffectSystem;GameInstance");
    RED4ext::ent::EntityID eid;
    if (!sys || !GetEntityID(player, eid))
        return;
    RED4ext::TweakDBID effectId(effect);
    game::CallAuto(sys, "RemoveStatusEffect", nullptr, eid, effectId);
}

// Inventory
void GiveItem(const std::string& tdbid, int quantity)
{
    auto ts = Sys("GetTransactionSystem;GameInstance");
    auto player = game::GetPlayer();
    if (!ts || !player)
        return;
    // Use GiveItemByTDBID: it builds a valid ItemID internally. A hand-built
    // ItemID (only tdbid set) was rejected by GiveItem (returned 0).
    RED4ext::TweakDBID id(tdbid.c_str());
    int32_t qty = quantity;
    bool ok = game::CallAuto(ts, "GiveItemByTDBID", nullptr, player, id, qty);
    CM_INFO("GiveItem(ByTDBID): '%s' x%d -> %d", tdbid.c_str(), quantity, ok ? 1 : 0);
}

// Time
uint32_t GetGameSeconds()
{
    RED4ext::GameTime gt;
    RED4ext::ExecuteFunction("gameTimeSystem", "GetGameTime", &gt);
    return gt.ToSeconds();
}

void SetGameSeconds(uint32_t seconds)
{
    RED4ext::ExecuteFunction("gameTimeSystem", "SetGameTimeBySeconds", nullptr, seconds);
}

// Weather
// CDPR stripped the reflected GetWeatherSystem getter on 2.x, and Codeware only
// re-adds the *methods* (SetWeather/ResetWeather/GetWeatherState) onto the
// worldWeatherScriptInterface class - not a way to obtain an instance. So we
// locate whatever RTTI function still hands back that interface and call it.
RED4ext::Handle<RED4ext::IScriptable> GetWeatherInterface()
{
    // 1) Legacy/global getter, if the build still exposes one.
    if (auto ws = Sys("GetWeatherSystem;GameInstance"))
        return ws;
    // 2) Any global function whose return type is the weather script interface.
    if (auto ws = game::CallGlobalReturning("worldWeatherScriptInterface"))
        return ws;
    return {};
}

void SetWeather(const std::string& state)
{
    RED4ext::CName id(state.c_str());
    float transition = 3.0f;
    uint32_t priority = 10; // high enough to override the natural cycle (Codeware)

    // The weather control surface needs a live, engine-bound weather interface.
    // GetWeatherInterface searches every getter (globals + class statics) for one
    // that hands back a worldWeatherScriptInterface. On 2.3.1 none exists - CDPR
    // stripped the accessor and Codeware only re-adds the methods - so this stays
    // a clean no-op rather than faking success. If a future Codeware/patch exposes
    // an accessor, this path lights up automatically.
    auto ws = GetWeatherInterface();
    if (!ws)
    {
        static bool s_dumped = false;
        if (!s_dumped)
        {
            s_dumped = true;
            CM_WARN("SetWeather: no weather interface accessor on this build (CDPR removed it); weather control disabled");
            game::LogFunctionsReturning("Weather");
        }
        CM_INFO("SetWeather: '%s' -> unavailable (no interface accessor)", state.c_str());
        return;
    }
    bool ok = game::CallAuto(ws, "SetWeather", nullptr, id, transition, priority);
    CM_INFO("SetWeather: '%s' -> %d (bound iface)", state.c_str(), ok ? 1 : 0);
}

// Vehicles
void EnableVehicle(const std::string& record, bool enable)
{
    auto vs = Sys("GetVehicleSystem;GameInstance");
    if (!vs)
        return;
    RED4ext::TweakDBID rec(record.c_str());
    bool en = enable;
    bool dis = !enable;
    game::CallAuto(vs, "EnablePlayerVehicle", nullptr, rec, en, dis);
}

void UnlockAllVehicles()
{
    auto vs = Sys("GetVehicleSystem;GameInstance");
    if (!vs)
        return;

    // Enumerate every vehicle record from TweakDB and enable it -> true unlock.
    auto rtti = RED4ext::CRTTISystem::Get();
    auto* tdb = RED4ext::TweakDB::Get();
    RED4ext::CClass* recCls = nullptr;
    if (rtti)
    {
        recCls = rtti->GetClass("Vehicle_Record");
        if (!recCls)
            recCls = rtti->GetClass("gamedataVehicle_Record");
    }

    int count = 0;
    if (tdb && recCls)
    {
        RED4ext::DynArray<RED4ext::Handle<RED4ext::IScriptable>> records;
        if (tdb->TryGetRecordsByType(recCls, records))
        {
            for (uint32_t i = 0; i < records.Size(); ++i)
            {
                RED4ext::TweakDBID id;
                if (game::CallAuto(records[i], "GetID", &id))
                {
                    bool en = true, dis = false;
                    game::CallAuto(vs, "EnablePlayerVehicle", nullptr, id, en, dis);
                    ++count;
                }
            }
        }
    }

    if (count == 0)
    {
        // Fallback: a few known base-game player vehicles.
        static const char* kVehicles[] = {
            "Vehicle.v_standard2_archer_quartz_player", "Vehicle.v_standard2_villefort_cortes_player",
            "Vehicle.v_sport2_quadra_type66_player",    "Vehicle.v_sport1_herrera_outlaw_player",
            "Vehicle.v_standard3_chevalier_emperor_player", "Vehicle.v_sportbike2_arch_player",
        };
        for (auto* rec : kVehicles)
            EnableVehicle(rec, true);
        count = static_cast<int>(sizeof(kVehicles) / sizeof(kVehicles[0]));
    }
    CM_INFO("UnlockAllVehicles: processed %d vehicle records", count);
}

// gameRPGManager::CreateStatModifier is a static; the stat type is resolved by
// name so the numeric ids can shift between game patches. One path backs the
// whole cooldown / netrunner / optical-camo / Sandevistan family.
RED4ext::Handle<RED4ext::IScriptable> CreateStatModByName(const char* statName, StatModType modType, float value)
{
    RED4ext::Handle<RED4ext::IScriptable> mod;
    auto rtti = RED4ext::CRTTISystem::Get();
    auto cls = rtti ? rtti->GetClass("gameRPGManager") : nullptr;
    auto fn = game::GetStaticFunction("gameRPGManager", "CreateStatModifier");
    if (!cls || !fn)
        return mod;
    int32_t st = static_cast<int32_t>(game::ResolveEnum("gamedataStatType", statName, -1));
    if (st < 0)
        return mod;
    int32_t mt = static_cast<int32_t>(modType);
    float v = value;
    RED4ext::ExecuteFunction(cls, fn, &mod, st, mt, v);
    return mod;
}

void ApplyStatMod(const RED4ext::Handle<RED4ext::IScriptable>& mod, bool add)
{
    if (!mod)
        return;
    auto ss = Sys("GetStatsSystem;GameInstance");
    auto player = game::GetPlayer();
    RED4ext::ent::EntityID eid;
    if (!ss || !player || !GetEntityID(player, eid))
        return;
    game::CallAuto(ss, add ? "AddModifier" : "RemoveModifier", nullptr, eid, mod);
}

// Apply/remove a player stat modifier on toggle transitions (held in static
// state at each call site). Returns nothing; logs creation failures once.
// Player stat-modifier registry keyed by stat name. Handles apply/remove, live
// value changes, and reapplication after a save reload (the player entity
// changes -> g_playerGen bumps -> mods are recreated for the new player).
struct ManagedStatMod
{
    RED4ext::Handle<RED4ext::IScriptable> handle;
    float value = 0.0f;
    bool applied = false;
    int gen = -1;
};
std::unordered_map<std::string, ManagedStatMod> g_statMods;
int g_playerGen = 0; // bumped when the player entity changes (load/respawn)

void SyncPlayerStatMod(bool want, const char* statName, StatModType type, float value)
{
    auto& e = g_statMods[statName];
    const bool valueChanged = e.applied && e.value != value;
    const bool reloaded = e.applied && e.gen != g_playerGen;
    if (e.applied && (!want || valueChanged || reloaded))
    {
        if (!reloaded) // after a reload the game already dropped the modifier
            ApplyStatMod(e.handle, false);
        e.handle = {};
        e.applied = false;
    }
    if (want && !e.applied)
    {
        e.handle = CreateStatModByName(statName, type, value);
        if (e.handle)
        {
            ApplyStatMod(e.handle, true);
            e.applied = true;
            e.value = value;
            e.gen = g_playerGen;
        }
    }
}

// Apply/remove a stat modifier on a specific StatsObjectID (e.g. the equipped
// weapon's item stats). Weapon stats (damage, magazine, recoil, reload) live on
// the item-data stats object - NOT the weapon entity - so modifiers must target
// that id or they have no effect.
void ApplyStatModToStats(const RED4ext::Handle<RED4ext::IScriptable>& mod, const RED4ext::game::StatsObjectID& sid,
                         bool add)
{
    if (!mod)
        return;
    auto ss = Sys("GetStatsSystem;GameInstance");
    if (!ss)
        return;
    game::CallAuto(ss, add ? "AddModifier" : "RemoveModifier", nullptr, sid, mod);
}

// The player's currently-drawn weapon. GetActiveWeapon is registered as a GLOBAL
// taking the GameObject as its first arg (not an instance method), so resolve it
// as a static and pass the player as `self`.
RED4ext::Handle<RED4ext::IScriptable> GetActiveWeaponObj(const Player& player)
{
    RED4ext::Handle<RED4ext::IScriptable> wpn;
    if (!player || !player.instance)
        return wpn;
    static RED4ext::CBaseFunction* s_fn = nullptr;
    static bool s_looked = false;
    if (!s_looked)
    {
        s_looked = true;
        s_fn = game::GetStaticFunction("gameObject", "GetActiveWeapon");
    }
    if (!s_fn)
        return wpn;
    RED4ext::StackArgs_t args;
    args.emplace_back(nullptr, const_cast<Player*>(&player));
    RED4ext::ExecuteFunction(static_cast<void*>(nullptr), s_fn, &wpn, args);
    return wpn;
}

// The active weapon's item-data StatsObjectID (where damage/magazine/recoil/reload
// stats actually live). GetItemData returns a WEAK ref - capture it in a WeakHandle
// (raw, never strong-held) and read GetStatsObjectID off it, so we don't corrupt
// the refcount (that was the old crash). Returns false if no weapon/data.
bool GetWeaponStatsId(const Player& weapon, RED4ext::game::StatsObjectID& out)
{
    if (!weapon || !weapon.instance)
        return false;
    auto fnData = game::GetMethodFromClass(weapon.instance->GetType(), "GetItemData");
    if (!fnData)
        return false;
    RED4ext::WeakHandle<RED4ext::IScriptable> itemData;
    RED4ext::ExecuteFunction(weapon.instance, fnData, &itemData);
    auto* idInst = itemData.instance; // borrowed; used immediately, not retained
    if (!idInst)
        return false;
    auto fnSid = game::GetMethodFromClass(idInst->GetType(), "GetStatsObjectID");
    if (!fnSid)
        return false;
    return RED4ext::ExecuteFunction(idInst, fnSid, &out);
}

// Per-weapon stat modifiers (damage / magazine / recoil / spread / reload / charge).
// Applied to the drawn weapon's item-data stats object, reapplied when the weapon
// or value changes, removed when the toggle goes off. Keyed by stat name.
struct WeaponStatMod
{
    RED4ext::Handle<RED4ext::IScriptable> handle;
    RED4ext::game::StatsObjectID sid{};
    uint64_t hash = 0;
    float value = 0.0f;
    bool applied = false;
};
std::unordered_map<std::string, WeaponStatMod> g_weaponMods;

void SyncWeaponStatMod(bool want, const Player& player, const char* statName, StatModType type, float value)
{
    auto& e = g_weaponMods[statName];
    if (!want)
    {
        if (e.applied)
        {
            ApplyStatModToStats(e.handle, e.sid, false);
            e = {};
        }
        return;
    }
    auto wpn = GetActiveWeaponObj(player);
    RED4ext::game::StatsObjectID sid{};
    if (!wpn || !GetWeaponStatsId(wpn, sid) || sid.entityHash == 0)
        return; // no weapon drawn / no item stats
    if (e.applied && e.hash == sid.entityHash && e.value == value)
        return; // already applied to this weapon at this value
    if (e.applied) // weapon changed or value changed: drop the previous modifier
        ApplyStatModToStats(e.handle, e.sid, false);
    e.handle = CreateStatModByName(statName, type, value);
    if (e.handle)
    {
        ApplyStatModToStats(e.handle, sid, true);
        e.applied = true;
        e.sid = sid;
        e.hash = sid.entityHash;
        e.value = value;
        CM_INFO("WeaponStatMod '%s' (x%.2f) -> stats hash=%llu", statName, value,
                static_cast<unsigned long long>(sid.entityHash));
    }
}

// Character development (XP / attributes)
RED4ext::Handle<RED4ext::IScriptable> GetPlayerDevData()
{
    RED4ext::Handle<RED4ext::IScriptable> data;
    auto player = game::GetPlayer();
    auto rtti = RED4ext::CRTTISystem::Get();
    auto cls = rtti ? rtti->GetClass("PlayerDevelopmentSystem") : nullptr;
    auto getData = game::GetStaticFunction("PlayerDevelopmentSystem", "GetData");
    if (!player || !cls || !getData)
        return data;
    RED4ext::ExecuteFunction(cls, getData, &data, player);
    return data;
}

void AddExperience(Proficiency type, int amount)
{
    auto data = GetPlayerDevData();
    if (!data)
        return;
    int32_t amt = amount;
    auto prof = static_cast<int32_t>(type);
    int32_t reason = 0; // telemetryLevelGainReason::Gameplay
    bool telemetry = false;
    game::CallAuto(data, "AddExperience", nullptr, amt, prof, reason, telemetry);
}

void MaxAllStats()
{
    auto data = GetPlayerDevData();
    if (!data)
        return;

    // Core attributes -> 20 (max). Stat-type IDs resolved by name at runtime so
    // they survive game patches.
    static const char* kAttributes[] = {"Strength", "Reflexes", "TechnicalAbility", "Intelligence", "Cool"};
    for (auto* attr : kAttributes)
    {
        const int64_t statType = game::ResolveEnum("gamedataStatType", attr, -1);
        if (statType < 0)
            continue;
        auto st = static_cast<int32_t>(statType);
        int32_t value = 20;
        game::CallAuto(data, "SetAttribute", nullptr, st, value);
    }

    // Grant development points (attributes + perks).
    const auto attrPoint = static_cast<int32_t>(game::ResolveEnum("gamedataDevelopmentPointType", "Attribute", 0));
    const auto perkPoint = static_cast<int32_t>(game::ResolveEnum("gamedataDevelopmentPointType", "Primary", 2));
    int32_t points = 100;
    game::CallAuto(data, "AddDevelopmentPoints", nullptr, points, attrPoint);
    game::CallAuto(data, "AddDevelopmentPoints", nullptr, points, perkPoint);

    // Top off survival pools.
    auto player = game::GetPlayer();
    SetStatPoolPct(player, StatPool::Health, 100.0f);
    SetStatPoolPct(player, StatPool::Stamina, 100.0f);
    SetStatPoolPct(player, StatPool::Oxygen, 100.0f);
}

// Teleport to tracked waypoint
void TeleportToWaypoint()
{
    auto player = game::GetPlayer();
    auto ms = Sys("GetMappinSystem;GameInstance");
    auto tp = Sys("GetTeleportationFacility;GameInstance");
    if (!player || !ms || !tp)
        return;

    // Position from the manually-tracked map pin. Oversize the id buffer so an
    // unknown id struct layout can't overflow the stack.
    struct MappinIdBuf { uint64_t a = 0, b = 0; } mappinId{};
    if (!game::CallAuto(ms, "GetManuallyTrackedMappinID", &mappinId))
        return;
    RED4ext::Handle<RED4ext::IScriptable> mappin;
    game::CallAuto(ms, "GetMappin", &mappin, mappinId);
    RED4ext::Vector4 pos{};
    if (!mappin || !game::CallAuto(mappin, "GetWorldPosition", &pos))
        return;

    // Keep the player's current facing (Yaw is a plain float -> safe to read).
    RED4ext::EulerAngles rot{};
    float yaw = 0.0f;
    game::CallAuto(player, "GetWorldYaw", &yaw);
    rot.Yaw = yaw;
    game::CallAuto(tp, "Teleport", nullptr, player, pos, rot);
}

void TeleportPlayerTo(float x, float y, float z)
{
    auto player = game::GetPlayer();
    auto tp = Sys("GetTeleportationFacility;GameInstance");
    if (!player || !tp)
        return;
    RED4ext::Vector4 pos(x, y, z, 1.0f);
    RED4ext::EulerAngles rot{};
    float yaw = 0.0f;
    game::CallAuto(player, "GetWorldYaw", &yaw);
    rot.Yaw = yaw;
    game::CallAuto(tp, "Teleport", nullptr, player, pos, rot);
}

// Teleport to the currently tracked quest objective's map pin.
void TeleportToObjective()
{
    auto player = game::GetPlayer();
    auto journal = Sys("GetJournalManager;GameInstance");
    auto ms = Sys("GetMappinSystem;GameInstance");
    if (!player || !journal || !ms)
        return;

    RED4ext::Handle<RED4ext::IScriptable> tracked;
    if (!game::CallAuto(journal, "GetTrackedEntry", &tracked) || !tracked)
        return;
    uint32_t hash = 0;
    game::CallAuto(journal, "GetEntryHash", &hash, tracked);
    if (hash == 0)
        return;

    // GetQuestMappinPositionsByObjective(objectiveHash, out array<Vector4>) -> Bool
    RED4ext::DynArray<RED4ext::Vector4> positions;
    bool ok = false;
    game::CallAuto(ms, "GetQuestMappinPositionsByObjective", &ok, hash, positions);
    if (positions.Size() == 0)
        return;
    const RED4ext::Vector4& p = positions[0];
    TeleportPlayerTo(p.X, p.Y, p.Z);
}

// Puppet Lab: dynamic spawning (Codeware DynamicEntitySystem)
// Spawns NPCs/vehicles/props by TweakDB record in front of the player. The
// spec object is constructed via RTTI and wrapped in a Handle (the game's
// Handle ctor sets up refcounting, so this is safe - unlike a weak GetItemData
// handle). Spawned entity IDs are tracked for despawn.
std::vector<RED4ext::ent::EntityID> g_spawnedEntities;

RED4ext::Handle<RED4ext::IScriptable> GetDynamicEntitySystem()
{
    if (auto h = game::GetSystemByClass("DynamicEntitySystem"))
        return h;
    // Fallback: Codeware adds GetDynamicEntitySystem as a GameInstance static;
    // resolve it by its return type.
    return game::CallGlobalReturning("DynamicEntitySystem");
}

void SpawnEntityRecord(const std::string& record)
{
    auto sys = GetDynamicEntitySystem();
    auto player = game::GetPlayer();
    if (!sys)
    {
        CM_WARN("Spawn: DynamicEntitySystem unavailable (Codeware required)");
        return;
    }
    if (!player)
        return;
    auto rtti = RED4ext::CRTTISystem::Get();
    auto specCls = rtti ? rtti->GetClass("DynamicEntitySpec") : nullptr;
    if (!specCls)
    {
        CM_WARN("Spawn: DynamicEntitySpec class missing");
        return;
    }
    auto* raw = reinterpret_cast<RED4ext::IScriptable*>(specCls->CreateInstance(true));
    if (!raw)
        return;
    RED4ext::Handle<RED4ext::IScriptable> spec(raw);

    // Position ~2.5 m in front of the player; orientation faces the player's yaw.
    RED4ext::Vector4 pos{};
    game::CallAuto(player, "GetWorldPosition", &pos);
    float yaw = 0.0f;
    game::CallAuto(player, "GetWorldYaw", &yaw);
    const float yawRad = yaw * 0.01745329252f; // deg -> rad
    const float fx = -std::sin(yawRad);         // CP2077: yaw 0 faces +Y
    const float fy = std::cos(yawRad);
    pos.X += fx * 2.5f;
    pos.Y += fy * 2.5f;
    RED4ext::Quaternion orient(0.0f, 0.0f, std::sin(yawRad * 0.5f), std::cos(yawRad * 0.5f));

    RED4ext::TweakDBID rec(record.c_str());

    // Validate the record actually exists in TweakDB, so a bad/guessed name fails
    // loudly (logged + skipped) instead of silently spawning nothing.
    if (auto* tdb = RED4ext::TweakDB::Get())
    {
        RED4ext::Handle<RED4ext::IScriptable> recH;
        const bool exists = tdb->TryGetRecord(rec, recH) && static_cast<bool>(recH);
        CM_INFO("Spawn: record '%s' valid=%d", record.c_str(), exists ? 1 : 0);
        if (!exists)
        {
            CM_WARN("Spawn: '%s' is not a TweakDB record on this build - skipping", record.c_str());
            return;
        }
    }

    game::SetProperty("DynamicEntitySpec", "recordID", spec.instance, rec);
    game::SetProperty("DynamicEntitySpec", "position", spec.instance, pos);
    game::SetProperty("DynamicEntitySpec", "orientation", spec.instance, orient);
    bool yes = true;
    game::SetProperty("DynamicEntitySpec", "spawnInView", spec.instance, yes);
    game::SetProperty("DynamicEntitySpec", "active", spec.instance, yes);

    RED4ext::ent::EntityID id{};
    bool ok = game::CallAuto(sys, "CreateEntity", &id, spec);
    CM_INFO("Spawn: '%s' -> ok=%d id=%llu", record.c_str(), ok ? 1 : 0,
            static_cast<unsigned long long>(id.hash));
    if (ok && id.hash != 0)
        g_spawnedEntities.push_back(id);
}

void DespawnLastEntity()
{
    auto sys = GetDynamicEntitySystem();
    if (!sys || g_spawnedEntities.empty())
        return;
    auto id = g_spawnedEntities.back();
    game::CallAuto(sys, "DeleteEntity", nullptr, id);
    g_spawnedEntities.pop_back();
    CM_INFO("Despawn last -> %llu", static_cast<unsigned long long>(id.hash));
}

void DespawnAllEntities()
{
    auto sys = GetDynamicEntitySystem();
    const int n = static_cast<int>(g_spawnedEntities.size());
    if (sys)
        for (auto& id : g_spawnedEntities)
            game::CallAuto(sys, "DeleteEntity", nullptr, id);
    g_spawnedEntities.clear();
    CM_INFO("Despawn all -> %d entities", n);
}

// Set any quest-system fact by name (the version-resilient way the game itself
// gates content). Used for tutorials and the Quest toolkit's fact tool.
void SetFact(const std::string& fact, int value)
{
    auto qs = Sys("GetQuestsSystem;GameInstance");
    if (!qs)
        return;
    RED4ext::CString f(fact.c_str());
    int32_t v = value;
    bool ok = game::CallAuto(qs, "SetFactStr", nullptr, f, v);
    CM_INFO("SetFact: '%s'=%d -> %d", fact.c_str(), value, ok ? 1 : 0);
}

// Turn off tutorial popups via the quests-system fact the game itself checks.
void DisableTutorials()
{
    SetFact("disable_tutorials", 1);
}

// Skip the currently playing cutscene/scene. The scene-system skip method isn't
// in our reference dump, so resolve the scene system, dump its methods once, and
// call whichever skip-like method exists (no-arg, safe). Returns true if a skip
// method was invoked.
bool SkipCutscene()
{
    auto sys = game::GetSystemByClass("scnSceneSystem");
    if (!sys)
        sys = game::GetSystemByClass("gameSceneSystem");
    if (!sys || !sys.instance || !sys.instance->GetType())
    {
        CM_WARN("SkipCutscene: scene system unavailable");
        return false;
    }
    auto* cls = sys.instance->GetType();
    static bool s_dumped = false;
    if (!s_dumped)
    {
        s_dumped = true;
        game::LogClassFunctions(cls->GetName().ToString(), "Skip");
        game::LogClassFunctions(cls->GetName().ToString(), "Scene");
    }
    static const char* kCandidates[] = {"RequestSkip", "Skip",       "SkipScene",
                                         "KillScene",   "StopScene",  "StopActiveScenes",
                                         "FastForward", "SkipActiveScenes"};
    for (auto* c : kCandidates)
        if (game::GetMethodFromClass(cls, c))
        {
            bool ok = game::CallAuto(sys, c, nullptr);
            CM_INFO("SkipCutscene: %s -> %d", c, ok ? 1 : 0);
            return true;
        }
    CM_INFO("SkipCutscene: no known skip method on %s (see dump)", cls->GetName().ToString());
    return false;
}

// Apply / remove a status effect on the player by TweakDB record id (Buff Lab).
void ApplyStatusToPlayer(const std::string& effectId)
{
    auto player = game::GetPlayer();
    if (!player)
        return;
    ApplyStatus(player, effectId.c_str());
    CM_INFO("ApplyStatusToPlayer: %s", effectId.c_str());
}

void RemoveStatusFromPlayer(const std::string& effectId)
{
    auto player = game::GetPlayer();
    if (!player)
        return;
    RemoveStatus(player, effectId.c_str());
    CM_INFO("RemoveStatusFromPlayer: %s", effectId.c_str());
}

// Remove a curated set of common negative effects (best-effort "cleanse").
void ClearCommonDebuffs()
{
    auto player = game::GetPlayer();
    if (!player)
        return;
    static const char* kDebuffs[] = {
        "BaseStatusEffect.Bleeding",  "BaseStatusEffect.Burning",   "BaseStatusEffect.Poisoned",
        "BaseStatusEffect.Electrocuted", "BaseStatusEffect.Blind",  "BaseStatusEffect.Stunned",
        "BaseStatusEffect.Defeated",  "BaseStatusEffect.HeavyDamage",
    };
    for (auto* d : kDebuffs)
        RemoveStatus(player, d);
    CM_INFO("ClearCommonDebuffs: cleared %d effects", static_cast<int>(sizeof(kDebuffs) / sizeof(kDebuffs[0])));
}

// The player's PreventionSystem (wanted/police), reached off the player.
RED4ext::Handle<RED4ext::IScriptable> GetPreventionSystem()
{
    RED4ext::Handle<RED4ext::IScriptable> ps;
    auto player = game::GetPlayer();
    if (player)
        game::CallAuto(player, "GetPreventionSystem", &ps);
    return ps;
}

// Set the wanted level (0 = clear) via PreventionSystem.ChangeHeatStage. The old
// SetWantedLevelFact/SetSystemLock names don't exist on this build; the real
// method is ChangeHeatStage(EPreventionHeatStage, String).
void SetWantedLevel(int level)
{
    auto ps = GetPreventionSystem();
    if (!ps)
    {
        CM_INFO("SetWantedLevel: prevention system unavailable");
        return;
    }
    int32_t lvl = level < 0 ? 0 : (level > 5 ? 5 : level);
    char member[16];
    std::snprintf(member, sizeof(member), "Heat_%d", lvl);
    int32_t stage = static_cast<int32_t>(game::ResolveEnum("EPreventionHeatStage", member, lvl));
    RED4ext::CString reason("CyberBallz");
    bool ok = game::CallAuto(ps, "ChangeHeatStage", nullptr, stage, reason);
    CM_INFO("SetWantedLevel(%d) ChangeHeatStage(stage=%d) -> %d", lvl, stage, ok ? 1 : 0);
}

void ClearHeat()
{
    SetWantedLevel(0);
}

// The vehicle the player is currently mounted in (empty if on foot).
Player GetMountedVehicle()
{
    RED4ext::Handle<RED4ext::IScriptable> veh;
    auto player = game::GetPlayer();
    if (!player)
        return veh;
    RED4ext::ScriptGameInstance gi;
    RED4ext::ExecuteGlobalFunction("GetMountedVehicle;GameInstance;GameObject", &veh, gi, player);
    return veh;
}

// Per-frame state
bool g_invulnApplied = false;
bool g_vehInvulnApplied = false;
bool g_freezeActive = false;
uint32_t g_frozenSeconds = 0;

// Log an experimental feature once, the first time it is seen enabled, so the
// UI/Diagnostics stay honest about what is validated vs pending without spam.
void ExperimentalToggle(const std::atomic<bool>& flag, const char* name)
{
    if (!flag.load())
        return;
    static std::unordered_set<std::string> logged;
    if (logged.insert(name).second)
        CM_WARN("Experimental feature active (binding pending validation): %s", name);
}

// (Removed: legacy SyncWeaponRecoilSpread / GetEquippedWeaponStatsId path. It
// resolved the weapon via GetItemData, which returns a weak handle - storing it
// strong corrupted the refcount and crashed. No Recoil / No Spread now go
// through SyncWeaponStatMod (GetActiveWeaponObj + entity-hash StatsObjectID).)

// Feature tick registry
// Each continuous "feature group" is a self-contained tick function, run under
// its own crash guard. If one faults it is disabled for the session and logged,
// while every other feature keeps running (per-feature isolation). Adding a new
// group is a one-line entry in g_featureTicks.

// Player: god mode + survival pools
void TickPlayer(const Player& player)
{
    auto& s = State::Get();
    // No Fall Damage is no longer full invulnerability - it is a FallDamageReduction
    // stat modifier (see TickStatMods), so only god mode drives the Invulnerable status.
    const bool wantInvuln = s.godMode.load();
    if (wantInvuln)
    {
        if (!HasStatus(player, kInvulnEffect))
            ApplyStatus(player, kInvulnEffect);
        g_invulnApplied = true;
    }
    else if (g_invulnApplied)
    {
        RemoveStatus(player, kInvulnEffect);
        g_invulnApplied = false;
    }

    // Pin the health pool when god mode is on so any damage that slips past the
    // Invulnerable status is instantly refilled.
    if (s.godMode)
        SetStatPoolPct(player, StatPool::Health, 100.0f);
    if (s.infiniteStamina)
        SetStatPoolPct(player, StatPool::Stamina, 100.0f);
    if (s.infiniteOxygen)
        SetStatPoolPct(player, StatPool::Oxygen, 100.0f);
    if (s.infiniteMemory)
        SetStatPoolPct(player, StatPool::Memory, 100.0f);

    // Override FOV: set the player camera component's FOV each frame. The toggle
    // had no implementation before; the real setter is gameCameraComponent::SetFOV.
    if (s.fovEnabled)
    {
        RED4ext::Handle<RED4ext::IScriptable> cam;
        RED4ext::CName camType("gameCameraComponent");
        const bool found = game::CallAuto(player, "FindComponentByType", &cam, camType) && cam;
        if (found)
        {
            float fov = s.fov.load();
            game::CallAuto(cam, "SetFOV", nullptr, fov);
        }
        else
        {
            // One-time: dump the player's component types to locate the camera component.
            static bool s_dumped = false;
            if (!s_dumped)
            {
                s_dumped = true;
                CM_WARN("FOV: gameCameraComponent not on player; dumping components");
                RED4ext::DynArray<RED4ext::Handle<RED4ext::IScriptable>> comps;
                if (game::CallAuto(player, "GetComponents", &comps))
                    for (uint32_t i = 0; i < comps.Size(); ++i)
                        if (comps[i] && comps[i].instance && comps[i].instance->GetType())
                            CM_INFO("  component: %s", comps[i].instance->GetType()->GetName().ToString());
            }
        }
    }

    // Auto-skip cutscenes: periodically request a skip while enabled (~0.75s).
    if (s.autoSkipCutscenes)
    {
        static int frame = 0;
        if (++frame % 45 == 0)
            SkipCutscene();
    }
}

// Vehicle: god mode on the mounted vehicle
void TickVehicle(const Player&)
{
    auto& s = State::Get();
    if (s.vehicleGodMode)
    {
        auto veh = GetMountedVehicle();
        static bool s_logged = false;
        if (!s_logged)
        {
            s_logged = true;
            CM_INFO("VehicleGod: mounted vehicle %s", veh ? "resolved" : "NULL (not driving?)");
        }
        if (veh)
        {
            // Pin the vehicle's health pool full every frame so it takes no damage
            // (mirrors player god mode). Keep the Invulnerable status as a backup.
            SetStatPoolPct(veh, StatPool::Health, 100.0f);
            if (!HasStatus(veh, kInvulnEffect))
                ApplyStatus(veh, kInvulnEffect);
            g_vehInvulnApplied = true;
        }
    }
    else if (g_vehInvulnApplied)
    {
        auto veh = GetMountedVehicle();
        if (veh)
            RemoveStatus(veh, kInvulnEffect);
        g_vehInvulnApplied = false;
    }
}

// No-clip free-fly
// Holds the player at a position we advance each frame and re-teleport to;
// Teleport ignores collision, so the player passes through geometry and gravity
// is overridden. Because the teleport facility forces the player's heading every
// frame (which is why the mouse can't yaw you), we own the heading ourselves and
// turn it with Q/E - the body, and therefore the camera, follow. Mouse still
// controls look-pitch. On-foot only; input suppressed while the menu is open.
//
// Controls: W/S forward-back, A/D strafe, Q/E turn, Space/Ctrl up-down, Shift boost.
void TickNoClip(const Player& player)
{
    auto& s = State::Get();
    static bool captured = false;
    static RED4ext::Vector4 pos{};
    static float yawDeg = 0.0f;

    if (!s.noClipEnabled || !player || GetMountedVehicle())
    {
        captured = false;
        return;
    }
    if (!captured)
    {
        if (!game::CallAuto(player, "GetWorldPosition", &pos))
            return;
        game::CallAuto(player, "GetWorldYaw", &yawDeg); // start facing where you are
        captured = true;
    }

    if (!s.uiOpen.load())
    {
        float step = s.noClipSpeed.load();
        float turn = 2.0f; // degrees/frame
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) { step *= 3.0f; turn *= 2.0f; }
        if (GetAsyncKeyState('Q') & 0x8000) yawDeg -= turn;
        if (GetAsyncKeyState('E') & 0x8000) yawDeg += turn;

        const float yawRad = yawDeg * 0.01745329252f;
        const float fx = -std::sin(yawRad), fy = std::cos(yawRad); // forward
        const float rx = std::cos(yawRad), ry = std::sin(yawRad);  // right
        float dx = 0.0f, dy = 0.0f, dz = 0.0f;
        if (GetAsyncKeyState('W') & 0x8000) { dx += fx; dy += fy; }
        if (GetAsyncKeyState('S') & 0x8000) { dx -= fx; dy -= fy; }
        if (GetAsyncKeyState('D') & 0x8000) { dx += rx; dy += ry; }
        if (GetAsyncKeyState('A') & 0x8000) { dx -= rx; dy -= ry; }
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) dz += 1.0f;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) dz -= 1.0f;
        pos.X += dx * step;
        pos.Y += dy * step;
        pos.Z += dz * step;
    }

    // Re-assert position + our owned heading every frame.
    RED4ext::Vector4 dst(pos.X, pos.Y, pos.Z, 1.0f);
    RED4ext::EulerAngles rot{};
    rot.Yaw = yawDeg;
    if (auto tp = Sys("GetTeleportationFacility;GameInstance"))
        game::CallAuto(tp, "Teleport", nullptr, player, dst, rot);
}

// Time: freeze time / photo freeze (share the captured clock value)
void TickTime(const Player&)
{
    auto& s = State::Get();
    const bool wantFreeze = s.freezeTime || s.photoFreeze;
    if (wantFreeze)
    {
        if (!g_freezeActive)
        {
            g_frozenSeconds = GetGameSeconds();
            g_freezeActive = true;
        }
        SetGameSeconds(g_frozenSeconds);
    }
    else
    {
        g_freezeActive = false;
    }
}

// Stat modifiers: movement, cooldowns, netrunner, camo, Sandevistan
void TickStatMods(const Player&)
{
    auto& s = State::Get();
    SyncPlayerStatMod(s.moveSpeedEnabled, "MaxSpeed", StatModType::Multiplier, s.moveSpeedMult.load());
    SyncPlayerStatMod(s.carryCapacityEnabled, "CarryCapacity", StatModType::Additive, s.carryCapacity.load());
    SyncPlayerStatMod(s.quickhackCostZero, "MemoryCostReduction", StatModType::Additive, 10000.0f);
    SyncPlayerStatMod(s.quickhackNoCooldown, "QuickhacksCooldownReduction", StatModType::Additive, 1.0f);
    SyncPlayerStatMod(s.ramRegenBoost, "MemoryRegenRateMult", StatModType::Additive, 100.0f);
    SyncPlayerStatMod(s.opticalCamoInfinite, "OpticalCamoRechargeDuration", StatModType::Multiplier, 0.01f);
    SyncPlayerStatMod(s.opticalCamoInfinite, "OpticalCamoChargesRegenRate", StatModType::Additive, 100.0f);
    SyncPlayerStatMod(s.sandevistanInfinite, "TimeDilationSandevistanRechargeDuration", StatModType::Multiplier, 0.01f);
    // Gadget / consumable charge regen + cyberware cooldowns.
    SyncPlayerStatMod(s.infiniteGrenades, "GrenadesChargesRegenMult", StatModType::Additive, 10000.0f);
    SyncPlayerStatMod(s.infiniteProjectiles, "ProjectileLauncherChargesRegenMult", StatModType::Additive, 10000.0f);
    SyncPlayerStatMod(s.infiniteHeals, "HealingItemsChargesRegenMult", StatModType::Additive, 10000.0f);
    SyncPlayerStatMod(s.berserkNoCooldown, "BerserkChargesRegenRate", StatModType::Additive, 100.0f);
    SyncPlayerStatMod(s.kerenzikovNoCooldown, "KerenzikovCooldownDuration", StatModType::Multiplier, 0.01f);
    SyncPlayerStatMod(s.overclockNoCooldown, "CyberdeckOverclockCooldown", StatModType::Multiplier, 0.01f);
    SyncPlayerStatMod(s.overclockNoCooldown, "CyberdeckOverclockRegenRate", StatModType::Additive, 100.0f);

    // (No Stamina Cost removed - Infinite Stamina already pins the pool full.
    //  crit/headshot/armor-pen are WEAPON stats, applied in TickCombat.)

    // Player utility.
    SyncPlayerStatMod(s.superJumpEnabled, "JumpHeight", StatModType::Multiplier, s.superJumpMult.load());
    // Huge value so big falls are fully negated regardless of whether the stat is
    // a 0..1 fraction (clamps to full) or a flat subtract (covers any fall).
    SyncPlayerStatMod(s.noFallDamage, "FallDamageReduction", StatModType::Additive, 1000000.0f);
    SyncPlayerStatMod(s.noRagdoll, "KnockdownImmunity", StatModType::Additive, 1.0f);
    SyncPlayerStatMod(s.healthRegenBoost, "HealthInCombatRegenEnabled", StatModType::Additive, 1.0f);
    SyncPlayerStatMod(s.healthRegenBoost, "HealthOutOfCombatRegenEnabled", StatModType::Additive, 1.0f);
    SyncPlayerStatMod(s.healthRegenBoost, "HealthInCombatRegenRateMult", StatModType::Additive, 50.0f);
    SyncPlayerStatMod(s.healthRegenBoost, "HealthOutOfCombatRegenRateMult", StatModType::Additive, 50.0f);
}

// Stealth & Heat: undetectable cloak + prevention-system freeze
void TickStealthHeat(const Player& player)
{
    auto& s = State::Get();

    // Undetectable: reassert the Cloaked status if stripped (self-heals reload).
    {
        static bool applied = false;
        const char* kCloak = "BaseStatusEffect.Cloaked";
        if (s.undetectable && (!applied || !HasStatus(player, kCloak)))
        {
            ApplyStatus(player, kCloak);
            applied = true;
        }
        else if (!s.undetectable && applied)
        {
            RemoveStatus(player, kCloak);
            applied = false;
        }
    }

    // Freeze wanted level: lock the prevention system so heat can't change.
    // Re-apply after a reload (player changed).
    {
        static bool applied = false;
        static int gen = -1;
        if (s.heatFreeze != applied || (s.heatFreeze && gen != g_playerGen))
        {
            gen = g_playerGen;
            RED4ext::Handle<RED4ext::IScriptable> ps;
            if (game::CallAuto(player, "GetPreventionSystem", &ps) && ps)
            {
                // Freeze = disable the prevention system so heat can't escalate.
                // TogglePreventionSystem(true) re-enables it. (SetSystemLock didn't exist.)
                bool enable = !s.heatFreeze;
                bool ok = game::CallAuto(ps, "TogglePreventionSystem", nullptr, enable);
                CM_INFO("HeatFreeze=%d -> TogglePreventionSystem(%d) -> %d", s.heatFreeze ? 1 : 0,
                        enable ? 1 : 0, ok ? 1 : 0);
            }
            applied = s.heatFreeze;
        }
    }

    // Hold Wanted Level: continuously pin the wanted level to the slider target,
    // re-asserted ~once/sec. At target 0 we also disable the prevention system so
    // cops never escalate (shooting near them won't raise heat); at >0 we keep it
    // enabled and hold that many stars.
    {
        static bool lastHold = false;
        static int lastTarget = -1;
        static int frame = 0;
        if (s.holdWantedLevel)
        {
            const int target = s.wantedLevelTarget.load();
            if (!lastHold || target != lastTarget || (++frame % 60 == 0))
            {
                RED4ext::Handle<RED4ext::IScriptable> ps;
                if (game::CallAuto(player, "GetPreventionSystem", &ps) && ps)
                {
                    bool enablePrevention = (target > 0);
                    game::CallAuto(ps, "TogglePreventionSystem", nullptr, enablePrevention);
                }
                SetWantedLevel(target);
            }
            lastHold = true;
            lastTarget = target;
        }
        else if (lastHold)
        {
            RED4ext::Handle<RED4ext::IScriptable> ps;
            if (game::CallAuto(player, "GetPreventionSystem", &ps) && ps)
            {
                bool on = true;
                game::CallAuto(ps, "TogglePreventionSystem", nullptr, on); // restore police
            }
            lastHold = false;
            lastTarget = -1;
            frame = 0;
        }
    }
}

// Combat: infinite ammo + weapon recoil/spread mods
void TickCombat(const Player& player)
{
    auto& s = State::Get();

    // Infinite reserve ammo (never deplete the inventory ammo pool).
    {
        static bool applied = false;
        const char* kInfAmmo = "GameplayRestriction.InfiniteAmmo";
        if (s.infiniteAmmoNoReload && (!applied || !HasStatus(player, kInfAmmo)))
        {
            ApplyStatus(player, kInfAmmo);
            applied = true;
        }
        else if (!s.infiniteAmmoNoReload && applied)
        {
            RemoveStatus(player, kInfAmmo);
            applied = false;
        }
    }

    // Per-weapon stat modifiers, reapplied as you swap weapons:
    //  - No-reload feel: ReloadTime x0 so reloads are instant (paired with the
    //    infinite reserve above). A literal "magazine count never moves" would
    //    need a fire-path hook - no RTTI setter exists for the loaded count.
    //  - No Recoil / No Spread: zero the weapon's master Recoil / Spread stats.
    //    Reworked off the safe CreateStatModifier path (the old GetItemData weak
    //    handle is what crashed before).
    // Infinite Ammo / No Reload: huge magazine so it never empties, plus instant
    // reload time. Paired with the InfiniteAmmo reserve status above.
    SyncWeaponStatMod(s.infiniteAmmoNoReload, player, "MagazineCapacity", StatModType::Additive, 9999999.0f);
    SyncWeaponStatMod(s.infiniteAmmoNoReload, player, "ReloadTime", StatModType::Multiplier, 0.0f);
    SyncWeaponStatMod(s.infiniteAmmoNoReload, player, "EmptyReloadTime", StatModType::Multiplier, 0.0f);
    // Fill the clip immediately when the toggle turns on or the weapon changes:
    // the capacity is now huge, so one instant reload (ReloadTime x0) tops the
    // magazine to the boosted value right away - no manual reload needed.
    {
        static bool wasOn = false;
        static uint64_t lastHash = 0;
        if (s.infiniteAmmoNoReload)
        {
            auto wpn = GetActiveWeaponObj(player);
            RED4ext::game::StatsObjectID sid{};
            if (wpn && GetWeaponStatsId(wpn, sid) && sid.entityHash != 0 && (!wasOn || sid.entityHash != lastHash))
            {
                game::CallAuto(wpn, "StartReload", nullptr);
                wasOn = true;
                lastHash = sid.entityHash;
                CM_INFO("InfiniteAmmo: auto-reload to fill boosted magazine (hash=%llu)",
                        static_cast<unsigned long long>(sid.entityHash));
            }
        }
        else
        {
            wasOn = false;
            lastHash = 0;
        }
    }
    // No Recoil: zero the specific recoil-kick stats (the proven set from the old
    // path) rather than the master Recoil, via the safe GetActiveWeaponObj route.
    SyncWeaponStatMod(s.noRecoil, player, "RecoilKickMin", StatModType::Multiplier, 0.0f);
    SyncWeaponStatMod(s.noRecoil, player, "RecoilKickMax", StatModType::Multiplier, 0.0f);
    // No Spread: the master "Spread" stat had no effect; zero the concrete spread
    // cone stats (hipfire + ADS) that the accuracy calc actually reads.
    SyncWeaponStatMod(s.noSpread, player, "SpreadDefaultX", StatModType::Multiplier, 0.0f);
    SyncWeaponStatMod(s.noSpread, player, "SpreadDefaultY", StatModType::Multiplier, 0.0f);
    SyncWeaponStatMod(s.noSpread, player, "SpreadMaxX", StatModType::Multiplier, 0.0f);
    SyncWeaponStatMod(s.noSpread, player, "SpreadMaxY", StatModType::Multiplier, 0.0f);
    SyncWeaponStatMod(s.noSpread, player, "SpreadAdsMaxX", StatModType::Multiplier, 0.0f);
    SyncWeaponStatMod(s.noSpread, player, "SpreadAdsMaxY", StatModType::Multiplier, 0.0f);
    // Crit / headshot / armor-pen are weapon stats (player-level had no effect).
    SyncWeaponStatMod(s.critChanceEnabled, player, "CritChance", StatModType::Additive, s.critChanceMult.load());
    SyncWeaponStatMod(s.critDamageEnabled, player, "CritDamage", StatModType::Additive, s.critDamageMult.load());
    SyncWeaponStatMod(s.headshotDamageEnabled, player, "HeadshotDamageMultiplier", StatModType::Additive,
                      s.headshotDamageMult.load());
    SyncWeaponStatMod(s.armorPenEnabled, player, "CanWeaponIgnoreArmor", StatModType::Additive, 1.0f);
    // Tech weapon: zero charge time for instant full charge.
    SyncWeaponStatMod(s.techChargeFast, player, "ChargeTime", StatModType::Multiplier, 0.0f);
    // Rapid Fire: shrink the per-shot cycle time / shot delay (x faster).
    {
        float rf = s.rapidFireMult.load();
        if (rf < 1.0f) rf = 1.0f;
        const float inv = 1.0f / rf;
        SyncWeaponStatMod(s.rapidFireEnabled, player, "CycleTime", StatModType::Multiplier, inv);
        SyncWeaponStatMod(s.rapidFireEnabled, player, "ShotDelay", StatModType::Multiplier, inv);
    }

    // Damage Multiplier / One-Hit Kill: scale the drawn weapon's base damage
    // (One-Hit uses a huge multiplier). Same safe weapon-stat path; the slider is
    // live thanks to value-change tracking. Applies to ranged weapons; melee uses
    // separate stats and is not covered here.
    const bool dmgWant = s.damageMultEnabled.load() || s.oneHitKill.load();
    const float dmgMult = s.oneHitKill.load() ? 1000.0f : s.damageMult.load();
    // BaseDamage is the source stat the damage calc derives from (BaseDamageMin/Max
    // are derived display values and don't affect output - the earlier no-op).
    SyncWeaponStatMod(dmgWant, player, "BaseDamage", StatModType::Multiplier, dmgMult);
}

// Experimental toggles: bindings pending in-game validation (log once)
void TickExperimental(const Player&)
{
    auto& s = State::Get();
    // damageMult / oneHitKill are now wired in TickCombat (weapon BaseDamage mod).
    ExperimentalToggle(s.alertFreeze, "Ghost: alert freeze");
    ExperimentalToggle(s.pedDensityEnabled, "City: pedestrian density");
    ExperimentalToggle(s.trafficDensityEnabled, "City: traffic density");
    ExperimentalToggle(s.freeShopping, "Economy: free shopping");
    ExperimentalToggle(s.sellMultEnabled, "Economy: sell multiplier");
    ExperimentalToggle(s.vehicleBoost, "Vehicle: infinite boost");
}

using TickFn = void (*)(const Player&);
struct FeatureTick
{
    const char* name;
    TickFn fn;
    bool disabled;
};
FeatureTick g_featureTicks[] = {
    {"Player", TickPlayer, false},      {"Vehicle", TickVehicle, false},
    {"Time", TickTime, false},          {"StatMods", TickStatMods, false},
    {"Stealth/Heat", TickStealthHeat, false}, {"Combat", TickCombat, false},
    {"NoClip", TickNoClip, false},
    {"Experimental", TickExperimental, false},
};

// Per-feature crash guard (no unwinding objects in this frame, required for SEH).
void SafeFeatureTick(FeatureTick& f, const Player& player)
{
    if (f.disabled)
        return;
    __try
    {
        f.fn(player);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        f.disabled = true;
        CM_WARN("Feature '%s' faulted - disabled this session; other features keep running", f.name);
    }
}

void ApplyContinuous(const Player& player)
{
    // Detect a player change (save load / respawn): bump the generation so the
    // stat-modifier registry reapplies its mods to the new entity.
    static void* s_lastPlayer = nullptr;
    if (player.instance != s_lastPlayer)
    {
        s_lastPlayer = player.instance;
        ++g_playerGen;
    }

    for (auto& f : g_featureTicks)
        SafeFeatureTick(f, player);
}

// One-time probe of every game binding the menu relies on. Runs after the
// world is loaded (systems resolvable). Results go to the log + Diagnostics
// panel so the install can be verified against the live game build.
void RunSelfTest()
{
    auto& st = State::Get();
    Diagnostics d = st.GetDiagnostics(); // preserve load/hook/version info
    d.checks.clear();
    d.okCount = 0;
    d.total = 0;
    auto add = [&](const char* name, bool ok)
    {
        d.checks.push_back({name, ok});
        ++d.total;
        if (ok)
            ++d.okCount;
    };

    auto methodOnSystem = [&](const char* getter, const char* method) -> bool
    {
        auto sys = Sys(getter);
        if (!sys || !sys.instance)
            return false;
        return game::GetMethodFromClass(sys.instance->GetType(), method) != nullptr;
    };

    CM_INFO("selftest: begin");
    add("RTTI system", RED4ext::CRTTISystem::Get() != nullptr);
    add("Player handle", static_cast<bool>(game::GetPlayer()));
    add("God mode (StatusEffectSystem)", methodOnSystem("GetStatusEffectSystem;GameInstance", "ApplyStatusEffect"));
    add("Stat pools (health/stamina)", methodOnSystem("GetStatPoolsSystem;GameInstance", "RequestSettingStatPoolValue"));
    add("Money & items (TransactionSystem)", methodOnSystem("GetTransactionSystem;GameInstance", "GiveItem"));
    add("Time (TimeSystem)", methodOnSystem("GetTimeSystem;GameInstance", "SetGameTimeBySeconds"));
    {
        // Weather has no reflected getter; resolve the interface the same way
        // SetWeather does and confirm the method exists on it.
        auto wi = GetWeatherInterface();
        bool weatherOk = wi && wi.instance && game::GetMethodFromClass(wi.instance->GetType(), "SetWeather") != nullptr;
        add("Weather (WeatherSystem)", weatherOk);
    }
    add("Vehicles (VehicleSystem)", methodOnSystem("GetVehicleSystem;GameInstance", "EnablePlayerVehicle"));
    add("Teleport (TeleportationFacility)", methodOnSystem("GetTeleportationFacility;GameInstance", "Teleport"));
    add("Quest objective (JournalManager)", methodOnSystem("GetJournalManager;GameInstance", "GetTrackedEntry"));
    add("Tutorials (QuestsSystem)", methodOnSystem("GetQuestsSystem;GameInstance", "SetFactStr"));
    const bool rpgOk = game::GetStaticFunction("gameRPGManager", "CreateStatModifier") != nullptr;
    add("Stat mods (gameRPGManager)", rpgOk);
    if (!rpgOk)
    {
        game::LogClassFunctions("gameRPGManager", nullptr);
        game::LogGlobalFunctions("CreateStatModifier");
    }

    // Heat: prevention system is reached off the player, not GameInstance.
    {
        auto pl = game::GetPlayer();
        RED4ext::Handle<RED4ext::IScriptable> ps;
        bool heatOk = pl && game::CallAuto(pl, "GetPreventionSystem", &ps) && static_cast<bool>(ps);
        add("Heat (PreventionSystem)", heatOk);
    }

    const bool devOk = game::GetStaticFunction("PlayerDevelopmentSystem", "GetData") != nullptr;
    add("XP (PlayerDevelopmentSystem)", devOk);
    if (!devOk)
        game::LogGlobalFunctions("GetData");

    // Weather lives in the world-runtime layer with no reflected getter; if we
    // cannot obtain the interface, dump every function that RETURNS a weather
    // type (that is the accessor we need) plus the interface/runtime methods.
    if (!GetWeatherInterface())
    {
        game::LogFunctionsReturning("Weather");
        game::LogClassFunctions("worldWeatherScriptInterface", nullptr);
        game::LogClassFunctions("worldRuntimeSystemWeather", nullptr);
    }

    // One-time: dump the real signatures of the write methods on the *resolved*
    // (concrete) system instances, so argument order/count can be validated for
    // the features that resolve but currently produce no effect.
    auto dumpSysFns = [&](const char* getter, const char* filter)
    {
        auto sys = Sys(getter);
        if (sys && sys.instance && sys.instance->GetType())
            game::LogClassFunctions(sys.instance->GetType()->GetName().ToString(), filter);
    };
    dumpSysFns("GetStatusEffectSystem;GameInstance", "Status");
    dumpSysFns("GetTransactionSystem;GameInstance", "Give");
    dumpSysFns("GetVehicleSystem;GameInstance", "Enable");
    dumpSysFns("GetQuestsSystem;GameInstance", "Fact");

    // Auto-discovery (no console needed): locate the weather method and the
    // wanted-level requests, and dump the player's prevention system methods.
    game::LogFunctionsMatching("Weather");
    game::LogFunctionsMatching("Wanted");

    // FOV + tutorials discovery: find the real setters so they can be wired (the
    // current FOV path is unimplemented and the tutorial fact does not gate on 2.3.1).
    game::LogFunctionsMatching("FOV");
    game::LogFunctionsMatching("FieldOfView");
    game::LogFunctionsMatching("Tutorial");
    {
        auto pl = game::GetPlayer();
        RED4ext::Handle<RED4ext::IScriptable> ps;
        if (pl && game::CallAuto(pl, "GetPreventionSystem", &ps) && ps && ps.instance && ps.instance->GetType())
            game::LogClassFunctions(ps.instance->GetType()->GetName().ToString(), nullptr);
    }


    d.selfTestRan = true;
    st.SetDiagnostics(d);

    CM_INFO("==================== CyberBallz binding self-test ====================");
    CM_INFO("Bindings available: %d / %d", d.okCount, d.total);
    for (const auto& c : d.checks)
        CM_INFO("  [%s] %s", c.ok ? "OK  " : "MISS", c.name.c_str());
    CM_INFO("=====================================================================");
}

void PublishDisplay(const Player& player)
{
    DisplayInfo d;
    d.gameLoaded = true;
    d.playerValid = static_cast<bool>(player);
    if (player)
    {
        // GetStatPoolValue returns the absolute current pool value (e.g. 119 HP),
        // so the bar's max must be the player's actual max stat, not a flat 100.
        d.health = GetStatPoolPct(player, StatPool::Health);
        const float maxHp = GetStatValue(player, "Health");
        d.healthMax = maxHp > 1.0f ? maxHp : 100.0f;
        d.stamina = GetStatPoolPct(player, StatPool::Stamina);
        const float maxStam = GetStatValue(player, "Stamina");
        d.staminaMax = maxStam > 1.0f ? maxStam : 100.0f;
        d.ram = GetStatPoolPct(player, StatPool::Memory);
        const float maxRam = GetStatValue(player, "Memory");
        d.ramMax = maxRam > 1.0f ? maxRam : 100.0f;
        d.inVehicle = static_cast<bool>(GetMountedVehicle());

        RED4ext::Vector4 pos{};
        if (game::CallAuto(player, "GetWorldPosition", &pos))
        {
            d.posX = pos.X;
            d.posY = pos.Y;
            d.posZ = pos.Z;
        }

        static bool loggedHp = false;
        if (!loggedHp)
        {
            loggedHp = true;
            CM_INFO("PublishDisplay: healthPool=%.1f maxHealthStat=%.1f staminaPool=%.1f ramPool=%.1f",
                    d.health, maxHp, d.stamina, d.ram);
        }

        // Feed the Dashboard sparklines (~2 Hz; cheap, only while UI open).
        static double s_lastSample = 0.0;
        const double now = static_cast<double>(GetGameSeconds());
        if (now != s_lastSample)
        {
            s_lastSample = now;
            const float hpPct = d.healthMax > 0 ? d.health / d.healthMax * 100.0f : 0;
            const float ramPct = d.ramMax > 0 ? d.ram / d.ramMax * 100.0f : 0;
            State::Get().PushHistory(hpPct, ramPct);
        }
    }
    State::Get().SetDisplay(d);
}

void Execute(const Action& a)
{
    auto player = game::GetPlayer();
    CM_INFO("Execute: action=%d playerValid=%d value=%.1f qty=%d text='%s'",
            static_cast<int>(a.type), player ? 1 : 0, a.value, a.quantity, a.text.c_str());

    switch (a.type)
    {
    case ActionType::HealPlayer:
        SetStatPoolPct(player, StatPool::Health, 100.0f);
        SetStatPoolPct(player, StatPool::Stamina, 100.0f);
        SetStatPoolPct(player, StatPool::Oxygen, 100.0f);
        break;

    case ActionType::MaxAllStats:
        MaxAllStats();
        break;

    case ActionType::AddMoney:
        GiveItem("Items.money", static_cast<int>(a.value));
        break;

    case ActionType::GiveItem:
        GiveItem(a.text, a.quantity);
        break;

    case ActionType::MaxCraftingComponents:
    {
        static const char* kComponents[] = {
            "Items.CommonMaterial1",  "Items.UncommonMaterial1", "Items.RareMaterial1",
            "Items.EpicMaterial1",    "Items.LegendaryMaterial1",
        };
        for (auto* c : kComponents)
            GiveItem(c, 5000);
        break;
    }

    case ActionType::AddStreetCredXP:
        AddExperience(Proficiency::StreetCred, a.quantity);
        break;
    case ActionType::AddLevelXP:
        AddExperience(Proficiency::Level, a.quantity);
        break;

    case ActionType::SetTimeHours:
        SetGameSeconds(static_cast<uint32_t>(a.value * 3600.0));
        break;

    case ActionType::SetWeather:
        SetWeather(a.text);
        break;

    case ActionType::UnlockAllVehicles:
        UnlockAllVehicles();
        break;
    case ActionType::SpawnVehicle:
        // Treat the "spawn" record field as an unlock (record-based).
        EnableVehicle(a.text, true);
        break;

    case ActionType::TeleportToWaypoint:
        TeleportToWaypoint();
        break;

    case ActionType::SaveLocation:
    {
        RED4ext::Vector4 pos{};
        if (player && game::CallAuto(player, "GetWorldPosition", &pos))
        {
            State::SavedLocation loc;
            loc.valid = true;
            loc.x = pos.X;
            loc.y = pos.Y;
            loc.z = pos.Z;
            State::Get().SetLocation(a.quantity, loc);
        }
        break;
    }

    case ActionType::TeleportToLocation:
    {
        auto loc = State::Get().GetLocation(a.quantity);
        if (loc.valid)
            TeleportPlayerTo(loc.x, loc.y, loc.z);
        break;
    }

    case ActionType::TeleportToObjective:
        TeleportToObjective();
        break;

    case ActionType::DisableTutorials:
        DisableTutorials();
        break;

    case ActionType::SkipCutscene:
        SkipCutscene();
        break;

    // v2 actions: implemented (reuse confirmed systems)
    case ActionType::ApplyStatusToPlayer:
        ApplyStatusToPlayer(a.text);
        break;
    case ActionType::RemoveStatusFromPlayer:
        RemoveStatusFromPlayer(a.text);
        break;
    case ActionType::ClearAllStatusEffects:
        ClearCommonDebuffs();
        break;
    case ActionType::SetQuestFact:
        SetFact(a.text, a.quantity);
        break;
    case ActionType::AddComponentTier:
    {
        static const char* kTier[] = {"", "Items.CommonMaterial1", "Items.UncommonMaterial1",
                                      "Items.RareMaterial1", "Items.EpicMaterial1", "Items.LegendaryMaterial1"};
        if (a.quantity >= 1 && a.quantity <= 5)
            GiveItem(kTier[a.quantity], 5000);
        break;
    }
    case ActionType::TeleportToQuestMappin:
        TeleportToObjective(); // index-specific mappin handled inside (uses [0] today)
        break;
    case ActionType::ClearHeat:
        ClearHeat();
        break;
    case ActionType::SetHeat:
        SetWantedLevel(a.quantity);
        break;

    // Puppet Lab (Codeware DynamicEntitySystem)
    case ActionType::SpawnNpc:
        SpawnEntityRecord(a.text);
        break;
    case ActionType::DespawnLastNpc:
        DespawnLastEntity();
        break;
    case ActionType::DespawnAllNpcs:
        DespawnAllEntities();
        break;

    // v2 actions: experimental (binding pending in-game validation)
    case ActionType::RespecPerks:
    case ActionType::ResetAttributes:
    case ActionType::CompleteTrackedQuest:
    case ActionType::UntrackAllQuests:
    case ActionType::FreezeNpcsInRadius:
    case ActionType::FlipVehicleUpright:
    case ActionType::TeleportToOwnedVehicle:
    case ActionType::RadioNextStation:
    case ActionType::RadioToggle:
    case ActionType::ScanNearbyLoot:
    case ActionType::SummonVehicle:
    case ActionType::RepairVehicle:
        CM_INFO("Action %d queued (experimental binding pending validation)", static_cast<int>(a.type));
        break;
    }
}

// Per-section crash isolation
// SEH guards so a fault in one game call can't abort the rest of the frame.
// These functions must not declare C++ objects requiring unwinding (SEH rule),
// hence the by-reference params and no locals.
void SafeApplyContinuous(const Player& player)
{
    __try
    {
        ApplyContinuous(player);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            CM_WARN("ApplyContinuous faulted - continuous toggles guarded, frame continues");
        }
    }
}

void SafeExecute(const Action& a)
{
    __try
    {
        Execute(a);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        CM_WARN("Action %d faulted - guarded, queue continues", static_cast<int>(a.type));
    }
}
} // namespace

void OnGameUpdate()
{
    static bool s_enter = false;
    if (!s_enter) { s_enter = true; CM_INFO("OGU: first call (game-thread update is firing)"); }

    // Heartbeat: confirms OnUpdate keeps firing and whether the player resolves.
    static int s_frame = 0;
    auto player = game::GetPlayer();
    if (++s_frame % 600 == 0)
        CM_INFO("OGU heartbeat: frame=%d, playerValid=%d", s_frame, player ? 1 : 0);

    if (!player)
    {
        DisplayInfo d;
        d.gameLoaded = false;
        State::Get().SetDisplay(d);
        return;
    }
    static bool s_pv = false;
    if (!s_pv) { s_pv = true; CM_INFO("OGU: player resolved - features live"); }

    // Run the binding self-test once. Flag is set BEFORE running so that, even
    // if a probe faults (caught by the SEH wrapper), it is not retried forever.
    static bool s_selfTestDone = false;
    if (!s_selfTestDone)
    {
        s_selfTestDone = true;
        CM_INFO("OGU: running self-test");
        RunSelfTest();
        CM_INFO("OGU: self-test returned");
    }

    // Only poll live stats (RTTI reads) while the menu is open and showing them.
    if (State::Get().uiOpen.load())
        PublishDisplay(player);
    static bool s_pd = false;
    if (!s_pd) { s_pd = true; CM_INFO("OGU: PublishDisplay path ok"); }

    // Continuous toggles still apply with the menu closed (god mode, etc.).
    // Guarded so a faulting toggle cannot prevent the action queue from draining.
    SafeApplyContinuous(player);
    static bool s_ac = false;
    if (!s_ac) { s_ac = true; CM_INFO("OGU: ApplyContinuous ok"); }

    const auto actions = State::Get().Drain();
    for (const auto& action : actions)
        SafeExecute(action);

    // Execute any queued debug-console commands (RTTI access on the game thread).
    debug::Pump();
}

// Self-check report: rerun the binding probes, read back the state a few key
// features are supposed to have applied, and write a Markdown report next to the
// log. Verifiable without playing - this is what the headless test pass reads.
void RunSelfCheck()
{
    RunSelfTest(); // refresh binding diagnostics
    auto& st = State::Get();
    const auto diag = st.GetDiagnostics();
    auto player = game::GetPlayer();

    // Per-feature read-backs: does the observed game state match the toggle?
    struct Check { const char* feature; const char* result; };
    auto yn = [](bool b) { return b ? "applied" : "not applied"; };

    SYSTEMTIME t;
    GetLocalTime(&t);
    char leaf[64];
    std::snprintf(leaf, sizeof(leaf), "selfcheck_%04d%02d%02d_%02d%02d%02d.md", t.wYear, t.wMonth, t.wDay,
                  t.wHour, t.wMinute, t.wSecond);
    const std::wstring path = cm::log::DataFilePath(leaf);

    FILE* f = path.empty() ? nullptr : _wfopen(path.c_str(), L"w");
    auto line = [&](const char* fmt, ...) {
        char buf[512];
        va_list a; va_start(a, fmt); std::vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
        if (f) { fputs(buf, f); fputc('\n', f); }
        CM_INFO("selfcheck: %s", buf);
    };

    line("# CyberBallz self-check - %04d-%02d-%02d %02d:%02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    line("");
    line("Player handle: **%s**", player ? "valid" : "INVALID (load a save first)");
    line("");
    line("## Bindings (%d / %d)", diag.okCount, diag.total);
    line("");
    line("| Binding | Status |");
    line("|---|---|");
    for (const auto& c : diag.checks)
        line("| %s | %s |", c.name.c_str(), c.ok ? "OK" : "**MISS**");
    line("");

    line("## Continuous feature state read-back");
    line("");
    line("| Feature | Toggle | Observed |");
    line("|---|---|---|");
    if (player)
    {
        line("| God Mode | %s | %s |", State::Get().godMode.load() ? "ON" : "off",
             yn(HasStatus(player, "BaseStatusEffect.Invulnerable")));
        line("| Undetectable | %s | %s |", State::Get().undetectable.load() ? "ON" : "off",
             yn(HasStatus(player, "BaseStatusEffect.Cloaked")));
        line("| Infinite Ammo | %s | reserve=%s |", State::Get().infiniteAmmoNoReload.load() ? "ON" : "off",
             yn(HasStatus(player, "GameplayRestriction.InfiniteAmmo")));
    }
    else
    {
        line("| (player invalid - read-backs skipped) | | |");
    }
    line("");

    line("## All registered toggles");
    line("");
    line("| Id | Category | Enabled | Experimental |");
    line("|---|---|---|---|");
    int on = 0;
    for (const auto& tg : toggles::All())
    {
        const bool en = tg.flag && tg.flag->load();
        if (en) ++on;
        line("| %s | %s | %s | %s |", tg.id, tg.category, en ? "ON" : "off", tg.experimental ? "yes" : "");
    }
    line("");
    line("**%d toggles enabled.** Report written to the CyberBallz data folder.", on);

    if (f)
        fclose(f);

    char u8[256];
    size_t n = 0;
    wcstombs_s(&n, u8, path.c_str(), sizeof(u8) - 1);
    CM_INFO("selfcheck: report saved -> %s", path.empty() ? "(failed)" : u8);
}
} // namespace cm::features
