#pragma once
#include <imgui.h>

#include <string>
#include <vector>

// Backend-agnostic background image state. The CPU side decodes the image
// (animated GIF or static PNG/JPG) and tracks playback; the render backend
// (DX12 in-game, DX11 in the preview) uploads frames to GPU textures and
// registers their ImTextureIDs here. The menu draws bg::CurrentTexture().
namespace cm::gui::bg
{
// Decode an image file. Returns false on failure. Increments Generation().
bool Load(const std::string& path);
void Unload();

bool Valid();
int Width();
int Height();
int FrameCount();

// Pixels for frame i (RGBA8, Width*Height*4). nullptr if out of range.
const unsigned char* FramePixels(int frame);

// Advance playback by dt seconds; updates the current frame index.
void Advance(float dtSeconds);
int CurrentFrame();

// A counter bumped on every Load/Unload so a backend can detect when it must
// (re)create its GPU textures.
unsigned Generation();

// Backend registers one ImTextureID per frame (in order). Cleared on Unload.
void SetTextures(std::vector<ImTextureID> textures);
bool TexturesReady();
ImTextureID CurrentTexture(); // 0 if not uploaded yet
} // namespace cm::gui::bg
