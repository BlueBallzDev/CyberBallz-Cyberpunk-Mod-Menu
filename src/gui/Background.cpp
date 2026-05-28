#include "Background.hpp"
#include "core/Logger.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_GIF
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include <stb_image.h>

#include <fstream>
#include <mutex>

namespace cm::gui::bg
{
namespace
{
struct Data
{
    std::vector<unsigned char> pixels; // frameCount * w * h * 4
    std::vector<int> delaysMs;         // per-frame, ms
    int width = 0;
    int height = 0;
    int frameCount = 0;

    int current = 0;
    float accumMs = 0.0f;

    std::vector<ImTextureID> textures;
    unsigned generation = 0;
};

Data g;
std::mutex g_mutex;

std::vector<unsigned char> ReadFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
        return {};
    const std::streamsize size = f.tellg();
    if (size <= 0)
        return {};
    f.seekg(0);
    std::vector<unsigned char> buf(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(buf.data()), size);
    return buf;
}
} // namespace

bool Load(const std::string& path)
{
    std::scoped_lock lock(g_mutex);

    const std::vector<unsigned char> file = ReadFile(path);
    if (file.empty())
    {
        CM_WARN("Background: cannot read %s", path.c_str());
        return false;
    }

    g.pixels.clear();
    g.delaysMs.clear();
    g.textures.clear();
    g.current = 0;
    g.accumMs = 0.0f;

    const bool isGif = path.size() >= 4 &&
                       (path[path.size() - 3] == 'g' || path[path.size() - 3] == 'G') &&
                       (path[path.size() - 2] == 'i' || path[path.size() - 2] == 'I');

    int w = 0, h = 0, comp = 0;
    if (isGif)
    {
        int* delays = nullptr;
        int frames = 0;
        stbi_uc* data = stbi_load_gif_from_memory(file.data(), static_cast<int>(file.size()), &delays, &w, &h,
                                                  &frames, &comp, 4);
        if (!data || frames <= 0)
        {
            if (data)
                stbi_image_free(data);
            CM_WARN("Background: GIF decode failed %s", path.c_str());
            return false;
        }
        g.width = w;
        g.height = h;
        g.frameCount = frames;
        const size_t total = static_cast<size_t>(w) * h * 4 * frames;
        g.pixels.assign(data, data + total);
        g.delaysMs.resize(frames);
        for (int i = 0; i < frames; ++i)
            g.delaysMs[i] = (delays && delays[i] > 0) ? delays[i] : 80; // sane default cadence
        stbi_image_free(data);
        if (delays)
            free(delays);
    }
    else
    {
        stbi_uc* data = stbi_load_from_memory(file.data(), static_cast<int>(file.size()), &w, &h, &comp, 4);
        if (!data)
        {
            CM_WARN("Background: image decode failed %s", path.c_str());
            return false;
        }
        g.width = w;
        g.height = h;
        g.frameCount = 1;
        g.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
        g.delaysMs.assign(1, 0);
        stbi_image_free(data);
    }

    ++g.generation;
    CM_INFO("Background loaded: %s (%dx%d, %d frame%s)", path.c_str(), g.width, g.height, g.frameCount,
            g.frameCount == 1 ? "" : "s");
    return true;
}

void Unload()
{
    std::scoped_lock lock(g_mutex);
    g.pixels.clear();
    g.delaysMs.clear();
    g.textures.clear();
    g.width = g.height = g.frameCount = 0;
    g.current = 0;
    g.accumMs = 0.0f;
    ++g.generation;
}

bool Valid()
{
    std::scoped_lock lock(g_mutex);
    return g.frameCount > 0 && g.width > 0 && g.height > 0;
}

int Width()
{
    std::scoped_lock lock(g_mutex);
    return g.width;
}

int Height()
{
    std::scoped_lock lock(g_mutex);
    return g.height;
}

int FrameCount()
{
    std::scoped_lock lock(g_mutex);
    return g.frameCount;
}

const unsigned char* FramePixels(int frame)
{
    std::scoped_lock lock(g_mutex);
    if (frame < 0 || frame >= g.frameCount)
        return nullptr;
    return g.pixels.data() + static_cast<size_t>(frame) * g.width * g.height * 4;
}

void Advance(float dtSeconds)
{
    std::scoped_lock lock(g_mutex);
    if (g.frameCount <= 1)
        return;
    g.accumMs += dtSeconds * 1000.0f;
    int guard = 0;
    while (g.accumMs >= static_cast<float>(g.delaysMs[g.current]) && guard++ < g.frameCount)
    {
        g.accumMs -= static_cast<float>(g.delaysMs[g.current]);
        g.current = (g.current + 1) % g.frameCount;
    }
}

int CurrentFrame()
{
    std::scoped_lock lock(g_mutex);
    return g.current;
}

unsigned Generation()
{
    std::scoped_lock lock(g_mutex);
    return g.generation;
}

void SetTextures(std::vector<ImTextureID> textures)
{
    std::scoped_lock lock(g_mutex);
    g.textures = std::move(textures);
}

bool TexturesReady()
{
    std::scoped_lock lock(g_mutex);
    return !g.textures.empty() && static_cast<int>(g.textures.size()) == g.frameCount;
}

ImTextureID CurrentTexture()
{
    std::scoped_lock lock(g_mutex);
    if (g.textures.empty())
        return 0;
    const int idx = g.current < static_cast<int>(g.textures.size()) ? g.current : 0;
    return g.textures[idx];
}
} // namespace cm::gui::bg
