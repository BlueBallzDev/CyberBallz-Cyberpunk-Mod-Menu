#include "ThemeIO.hpp"
#include "core/Json.hpp"
#include "core/Config.hpp"
#include "core/Logger.hpp"

#include <Windows.h>
#include <Shlobj.h>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace cm::gui::themeio
{
namespace
{
int ClampByte(double v)
{
    int i = static_cast<int>(v + 0.5);
    return i < 0 ? 0 : (i > 255 ? 255 : i);
}

// Read an {r,g,b,a} color trying several key aliases. Returns false if none hit.
bool ReadColor(const json::Value& root, std::initializer_list<const char*> keys, ImU32& out)
{
    for (const char* k : keys)
    {
        const json::Value* v = root.Find(k);
        if (v && v->IsObject())
        {
            const auto* r = v->Find("r");
            const auto* g = v->Find("g");
            const auto* b = v->Find("b");
            const auto* a = v->Find("a");
            out = IM_COL32(ClampByte(r ? r->AsNumber() : 0), ClampByte(g ? g->AsNumber() : 0),
                           ClampByte(b ? b->AsNumber() : 0), ClampByte(a ? a->AsNumber(255) : 255));
            return true;
        }
    }
    return false;
}

float ReadFloat(const json::Value& root, std::initializer_list<const char*> keys, float def)
{
    for (const char* k : keys)
        if (const json::Value* v = root.Find(k); v && v->IsNumber())
            return static_cast<float>(v->number);
    return def;
}

ImVec2 ReadVec2(const json::Value& root, const char* key, ImVec2 def)
{
    if (const json::Value* v = root.Find(key); v && v->IsObject())
    {
        const auto* x = v->Find("x");
        const auto* y = v->Find("y");
        return ImVec2(x ? static_cast<float>(x->number) : def.x, y ? static_cast<float>(y->number) : def.y);
    }
    return def;
}

ImU32 WithA(ImU32 c, float a)
{
    return theme::WithAlpha(c, a);
}

// Blend rgb of `c` toward `target` by t, keeping c's alpha.
ImU32 Lighten(ImU32 c, ImU32 target, float t)
{
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
    ImVec4 d = ImGui::ColorConvertU32ToFloat4(target);
    ImVec4 r(v.x + (d.x - v.x) * t, v.y + (d.y - v.y) * t, v.z + (d.z - v.z) * t, v.w);
    return ImGui::ColorConvertFloat4ToU32(r);
}

std::wstring LocalThemesDir()
{
    PWSTR path = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
    {
        out = path;
        out += L"\\CyberBallz\\themes";
    }
    if (path)
        CoTaskMemFree(path);
    return out;
}

void AddThemesFromDir(const fs::path& dir, std::vector<ThemeEntry>& out)
{
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
        return;

    // Flat .json files directly in the directory.
    for (const auto& e : fs::directory_iterator(dir, ec))
    {
        if (ec)
            break;
        if (e.is_regular_file() && e.path().extension() == ".json")
            out.push_back({e.path().stem().string(), e.path().string()});
    }

    // A theme may also be a subfolder containing <name>.json.
    for (const auto& e : fs::directory_iterator(dir, ec))
    {
        if (ec)
            break;
        if (!e.is_directory())
            continue;
        for (const auto& inner : fs::directory_iterator(e.path(), ec))
        {
            if (inner.is_regular_file() && inner.path().extension() == ".json")
            {
                out.push_back({e.path().filename().string(), inner.path().string()});
                break;
            }
        }
    }
}
} // namespace

std::vector<ThemeEntry> ScanThemes()
{
    std::vector<ThemeEntry> out;
    AddThemesFromDir(fs::path(config::Prefs().themeDir), out);
    AddThemesFromDir(fs::path(LocalThemesDir()), out);

    std::sort(out.begin(), out.end(), [](const ThemeEntry& a, const ThemeEntry& b) { return a.name < b.name; });
    out.erase(std::unique(out.begin(), out.end(),
                          [](const ThemeEntry& a, const ThemeEntry& b) { return a.jsonPath == b.jsonPath; }),
              out.end());
    return out;
}

bool LoadTheme(const std::string& jsonPath, theme::LoadedTheme& out)
{
    json::Value root;
    if (!json::ParseFile(jsonPath, root) || !root.IsObject())
    {
        CM_WARN("Theme parse failed: %s", jsonPath.c_str());
        return false;
    }

    theme::LoadedTheme t;
    t.name = fs::path(jsonPath).parent_path().filename().string();
    if (t.name.empty())
        t.name = fs::path(jsonPath).stem().string();

    auto& p = t.palette;
    ImU32 windowBg = IM_COL32(12, 12, 12, 255);
    ImU32 childBg = IM_COL32(8, 8, 8, 255);
    ImU32 frameBg = IM_COL32(25, 25, 25, 255);
    ImU32 border = IM_COL32(255, 255, 255, 26);
    ImU32 text = IM_COL32(255, 255, 255, 255);
    ImU32 danger = IM_COL32(255, 74, 82, 255);
    ImU32 accent = IM_COL32(247, 251, 255, 255); // cool white "star" accent

    ReadColor(root, {"WindowBg", "Window Bg"}, windowBg);
    ReadColor(root, {"ChildBg", "Child Bg"}, childBg);
    ReadColor(root, {"FrameBg", "Frame Bg"}, frameBg);
    ReadColor(root, {"Border"}, border);
    ReadColor(root, {"Text"}, text);
    ReadColor(root, {"Console Attack"}, danger);
    // Accent preference: explicit accent-ish keys, else a star/check color.
    ReadColor(root, {"Accent", "Title Bar Star Icon Color", "CheckMark", "Check Mark", "NavCursor"}, accent);

    p.windowBg = windowBg;
    p.sidebarBg = childBg;
    p.panel = frameBg;
    p.panelHover = Lighten(frameBg, text, 0.10f);
    p.panelActive = Lighten(frameBg, text, 0.20f);
    p.border = border;
    p.text = text;
    p.textDim = WithA(text, 0.62f);
    p.textFaint = WithA(text, 0.40f);
    p.success = IM_COL32(60, 220, 130, 255);
    p.danger = danger;
    p.track = Lighten(windowBg, text, 0.16f);
    p.accent = accent;
    p.accentDim = WithA(accent, 0.55f);
    p.accentSoft = WithA(accent, 0.14f);

    t.windowRounding = ReadFloat(root, {"Window Rounding"}, 6.0f);
    t.childRounding = ReadFloat(root, {"Child Rounding"}, 6.0f);
    t.frameRounding = ReadFloat(root, {"Frame Rounding"}, 5.0f);
    t.popupRounding = ReadFloat(root, {"Popup Rounding"}, 6.0f);
    t.tabRounding = ReadFloat(root, {"Tab Rounding"}, 6.0f);
    t.scrollbarRounding = ReadFloat(root, {"Scrollbar Rounding"}, 9.0f);
    t.scrollbarSize = ReadFloat(root, {"Scrollbar Size"}, 10.0f);
    t.childBorderSize = ReadFloat(root, {"Child Border Size"}, 1.0f);
    t.frameBorderSize = ReadFloat(root, {"Frame Border Size"}, 0.0f);
    t.popupBorderSize = ReadFloat(root, {"Popup Border Size"}, 1.0f);
    t.framePadding = ReadVec2(root, "Frame Padding", ImVec2(12, 7));
    t.itemSpacing = ReadVec2(root, "Item Spacing", ImVec2(8, 6));
    t.itemInnerSpacing = ReadVec2(root, "Item Inner Spacing", ImVec2(6, 5));

    // Background image: a sibling background.gif/png/jpg next to the .json.
    const fs::path dir = fs::path(jsonPath).parent_path();
    for (const char* candidate : {"background.gif", "background.png", "background.jpg", "background.jpeg"})
    {
        std::error_code ec;
        const fs::path bg = dir / candidate;
        if (fs::exists(bg, ec))
        {
            t.backgroundPath = bg.string();
            break;
        }
    }

    t.valid = true;
    out = std::move(t);
    return true;
}

void ApplyConfiguredTheme()
{
    // Theme-file loading was removed - the menu uses the built-in look with a
    // user-selectable accent colour only. Always use built-in.
    theme::UseBuiltIn();
}
} // namespace cm::gui::themeio
