#include "Renderer.hpp"
#include "WndProc.hpp"
#include "gui/Menu.hpp"
#include "gui/Theme.hpp"
#include "gui/ThemeIO.hpp"
#include "gui/Background.hpp"
#include "gui/Notifications.hpp"
#include "gui/Console.hpp"
#include "core/Logger.hpp"
#include "core/Config.hpp"
#include "core/DebugConsole.hpp"

#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include <cstring>

namespace cm::render
{
namespace
{
template <typename T>
void SafeRelease(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}
} // namespace

Renderer& Renderer::Get()
{
    static Renderer instance;
    return instance;
}

void Renderer::SetCommandQueue(ID3D12CommandQueue* queue)
{
    if (!queue || m_commandQueue.load())
        return;
    // Only the DIRECT queue is valid for presenting our overlay.
    if (queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
        m_commandQueue.store(queue);
}

bool Renderer::CreateDeviceResources(IDXGISwapChain3* swapChain)
{
    CM_INFO("CDR: entered (swapChain=%p)", (void*)swapChain);
    if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&m_device))))
    {
        CM_ERROR("Renderer: failed to get D3D12 device from swapchain");
        return false;
    }
    CM_INFO("CDR: got device");

    DXGI_SWAP_CHAIN_DESC desc{};
    swapChain->GetDesc(&desc);
    m_bufferCount = desc.BufferCount;
    m_hwnd = desc.OutputWindow;
    m_rtvFormat = desc.BufferDesc.Format;
    if (m_rtvFormat == DXGI_FORMAT_UNKNOWN)
        m_rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    CM_INFO("Renderer init: device ok, %u buffers, hwnd=%p, fmt=%d", m_bufferCount, (void*)m_hwnd,
            (int)m_rtvFormat);

    // SRV heap: descriptor 0 = ImGui font, descriptor 1 = themed background.
    {
        D3D12_DESCRIPTOR_HEAP_DESC d{};
        d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        d.NumDescriptors = 2;
        d.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(m_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&m_srvHeap))))
        {
            CM_ERROR("Renderer: failed to create SRV heap");
            return false;
        }
        m_srvIncrement = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // RTV heap, one per back buffer.
    {
        D3D12_DESCRIPTOR_HEAP_DESC d{};
        d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        d.NumDescriptors = m_bufferCount;
        d.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(m_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&m_rtvHeap))))
        {
            CM_ERROR("Renderer: failed to create RTV heap");
            return false;
        }
    }

    // Per-frame command allocators + a shared command list.
    m_frames.clear();
    m_frames.resize(m_bufferCount);
    for (UINT i = 0; i < m_bufferCount; ++i)
    {
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    IID_PPV_ARGS(&m_frames[i].allocator))))
        {
            CM_ERROR("Renderer: failed to create command allocator %u", i);
            return false;
        }
    }

    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_frames[0].allocator, nullptr,
                                           IID_PPV_ARGS(&m_commandList))))
    {
        CM_ERROR("Renderer: failed to create command list");
        return false;
    }
    m_commandList->Close();
    CM_INFO("Renderer init: heaps + command list ok");

    CreateRenderTargets(swapChain);
    CM_INFO("Renderer init: render targets ok");

    // ImGui setup.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // don't write imgui.ini into the game folder
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    cm::gui::theme::Apply();
    CM_INFO("Renderer init: theme/fonts applied");
    cm::gui::themeio::ApplyConfiguredTheme();
    CM_INFO("Renderer init: configured theme applied");
    if (cm::config::Prefs().animatedBackground && cm::gui::theme::IsThemeLoaded() &&
        !cm::gui::theme::BackgroundPath().empty())
    {
        cm::gui::bg::Load(cm::gui::theme::BackgroundPath());
        CM_INFO("Renderer init: background loaded");
    }

    if (!ImGui_ImplWin32_Init(m_hwnd))
    {
        CM_ERROR("Renderer: ImGui_ImplWin32_Init failed");
        return false;
    }
    CM_INFO("Renderer init: ImGui Win32 ok");

    const auto srvCpu = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    const auto srvGpu = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    if (!ImGui_ImplDX12_Init(m_device, m_bufferCount, m_rtvFormat, m_srvHeap, srvCpu, srvGpu))
    {
        CM_ERROR("Renderer: ImGui_ImplDX12_Init failed");
        return false;
    }

    input::Install(m_hwnd);

    CM_INFO("Renderer initialized (%u buffers, hwnd=%p)", m_bufferCount, (void*)m_hwnd);
    return true;
}

void Renderer::CreateRenderTargets(IDXGISwapChain3* swapChain)
{
    const UINT rtvSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < m_bufferCount; ++i)
    {
        ID3D12Resource* buffer = nullptr;
        swapChain->GetBuffer(i, IID_PPV_ARGS(&buffer));
        if (buffer)
        {
            m_device->CreateRenderTargetView(buffer, nullptr, handle);
            m_frames[i].backBuffer = buffer;
            m_frames[i].rtv = handle;
        }
        handle.ptr += rtvSize;
    }
    m_renderTargetsValid = true;
}

void Renderer::ReleaseRenderTargets()
{
    for (auto& f : m_frames)
        SafeRelease(f.backBuffer);
    m_renderTargetsValid = false;
}

void Renderer::OnResize()
{
    // Must drop references to the back buffers before ResizeBuffers runs.
    if (m_initialized)
        ReleaseRenderTargets();
}

namespace
{
UINT Align256(UINT v)
{
    return (v + 255u) & ~255u;
}
} // namespace

void Renderer::ReleaseBackground()
{
    SafeRelease(m_bgTexture);
    for (auto* u : m_bgUploads)
        if (u)
            u->Release();
    m_bgUploads.clear();
    m_bgGeneration = 0xFFFFFFFFu;
    m_bgLastFrame = -1;
    m_bgUploadRing = 0;
    m_bgState = D3D12_RESOURCE_STATE_COPY_DEST;
    cm::gui::bg::SetTextures({});
}

void Renderer::EnsureBackground()
{
    const bool want = cm::config::Prefs().animatedBackground && cm::gui::bg::Valid();
    if (!want)
    {
        if (m_bgTexture)
            ReleaseBackground();
        return;
    }
    if (m_bgTexture && cm::gui::bg::Generation() == m_bgGeneration)
        return;

    ReleaseBackground();
    m_bgGeneration = cm::gui::bg::Generation();
    m_bgWidth = cm::gui::bg::Width();
    m_bgHeight = cm::gui::bg::Height();
    if (m_bgWidth <= 0 || m_bgHeight <= 0)
        return;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = static_cast<UINT64>(m_bgWidth);
    td.Height = static_cast<UINT>(m_bgHeight);
    td.DepthOrArraySize = 1;
    td.MipLevels = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    if (FAILED(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &td,
                                                 D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                 IID_PPV_ARGS(&m_bgTexture))))
    {
        CM_WARN("Background: failed to create texture");
        return;
    }
    m_bgState = D3D12_RESOURCE_STATE_COPY_DEST;

    // SRV at descriptor index 1.
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += m_srvIncrement;
    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(m_bgTexture, &sd, cpu);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += m_srvIncrement;
    cm::gui::bg::SetTextures({static_cast<ImTextureID>(gpu.ptr)});

    // Ring of upload buffers (one frame each) to avoid CPU/GPU overwrite.
    const UINT rowPitch = Align256(static_cast<UINT>(m_bgWidth) * 4);
    const UINT64 uploadSize = static_cast<UINT64>(rowPitch) * m_bgHeight;
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC ud{};
    ud.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    ud.Width = uploadSize;
    ud.Height = 1;
    ud.DepthOrArraySize = 1;
    ud.MipLevels = 1;
    ud.Format = DXGI_FORMAT_UNKNOWN;
    ud.SampleDesc.Count = 1;
    ud.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    const int ring = m_bufferCount > 1 ? static_cast<int>(m_bufferCount) : 2;
    for (int i = 0; i < ring; ++i)
    {
        ID3D12Resource* buf = nullptr;
        if (SUCCEEDED(m_device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &ud,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                        IID_PPV_ARGS(&buf))))
            m_bgUploads.push_back(buf);
    }
    m_bgLastFrame = -1;
    m_bgUploadRing = 0;
    CM_INFO("Background GPU texture ready (%dx%d)", m_bgWidth, m_bgHeight);
}

void Renderer::UpdateBackground()
{
    if (!m_bgTexture || m_bgUploads.empty() || !cm::gui::bg::Valid())
        return;
    const int cur = cm::gui::bg::CurrentFrame();
    if (cur == m_bgLastFrame)
        return;
    const unsigned char* px = cm::gui::bg::FramePixels(cur);
    if (!px)
        return;

    ID3D12Resource* up = m_bgUploads[m_bgUploadRing];
    m_bgUploadRing = (m_bgUploadRing + 1) % static_cast<int>(m_bgUploads.size());

    const UINT rowPitch = Align256(static_cast<UINT>(m_bgWidth) * 4);
    void* mapped = nullptr;
    D3D12_RANGE noRead{0, 0};
    if (FAILED(up->Map(0, &noRead, &mapped)))
        return;
    for (int y = 0; y < m_bgHeight; ++y)
        std::memcpy(static_cast<char*>(mapped) + static_cast<size_t>(y) * rowPitch,
                    px + static_cast<size_t>(y) * m_bgWidth * 4, static_cast<size_t>(m_bgWidth) * 4);
    up->Unmap(0, nullptr);

    auto barrier = [&](D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = m_bgTexture;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = before;
        b.Transition.StateAfter = after;
        m_commandList->ResourceBarrier(1, &b);
    };

    if (m_bgState != D3D12_RESOURCE_STATE_COPY_DEST)
        barrier(m_bgState, D3D12_RESOURCE_STATE_COPY_DEST);

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = m_bgTexture;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = up;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = 0;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    src.PlacedFootprint.Footprint.Width = static_cast<UINT>(m_bgWidth);
    src.PlacedFootprint.Footprint.Height = static_cast<UINT>(m_bgHeight);
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = rowPitch;
    m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    barrier(D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_bgState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    m_bgLastFrame = cur;
}

void Renderer::NotifyRenderFault()
{
    if (!m_renderFailed)
        CM_ERROR("Overlay render faulted - disabling overlay to keep the game stable. "
                 "See the last CreateDeviceResources step logged above.");
    m_renderFailed = true;
}

void Renderer::Render(IDXGISwapChain3* swapChain)
{
    if (m_renderFailed)
        return; // a previous frame faulted; stay disabled rather than crash

    static bool s_first = true;
    if (s_first)
    {
        s_first = false;
        CM_INFO("Render: first call");
    }

    ID3D12CommandQueue* queue = m_commandQueue.load();
    if (!queue)
        return; // wait until ExecuteCommandLists hook captures the queue

    // One-time init MUST run regardless of menu state - it installs the WndProc
    // input hook (the Insert toggle). Skipping it would mean the menu can never
    // be opened.
    if (!m_initialized)
    {
        if (!CreateDeviceResources(swapChain))
            return;
        m_initialized = true;
        CM_INFO("Renderer: first-frame initialization complete");
    }

    if (!m_renderTargetsValid)
        CreateRenderTargets(swapChain);

    // Idle fast-path: after init, when there is nothing to show, do no ImGui
    // work and submit no command list -> ~zero idle cost. The persistent HUD
    // (watermark/FPS) forces a lightweight frame when enabled.
    const bool hud = cm::config::Prefs().showWatermark || cm::config::Prefs().showFps;
    if (!cm::gui::Menu::Get().NeedsRender() && !cm::gui::notify::HasActive() && !hud &&
        !cm::debug::IsOpen())
        return;

    cm::gui::bg::Advance(ImGui::GetIO().DeltaTime);
    EnsureBackground();

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    // The idle fast-path skips frames, so the first frame after re-opening can
    // report a huge delta. Clamp it so animations/timers behave.
    if (io.DeltaTime > 0.1f)
        io.DeltaTime = 0.1f;
    else if (io.DeltaTime <= 0.0f)
        io.DeltaTime = 1.0f / 60.0f;

    // Robust cursor: feed mouse position + buttons straight from the OS while the
    // menu is open. CP2077's window/focus setup defeats ImGui's normal mouse
    // path (the swapchain HWND isn't the foreground window), so we override it.
    const bool open = cm::gui::Menu::Get().IsOpen();
    static bool s_wasOpen = false;
    if (open && m_hwnd)
    {
        io.MouseDrawCursor = true;
        if (!s_wasOpen)
        {
            // Just opened: drop the cursor in the screen centre so it's visible
            // immediately instead of wherever the (hidden) OS cursor last sat.
            io.AddMousePosEvent(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        }
        else
        {
            POINT pt{};
            if (GetCursorPos(&pt))
            {
                ScreenToClient(m_hwnd, &pt);
                io.AddMousePosEvent(static_cast<float>(pt.x), static_cast<float>(pt.y));
            }
        }
        io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
        io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
    }
    s_wasOpen = open;

    ImGui::NewFrame();

    cm::gui::Menu::Get().Draw();
    cm::gui::DrawDebugConsole();

    // Keep the mouse free while the menu is open: release any clip the game set
    // (the SetCursorPos hook handles recentering; this handles a one-time clip).
    if (cm::gui::Menu::Get().IsOpen())
        ClipCursor(nullptr);

    ImGui::Render();

    const UINT idx = swapChain->GetCurrentBackBufferIndex();
    if (idx >= m_frames.size() || !m_frames[idx].backBuffer)
        return;
    FrameContext& fc = m_frames[idx];

    fc.allocator->Reset();

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = fc.backBuffer;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    m_commandList->Reset(fc.allocator, nullptr);
    m_commandList->ResourceBarrier(1, &barrier);

    // Upload the current background frame (records into this command list before
    // the overlay draw that samples it).
    UpdateBackground();

    m_commandList->OMSetRenderTargets(1, &fc.rtv, FALSE, nullptr);
    m_commandList->SetDescriptorHeaps(1, &m_srvHeap);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);
    m_commandList->Close();

    ID3D12CommandList* lists[] = {m_commandList};
    queue->ExecuteCommandLists(1, lists);
}

void Renderer::Shutdown()
{
    if (!m_initialized)
        return;

    input::Uninstall();

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    ReleaseBackground();
    ReleaseRenderTargets();
    for (auto& f : m_frames)
        SafeRelease(f.allocator);
    m_frames.clear();
    SafeRelease(m_commandList);
    SafeRelease(m_rtvHeap);
    SafeRelease(m_srvHeap);
    SafeRelease(m_device);

    m_initialized = false;
    CM_INFO("Renderer shut down");
}
} // namespace cm::render
