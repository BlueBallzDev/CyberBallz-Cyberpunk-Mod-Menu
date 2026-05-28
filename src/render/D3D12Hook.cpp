#include "D3D12Hook.hpp"
#include "Renderer.hpp"
#include "gui/Menu.hpp"
#include "core/Logger.hpp"
#include "core/DebugConsole.hpp"

#include <MinHook.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace cm::render
{
namespace
{
// IDXGISwapChain / ID3D12CommandQueue vtable indices (stable COM layout).
constexpr int kPresentIndex = 8;
constexpr int kResizeBuffersIndex = 13;
constexpr int kExecuteCommandListsIndex = 10;

using Present_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT);
using ResizeBuffers_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ExecuteCommandLists_t = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

Present_t g_origPresent = nullptr;
ResizeBuffers_t g_origResizeBuffers = nullptr;
ExecuteCommandLists_t g_origExecuteCommandLists = nullptr;

void* g_presentTarget = nullptr;
void* g_resizeTarget = nullptr;
void* g_executeTarget = nullptr;

// Cursor freedom: the game locks/recenters the mouse for camera control. While
// the menu is open we neutralize those calls so the cursor moves freely.
using SetCursorPos_t = BOOL(WINAPI*)(int, int);
using ClipCursor_t = BOOL(WINAPI*)(const RECT*);
SetCursorPos_t g_origSetCursorPos = nullptr;
ClipCursor_t g_origClipCursor = nullptr;

BOOL WINAPI HookedSetCursorPos(int x, int y)
{
    if (cm::gui::Menu::Get().IsOpen() || cm::debug::IsOpen())
        return TRUE; // swallow the game's per-frame recenter so the cursor stays put
    return g_origSetCursorPos(x, y);
}

BOOL WINAPI HookedClipCursor(const RECT* rect)
{
    if (cm::gui::Menu::Get().IsOpen() || cm::debug::IsOpen())
        return g_origClipCursor(nullptr); // release any clip so the cursor can roam
    return g_origClipCursor(rect);
}

// SEH wrappers: a fault anywhere in our overlay code is caught here so it can
// never crash the game. On a fault the overlay disables itself. These wrappers
// hold no C++ objects (required to mix __try with /EHsc).
void SafeRender(IDXGISwapChain3* swapChain)
{
    __try
    {
        Renderer::Get().Render(swapChain);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Renderer::Get().NotifyRenderFault();
    }
}

void SafeSetQueue(ID3D12CommandQueue* queue)
{
    __try
    {
        Renderer::Get().SetCommandQueue(queue);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void SafeResize()
{
    __try
    {
        Renderer::Get().OnResize();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Renderer::Get().NotifyRenderFault();
    }
}

HRESULT STDMETHODCALLTYPE HookedPresent(IDXGISwapChain3* swapChain, UINT syncInterval, UINT flags)
{
    static bool s_first = true;
    if (s_first)
    {
        s_first = false;
        CM_INFO("hook: first Present call (swapChain=%p)", (void*)swapChain);
    }
    SafeRender(swapChain);
    return g_origPresent(swapChain, syncInterval, flags);
}

HRESULT STDMETHODCALLTYPE HookedResizeBuffers(IDXGISwapChain3* swapChain, UINT bufferCount, UINT width,
                                              UINT height, DXGI_FORMAT newFormat, UINT flags)
{
    SafeResize();
    return g_origResizeBuffers(swapChain, bufferCount, width, height, newFormat, flags);
}

void STDMETHODCALLTYPE HookedExecuteCommandLists(ID3D12CommandQueue* queue, UINT numLists,
                                                 ID3D12CommandList* const* lists)
{
    static bool s_first = true;
    if (s_first)
    {
        s_first = false;
        CM_INFO("hook: first ExecuteCommandLists call (queue=%p)", (void*)queue);
    }
    SafeSetQueue(queue);
    g_origExecuteCommandLists(queue, numLists, lists);
}

// Spin up a hidden device + swapchain just long enough to read the vtables.
bool ResolveVTableTargets()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"CyberBallzDummyWnd";
    if (!RegisterClassExW(&wc))
        return false;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr,
                                nullptr, wc.hInstance, nullptr);
    if (!hwnd)
    {
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    bool ok = false;
    {
        ComPtr<ID3D12Device> device;
        ComPtr<ID3D12CommandQueue> queue;
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGISwapChain1> swapChain1;
        ComPtr<IDXGISwapChain3> swapChain;

        if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
        {
            D3D12_COMMAND_QUEUE_DESC qd{};
            qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            if (SUCCEEDED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue))) &&
                SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            {
                DXGI_SWAP_CHAIN_DESC1 scd{};
                scd.BufferCount = 2;
                scd.Width = 100;
                scd.Height = 100;
                scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                scd.SampleDesc.Count = 1;
                scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

                if (SUCCEEDED(factory->CreateSwapChainForHwnd(queue.Get(), hwnd, &scd, nullptr, nullptr,
                                                              &swapChain1)) &&
                    SUCCEEDED(swapChain1.As(&swapChain)))
                {
                    void** scVtbl = *reinterpret_cast<void***>(swapChain.Get());
                    void** cqVtbl = *reinterpret_cast<void***>(queue.Get());
                    g_presentTarget = scVtbl[kPresentIndex];
                    g_resizeTarget = scVtbl[kResizeBuffersIndex];
                    g_executeTarget = cqVtbl[kExecuteCommandListsIndex];
                    ok = true;
                }
            }
        }
    }

    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return ok;
}
} // namespace

bool InstallHooks()
{
    if (MH_Initialize() != MH_OK)
    {
        CM_ERROR("MinHook initialize failed");
        return false;
    }

    if (!ResolveVTableTargets())
    {
        CM_ERROR("Failed to resolve DX12 vtable targets");
        return false;
    }

    auto create = [](void* target, void* detour, void** orig, const char* name) -> bool
    {
        if (MH_CreateHook(target, detour, orig) != MH_OK)
        {
            CM_ERROR("MH_CreateHook failed for %s", name);
            return false;
        }
        return true;
    };

    bool ok = create(g_presentTarget, &HookedPresent, reinterpret_cast<void**>(&g_origPresent), "Present");
    ok &= create(g_resizeTarget, &HookedResizeBuffers, reinterpret_cast<void**>(&g_origResizeBuffers),
                 "ResizeBuffers");
    ok &= create(g_executeTarget, &HookedExecuteCommandLists,
                 reinterpret_cast<void**>(&g_origExecuteCommandLists), "ExecuteCommandLists");

    // Cursor freedom hooks (user32). Non-fatal if they fail.
    create(reinterpret_cast<void*>(&SetCursorPos), &HookedSetCursorPos,
           reinterpret_cast<void**>(&g_origSetCursorPos), "SetCursorPos");
    create(reinterpret_cast<void*>(&ClipCursor), &HookedClipCursor,
           reinterpret_cast<void**>(&g_origClipCursor), "ClipCursor");

    if (!ok || MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
    {
        CM_ERROR("Failed to enable DX12 hooks");
        return false;
    }

    CM_INFO("DX12 hooks installed (+cursor freedom)");
    return true;
}

void RemoveHooks()
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    Renderer::Get().Shutdown();
    CM_INFO("DX12 hooks removed");
}
} // namespace cm::render
