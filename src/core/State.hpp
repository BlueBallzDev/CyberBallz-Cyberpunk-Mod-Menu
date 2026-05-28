#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// Shared state between the render thread (UI) and the game thread (feature
// execution). Continuous toggles/values live as atomics; one-shot actions are
// pushed to a queue drained on the game thread. Display info flows the other
// way: the game thread publishes a snapshot the UI copies each frame.
namespace cm
{
// One-shot actions queued by the UI, executed on the game thread.
enum class ActionType
{
    // Player
    HealPlayer,
    MaxAllStats,
    // Teleport (TeleportToWaypoint is defined below)
    SaveLocation,        // capture player position into a slot (quantity = slot)
    TeleportToLocation,  // teleport to a saved slot (quantity = slot)
    TeleportToObjective, // teleport to the tracked quest marker
    // Misc
    DisableTutorials,    // turn off tutorial popups (disable_tutorials fact)
    SkipCutscene,        // skip the currently playing cutscene/scene
    // Inventory & money
    AddMoney,
    GiveItem,
    MaxCraftingComponents,
    AddStreetCredXP,
    AddLevelXP,
    // Vehicles
    SummonVehicle,
    RepairVehicle,
    UnlockAllVehicles,
    SpawnVehicle,
    // World
    SetTimeHours,
    SetWeather,
    TeleportToWaypoint,

    // v2 expansion
    // Heat & Law
    ClearHeat,           // remove all wanted heat
    SetHeat,             // set wanted level (quantity = 0..5)
    // Buff / Debuff Laboratory + Ghost Ops one-shots
    ApplyStatusToPlayer, // text = StatusEffect record id
    RemoveStatusFromPlayer,
    ClearAllStatusEffects,
    // Build Lab
    RespecPerks,         // refund all perk points
    ResetAttributes,     // refund attribute points (experimental)
    // Quest & Journal
    SetQuestFact,        // text = fact name, quantity = value
    CompleteTrackedQuest,
    TeleportToQuestMappin, // quantity = mappin index for tracked objective
    UntrackAllQuests,
    // Puppet Lab
    SpawnNpc,            // text = character record id
    DespawnLastNpc,
    DespawnAllNpcs,
    FreezeNpcsInRadius,
    // Vehicle Tuner
    FlipVehicleUpright,
    TeleportToOwnedVehicle, // text = vehicle record (summon to player)
    // Media
    RadioNextStation,
    RadioToggle,
    // Loot & economy
    AddComponentTier,    // quantity = tier (1..5)
    ScanNearbyLoot,      // refresh the loot-intel snapshot
};

struct Action
{
    ActionType type{};
    double value = 0.0;   // generic numeric payload (amount, hours, etc.)
    int quantity = 1;     // item / xp quantity / sub-selector
    std::string text;     // record id, weather id, item id, fact name, etc.
};

// Snapshot of live game data shown in the UI footer / dashboard.
struct DisplayInfo
{
    bool gameLoaded = false;
    bool playerValid = false;
    float health = 0.0f;
    float healthMax = 0.0f;
    float stamina = 0.0f;
    float staminaMax = 0.0f;
    float ram = 0.0f;       // quickhack memory pool
    float ramMax = 0.0f;
    int money = 0;
    int level = 0;
    int streetCred = 0;
    int wantedLevel = 0;    // current heat (0..5), -1 if unreadable
    float speedKmh = 0.0f;  // vehicle speed when mounted
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    bool inVehicle = false;
};

// Rolling history for the Dashboard sparklines (health / RAM / heat over time).
// Single-writer (game thread) / single-reader (UI) - copied under a mutex.
struct HistoryRing
{
    static constexpr int kSize = 120; // ~last 60s at 2 Hz sampling
    float health[kSize] = {};
    float ram[kSize] = {};
    int head = 0;
    int count = 0;
    void Push(float h, float r)
    {
        health[head] = h;
        ram[head] = r;
        head = (head + 1) % kSize;
        if (count < kSize) ++count;
    }
};

// Install / binding self-check, surfaced in the Diagnostics panel + log.
struct BindingCheck
{
    std::string name;
    bool ok = false;
};

struct Diagnostics
{
    bool loaded = false;       // plugin reached Main(Load)
    bool dxHookOk = false;     // DX12 overlay hook installed
    bool gameStateOk = false;  // Running game-state registered
    std::string gameVersion;   // game runtime version reported by RED4ext
    bool selfTestRan = false;  // RTTI binding probe completed (needs in-world)
    int okCount = 0;
    int total = 0;
    std::vector<BindingCheck> checks;
};

class State
{
public:
    static State& Get();

    // Player (continuous)
    std::atomic<bool> godMode{false};
    std::atomic<bool> infiniteStamina{false};
    std::atomic<bool> infiniteOxygen{false};
    std::atomic<bool> infiniteMemory{false}; // quickhack RAM
    std::atomic<bool> noFallDamage{false};
    std::atomic<bool> noRagdoll{false};       // immune to knockdown/ragdoll
    std::atomic<bool> moveSpeedEnabled{false};
    std::atomic<float> moveSpeedMult{1.0f};
    std::atomic<bool> carryCapacityEnabled{false};
    std::atomic<float> carryCapacity{500.0f};
    std::atomic<bool> superJumpEnabled{false};
    std::atomic<float> superJumpMult{2.5f};      // JumpHeight multiplier
    std::atomic<bool> healthRegenBoost{false};   // fast in/out-of-combat regen
    std::atomic<bool> noClipEnabled{false};      // free-fly through geometry
    std::atomic<float> noClipSpeed{0.5f};        // world units per frame

    // Ghost Ops (continuous)
    std::atomic<bool> undetectable{false};      // suppress detection escalation
    std::atomic<bool> alertFreeze{false};       // lock current alert state
    std::atomic<bool> opticalCamoInfinite{false};

    // Heat & Law (continuous)
    std::atomic<bool> heatFreeze{false};        // no escalation / decay
    std::atomic<bool> holdWantedLevel{false};   // continuously pin wanted to target
    std::atomic<int> wantedLevelTarget{0};      // 0 = never wanted (police off)

    // Netrunner (continuous)
    std::atomic<bool> quickhackCostZero{false};
    std::atomic<bool> quickhackNoCooldown{false};
    std::atomic<bool> ramRegenBoost{false};

    // Combat sandbox (continuous)
    std::atomic<bool> damageMultEnabled{false};
    std::atomic<float> damageMult{2.0f};
    std::atomic<bool> oneHitKill{false};
    std::atomic<bool> infiniteAmmoNoReload{false};
    std::atomic<bool> noRecoil{false};
    std::atomic<bool> noSpread{false};
    std::atomic<bool> sandevistanInfinite{false};
    std::atomic<bool> infiniteGrenades{false};
    std::atomic<bool> infiniteProjectiles{false};
    std::atomic<bool> infiniteHeals{false};      // inhalers / health items
    std::atomic<bool> berserkNoCooldown{false};
    std::atomic<bool> kerenzikovNoCooldown{false};
    std::atomic<bool> overclockNoCooldown{false};

    // Combat buffs (continuous, player/weapon stat modifiers)
    std::atomic<bool> critChanceEnabled{false};
    std::atomic<float> critChanceMult{1.0f};        // additive to crit chance (0..1)
    std::atomic<bool> critDamageEnabled{false};
    std::atomic<float> critDamageMult{2.0f};        // additive to crit-damage multiplier
    std::atomic<bool> headshotDamageEnabled{false};
    std::atomic<float> headshotDamageMult{2.0f};    // additive to headshot multiplier
    std::atomic<bool> armorPenEnabled{false};       // CanWeaponIgnoreArmor
    std::atomic<bool> techChargeFast{false};        // weapon ChargeTime x0 (instant tech charge)
    std::atomic<bool> rapidFireEnabled{false};      // scale CycleTime/ShotDelay down
    std::atomic<float> rapidFireMult{4.0f};         // fire-rate multiplier (x faster)

    // City Director (continuous)
    std::atomic<bool> pedDensityEnabled{false};
    std::atomic<float> pedDensityMult{1.0f};
    std::atomic<bool> trafficDensityEnabled{false};
    std::atomic<float> trafficDensityMult{1.0f};

    // Loot & economy (continuous)
    std::atomic<bool> freeShopping{false};
    std::atomic<bool> sellMultEnabled{false};
    std::atomic<float> sellMult{2.0f};

    // Photo / world (continuous)
    std::atomic<bool> photoFreeze{false};       // freeze world time + hide HUD

    // Vehicles (continuous)
    std::atomic<bool> vehicleGodMode{false};
    std::atomic<bool> vehicleBoost{false};
    std::atomic<int> handlingPreset{0};         // 0 stock, 1 arcade, 2 sim
    std::string spawnVehicleRecord = "Vehicle.v_standard2_archer_quartz_player";

    // Buff Lab / Puppet inputs (read by game thread)
    std::string statusEffectId = "BaseStatusEffect.Berserk";
    std::string npcRecordId = "Character.afterlife_bouncer";

    // World & visuals (continuous)
    std::atomic<bool> freezeTime{false};
    std::atomic<bool> timeScaleEnabled{false};
    std::atomic<float> timeScale{1.0f};
    std::atomic<bool> fovEnabled{false};
    std::atomic<float> fov{80.0f};
    std::atomic<bool> autoSkipCutscenes{false}; // auto-skip cutscenes as they play
    std::string weatherId = "24h_weather_sunny";

    // UI-side persisted prefs that the game thread also reads
    std::atomic<int> giveItemQty{1};
    std::string giveItemId = "Items.money";

    // Menu visibility (set by UI thread, read by game thread to skip polling).
    std::atomic<bool> uiOpen{false};

    // Action queue
    void Push(const Action& a);
    std::vector<Action> Drain();

    // Display snapshot
    void SetDisplay(const DisplayInfo& d);
    DisplayInfo GetDisplay();

    // Diagnostics
    void SetDiagnostics(const Diagnostics& d);
    Diagnostics GetDiagnostics();

    // Sparkline history (health / RAM)
    void PushHistory(float health, float ram);
    HistoryRing GetHistory();

    // Saved teleport locations (3 slots)
    struct SavedLocation
    {
        bool valid = false;
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };
    static constexpr int kLocationSlots = 3;
    void SetLocation(int slot, const SavedLocation& loc);
    SavedLocation GetLocation(int slot);

private:
    State() = default;

    std::mutex m_queueMutex;
    std::vector<Action> m_queue;

    std::mutex m_dispMutex;
    DisplayInfo m_display;

    std::mutex m_diagMutex;
    Diagnostics m_diag;

    std::mutex m_histMutex;
    HistoryRing m_history;

    std::mutex m_locMutex;
    SavedLocation m_locations[kLocationSlots];
};
} // namespace cm
