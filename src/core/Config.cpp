#include "Config.hpp"

#include "State.hpp"
#include "Toggles.hpp"
#include "Logger.hpp"

#include <Windows.h>
#include <Shlobj.h>
#include <cstdio>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace cm::config
{
namespace
{
UiPrefs g_prefs;

std::wstring PathFor(const wchar_t* leaf)
{
    PWSTR path = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
    {
        out = path;
        out += L"\\CyberBallz";
        CreateDirectoryW(out.c_str(), nullptr);
        out += L'\\';
        out += leaf;
    }
    if (path)
        CoTaskMemFree(path);
    return out;
}
std::wstring ConfigPath() { return PathFor(L"config.ini"); }   // UI prefs
std::wstring LoadoutPath() { return PathFor(L"loadout.ini"); } // toggles + values

// Read a flat key=value file into a map (empty map if missing).
std::unordered_map<std::string, std::string> ReadKv(const std::wstring& path)
{
    std::unordered_map<std::string, std::string> kv;
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"r") != 0 || !fp)
        return kv;
    char buf[1024];
    while (std::fgets(buf, sizeof(buf), fp))
    {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
    std::fclose(fp);
    return kv;
}

template <typename T>
T GetOr(const std::unordered_map<std::string, std::string>& m, const char* key, T def)
{
    auto it = m.find(key);
    if (it == m.end())
        return def;
    std::istringstream ss(it->second);
    T v{};
    ss >> v;
    return ss.fail() ? def : v;
}

} // namespace

UiPrefs& Prefs()
{
    return g_prefs;
}

bool IsFavorite(const std::string& id)
{
    const std::string hay = "," + g_prefs.favorites + ",";
    return hay.find("," + id + ",") != std::string::npos;
}

void ToggleFavorite(const std::string& id)
{
    if (IsFavorite(id))
    {
        // Rebuild the list without `id`.
        std::string out;
        std::stringstream ss(g_prefs.favorites);
        std::string item;
        while (std::getline(ss, item, ','))
            if (!item.empty() && item != id)
                out += (out.empty() ? "" : ",") + item;
        g_prefs.favorites = out;
    }
    else
    {
        g_prefs.favorites += (g_prefs.favorites.empty() ? "" : ",") + id;
    }
    Save();
}

void Load()
{
    // UI preferences (config.ini)
    const auto kv = ReadKv(ConfigPath());
    g_prefs.toggleKey = GetOr(kv, "toggleKey", g_prefs.toggleKey);
    g_prefs.accentPreset = GetOr(kv, "accentPreset", g_prefs.accentPreset);
    g_prefs.uiScale = GetOr(kv, "uiScale", g_prefs.uiScale);
    g_prefs.startOpen = GetOr<int>(kv, "startOpen", 0) != 0;
    g_prefs.overlayEnabled = GetOr<int>(kv, "overlayEnabled", 1) != 0;
    g_prefs.showWatermark = GetOr<int>(kv, "showWatermark", 1) != 0;
    g_prefs.showFps = GetOr<int>(kv, "showFps", 1) != 0;
    g_prefs.showDiagnostics = GetOr<int>(kv, "showDiagnostics", 0) != 0;
    if (auto it = kv.find("themeDir"); it != kv.end())
        g_prefs.themeDir = it->second;
    if (auto it = kv.find("themeFile"); it != kv.end())
        g_prefs.themeFile = it->second;
    g_prefs.animatedBackground = GetOr<int>(kv, "animatedBackground", 1) != 0;
    if (auto it = kv.find("favorites"); it != kv.end())
        g_prefs.favorites = it->second;

    // Feature loadout (loadout.ini) - only present if the user saved one
    const auto lk = ReadKv(LoadoutPath());
    int restored = 0;
    for (const auto& t : toggles::All())
    {
        if (!t.flag)
            continue;
        const std::string key = std::string("feat.") + t.id;
        if (lk.find(key) != lk.end())
        {
            t.flag->store(GetOr<int>(lk, key.c_str(), 0) != 0);
            ++restored;
        }
    }
    for (const auto& v : toggles::Floats())
        if (v.value)
            v.value->store(GetOr(lk, (std::string("val.") + v.id).c_str(), v.value->load()));

    CM_INFO("config: loaded prefs + %d saved toggles (overlayEnabled=%d, themeFile='%s')", restored,
            g_prefs.overlayEnabled ? 1 : 0, g_prefs.themeFile.c_str());
}

void Save()
{
    const auto path = ConfigPath();
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"w") != 0 || !fp)
    {
        CM_WARN("config: could not open for writing");
        return;
    }

    std::fprintf(fp, "# CyberBallz config\n");
    std::fprintf(fp, "toggleKey=%d\n", g_prefs.toggleKey);
    std::fprintf(fp, "accentPreset=%d\n", g_prefs.accentPreset);
    std::fprintf(fp, "uiScale=%.3f\n", g_prefs.uiScale);
    std::fprintf(fp, "startOpen=%d\n", g_prefs.startOpen ? 1 : 0);
    std::fprintf(fp, "overlayEnabled=%d\n", g_prefs.overlayEnabled ? 1 : 0);
    std::fprintf(fp, "showWatermark=%d\n", g_prefs.showWatermark ? 1 : 0);
    std::fprintf(fp, "showFps=%d\n", g_prefs.showFps ? 1 : 0);
    std::fprintf(fp, "showDiagnostics=%d\n", g_prefs.showDiagnostics ? 1 : 0);
    std::fprintf(fp, "themeDir=%s\n", g_prefs.themeDir.c_str());
    std::fprintf(fp, "themeFile=%s\n", g_prefs.themeFile.c_str());
    std::fprintf(fp, "animatedBackground=%d\n", g_prefs.animatedBackground ? 1 : 0);
    std::fprintf(fp, "favorites=%s\n", g_prefs.favorites.c_str());
    // Feature toggles + slider values are intentionally not written here; they
    // persist only via SaveLoadout() (the explicit Save button), so a preference
    // change never drags the whole loadout to disk with it.
    std::fclose(fp);
}

void SaveLoadout()
{
    const auto path = LoadoutPath();
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"w") != 0 || !fp)
    {
        CM_WARN("config: could not open loadout for writing");
        return;
    }
    std::fprintf(fp, "# CyberBallz loadout (saved on demand)\n");
    int n = 0;
    for (const auto& t : toggles::All())
        if (t.flag)
        {
            std::fprintf(fp, "feat.%s=%d\n", t.id, t.flag->load() ? 1 : 0);
            ++n;
        }
    for (const auto& v : toggles::Floats())
        if (v.value)
            std::fprintf(fp, "val.%s=%.3f\n", v.id, v.value->load());
    std::fclose(fp);
    CM_INFO("config: saved loadout (%d toggles) to '%ls'", n, path.c_str());
}
} // namespace cm::config
