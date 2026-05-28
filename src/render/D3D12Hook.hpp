#pragma once

// Installs/removes the DirectX 12 hooks used to render the overlay. Uses a
// throwaway device + swapchain to read the IDXGISwapChain / ID3D12CommandQueue
// vtables, then detours Present, ResizeBuffers and ExecuteCommandLists.
namespace cm::render
{
bool InstallHooks();
void RemoveHooks();
} // namespace cm::render
