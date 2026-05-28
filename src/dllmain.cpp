#include <RED4ext/RED4ext.hpp>

#include "core/Logger.hpp"
#include "core/Config.hpp"
#include "core/State.hpp"
#include "render/D3D12Hook.hpp"
#include "gui/Menu.hpp"
#include "game/Features.hpp"

#include <Windows.h>
#include <cstdio>

namespace
{
RED4ext::v1::PluginHandle g_handle = nullptr;

// Running game-state hooks (run on the main game thread)
bool OnEnter(RED4ext::CGameApplication*)
{
    return true;
}

void SafeGameUpdate()
{
    __try
    {
        cm::features::OnGameUpdate();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // A faulting game binding must not crash the game.
    }
}

bool OnUpdate(RED4ext::CGameApplication*)
{
    SafeGameUpdate();
    // Return false so the "Running" state keeps calling us every frame. Returning
    // true tells RED4ext the state is finished and it stops invoking OnUpdate.
    return false;
}

bool OnExit(RED4ext::CGameApplication*)
{
    return true;
}

RED4ext::v1::GameState g_runningState{&OnEnter, &OnUpdate, &OnExit};
} // namespace

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle, RED4ext::v1::EMainReason aReason,
                                        const RED4ext::v1::Sdk* aSdk)
{
    switch (aReason)
    {
    case RED4ext::v1::EMainReason::Load:
    {
        g_handle = aHandle;
        cm::log::Init(aHandle, aSdk);

        cm::config::Load();
        if (cm::config::Prefs().startOpen)
            cm::gui::Menu::Get().Open(true);

        // Run features on the main thread every frame.
        const bool gameStateOk =
            aSdk && aSdk->gameStates &&
            aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &g_runningState);

        // Render thread: install the DX12 overlay hooks (skippable via config
        // for safe-mode / crash isolation).
        bool hookOk = false;
        if (cm::config::Prefs().overlayEnabled)
        {
            hookOk = cm::render::InstallHooks();
            if (!hookOk)
                CM_ERROR("Failed to install DX12 hooks - overlay will not render");
        }
        else
        {
            CM_INFO("Overlay disabled by config (overlayEnabled=0) - running in safe mode");
        }

        CM_INFO("post-hook: InstallHooks returned %d, building diagnostics", hookOk ? 1 : 0);

        // Install self-check (logged so the install can be verified)
        cm::Diagnostics diag;
        diag.loaded = true;
        diag.dxHookOk = hookOk;
        diag.gameStateOk = gameStateOk;
        if (aSdk && aSdk->runtime)
        {
            char ver[64];
            std::snprintf(ver, sizeof(ver), "%u.%u.%u", aSdk->runtime->major, aSdk->runtime->minor,
                          aSdk->runtime->patch);
            diag.gameVersion = ver;
        }
        CM_INFO("post-hook: setting diagnostics snapshot");
        cm::State::Get().SetDiagnostics(diag);
        CM_INFO("post-hook: diagnostics set");

        CM_INFO("==================== CyberBallz install check ====================");
        CM_INFO("Plugin loaded by RED4ext       : YES");
        CM_INFO("Game runtime version           : %s", diag.gameVersion.empty() ? "unknown" : diag.gameVersion.c_str());
        CM_INFO("DX12 overlay hook              : %s", hookOk ? "installed" : "FAILED");
        CM_INFO("Game-thread update registered  : %s", gameStateOk ? "yes" : "FAILED");
        CM_INFO("Toggle key (VK)                : 0x%02X", cm::config::Prefs().toggleKey);
        CM_INFO("Binding self-test runs once you load into the world.");
        CM_INFO("=================================================================");
        break;
    }
    case RED4ext::v1::EMainReason::Unload:
    {
        CM_INFO("CyberBallz unloading...");
        cm::config::Save();
        cm::render::RemoveHooks();
        cm::log::Shutdown();
        break;
    }
    }

    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
    aInfo->name = L"CyberBallz";
    aInfo->author = L"CyberBallz";
    aInfo->version = RED4EXT_V1_SEMVER(1, 0, 0);
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
