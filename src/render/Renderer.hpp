#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <atomic>
#include <vector>

// Owns the ImGui DX12 + Win32 backends and renders the overlay each frame from
// inside the hooked IDXGISwapChain::Present. The graphics command queue is
// captured separately from the ID3D12CommandQueue::ExecuteCommandLists hook.
namespace cm::render
{
class Renderer
{
public:
    static Renderer& Get();

    // Called from the ExecuteCommandLists hook to capture the direct queue.
    void SetCommandQueue(ID3D12CommandQueue* queue);

    // Called from the Present hook every frame.
    void Render(IDXGISwapChain3* swapChain);

    // Called from the ResizeBuffers hook before the original runs.
    void OnResize();

    // Disable the overlay after a caught exception so it can't crash the game.
    void NotifyRenderFault();

    void Shutdown();

private:
    struct FrameContext
    {
        ID3D12CommandAllocator* allocator = nullptr;
        ID3D12Resource* backBuffer = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    };

    bool CreateDeviceResources(IDXGISwapChain3* swapChain);
    void CreateRenderTargets(IDXGISwapChain3* swapChain);
    void ReleaseRenderTargets();

    // Themed background image (single texture, updated per animation frame).
    void EnsureBackground();
    void UpdateBackground();
    void ReleaseBackground();

    std::atomic<ID3D12CommandQueue*> m_commandQueue{nullptr};

    bool m_initialized = false;
    bool m_renderTargetsValid = false;
    bool m_renderFailed = false; // set after a caught fault; disables the overlay

    ID3D12Device* m_device = nullptr;
    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    ID3D12DescriptorHeap* m_srvHeap = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;
    std::vector<FrameContext> m_frames;
    UINT m_bufferCount = 0;
    DXGI_FORMAT m_rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    HWND m_hwnd = nullptr;

    // Background image GPU resources.
    ID3D12Resource* m_bgTexture = nullptr;
    std::vector<ID3D12Resource*> m_bgUploads;
    UINT m_srvIncrement = 0;
    int m_bgWidth = 0;
    int m_bgHeight = 0;
    unsigned m_bgGeneration = 0xFFFFFFFFu;
    int m_bgLastFrame = -1;
    int m_bgUploadRing = 0;
    D3D12_RESOURCE_STATES m_bgState = D3D12_RESOURCE_STATE_COPY_DEST;
};
} // namespace cm::render
