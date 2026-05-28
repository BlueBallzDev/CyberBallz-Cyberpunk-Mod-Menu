#include "Theme.hpp"
#include "core/Config.hpp"

#include <cstdio>

namespace cm::gui::theme
{
namespace
{
Palette g_palette;
Fonts g_fonts;
int g_accentIndex = 0;
ImU32 g_accent = IM_COL32(0, 224, 255, 255); // current accent (preset or theme)

bool g_useTheme = false;
LoadedTheme g_theme;

const AccentPreset kAccents[] = {
    {"Neon Cyan", IM_COL32(0, 224, 255, 255)},
    {"Electric Blue", IM_COL32(40, 120, 255, 255)},
    {"Royal Blue", IM_COL32(70, 90, 255, 255)},
    {"Indigo", IM_COL32(99, 102, 241, 255)},
    {"Synth Violet", IM_COL32(149, 97, 255, 255)},
    {"Purple", IM_COL32(168, 85, 247, 255)},
    {"Lavender", IM_COL32(180, 160, 255, 255)},
    {"Hot Magenta", IM_COL32(255, 47, 146, 255)},
    {"Hot Pink", IM_COL32(255, 80, 180, 255)},
    {"Rose", IM_COL32(244, 114, 182, 255)},
    {"Coral", IM_COL32(255, 99, 99, 255)},
    {"Crimson", IM_COL32(255, 64, 72, 255)},
    {"Red", IM_COL32(239, 68, 68, 255)},
    {"Orange", IM_COL32(255, 120, 40, 255)},
    {"Tangerine", IM_COL32(255, 159, 28, 255)},
    {"Amber", IM_COL32(255, 193, 7, 255)},
    {"Cyber Yellow", IM_COL32(252, 238, 10, 255)},
    {"Lime", IM_COL32(190, 242, 100, 255)},
    {"Toxic Green", IM_COL32(60, 245, 145, 255)},
    {"Emerald", IM_COL32(16, 185, 129, 255)},
    {"Teal", IM_COL32(20, 200, 200, 255)},
    {"Aqua", IM_COL32(34, 211, 238, 255)},
    {"Mint", IM_COL32(110, 231, 183, 255)},
    {"White", IM_COL32(235, 240, 245, 255)},
};

ImVec4 ToVec4(ImU32 c)
{
    return ImGui::ColorConvertU32ToFloat4(c);
}

void BuildPalette()
{
    if (g_useTheme && g_theme.valid)
        g_palette = g_theme.palette; // base colors from the loaded theme
    else
    {
        // Pure-black "neon sign" look: black background, everything in neon blue.
        g_palette.windowBg = IM_COL32(0, 0, 0, 255);
        g_palette.sidebarBg = IM_COL32(0, 0, 0, 255);
        g_palette.panel = IM_COL32(5, 8, 13, 255);
        g_palette.panelHover = IM_COL32(12, 22, 36, 255);
        g_palette.panelActive = IM_COL32(20, 34, 54, 255);
        g_palette.border = WithAlpha(g_accent, 0.34f); // neon-blue hairlines
        g_palette.text = IM_COL32(66, 190, 255, 255);   // neon blue text
        g_palette.textDim = IM_COL32(44, 130, 190, 255);
        g_palette.textFaint = IM_COL32(30, 84, 128, 255);
        g_palette.success = IM_COL32(40, 200, 255, 255); // blue (keeps the theme)
        g_palette.danger = IM_COL32(255, 70, 96, 255);
        g_palette.track = IM_COL32(14, 24, 38, 255);
    }

    // Accent is an independent layer so the swatches work in either mode.
    g_palette.accent = g_accent;
    g_palette.accentDim = WithAlpha(g_accent, 0.55f);
    g_palette.accentSoft = WithAlpha(g_accent, 0.14f);
}

// Load a TTF only if the file can actually be opened. Calling
// AddFontFromFileTTF on a missing/unreadable path raises an ImGui error and
// leaves the atlas in a bad state (some Windows installs lack Segoe Semibold),
// so we probe first and gracefully fall back.
ImFont* TryLoadFont(const char* path, float size, const ImFontConfig* cfg)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f)
        return nullptr;
    fclose(f);
    return ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size, cfg);
}

void LoadFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const char* regular = "C:\\Windows\\Fonts\\segoeui.ttf";
    const char* semibold = "C:\\Windows\\Fonts\\segoeuisb.ttf";
    const char* bold = "C:\\Windows\\Fonts\\segoeuib.ttf";

    // Cover Latin-1 plus the punctuation/currency we actually use (bullet
    // U+2022, en/em dashes, ellipsis, euro U+20AC) so they don't render as '?'.
    static const ImWchar kRanges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
        0x2010, 0x2027, // general punctuation (dashes, quotes, bullet, ellipsis)
        0x20A0, 0x20BF, // currency symbols (euro)
        0,
    };

    ImFontConfig cfg;
    cfg.OversampleH = 3;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = false;
    cfg.GlyphRanges = kRanges;

    g_fonts.body = TryLoadFont(regular, 18.0f, &cfg);
    if (!g_fonts.body)
        g_fonts.body = io.Fonts->AddFontDefault();

    g_fonts.caption = TryLoadFont(regular, 14.0f, &cfg);
    if (!g_fonts.caption)
        g_fonts.caption = g_fonts.body;

    g_fonts.header = TryLoadFont(semibold, 22.0f, &cfg);
    if (!g_fonts.header)
        g_fonts.header = TryLoadFont(bold, 22.0f, &cfg);
    if (!g_fonts.header)
        g_fonts.header = g_fonts.body;

    g_fonts.title = TryLoadFont(bold, 27.0f, &cfg);
    if (!g_fonts.title)
        g_fonts.title = TryLoadFont(regular, 27.0f, &cfg);
    if (!g_fonts.title)
        g_fonts.title = g_fonts.header;

    io.FontDefault = g_fonts.body;
}

void ApplyStyle()
{
    ImGuiStyle& s = ImGui::GetStyle();
    // Reset to defaults first so ScaleAllSizes() below never compounds across
    // repeated calls (otherwise dragging the UI-scale slider keeps multiplying
    // unset fields each frame and corrupts the layout).
    s = ImGuiStyle();
    const bool t = g_useTheme && g_theme.valid;

    s.WindowRounding = t ? g_theme.windowRounding : 12.0f;
    s.ChildRounding = t ? g_theme.childRounding : 10.0f;
    s.FrameRounding = t ? g_theme.frameRounding : 7.0f;
    s.PopupRounding = t ? g_theme.popupRounding : 8.0f;
    s.GrabRounding = t ? g_theme.frameRounding : 7.0f;
    s.TabRounding = t ? g_theme.tabRounding : 7.0f;
    s.ScrollbarRounding = t ? g_theme.scrollbarRounding : 9.0f;

    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = t ? g_theme.childBorderSize : 1.0f;
    s.FrameBorderSize = t ? g_theme.frameBorderSize : 0.0f;
    s.PopupBorderSize = t ? g_theme.popupBorderSize : 1.0f;

    // Root WindowPadding stays 0 (we lay the shell out manually) regardless of
    // the theme, but inner frame/item metrics follow the theme.
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = t ? g_theme.framePadding : ImVec2(13, 8);
    s.ItemSpacing = t ? g_theme.itemSpacing : ImVec2(10, 9);
    s.ItemInnerSpacing = t ? g_theme.itemInnerSpacing : ImVec2(8, 6);
    s.ScrollbarSize = t ? g_theme.scrollbarSize : 11.0f;
    s.GrabMinSize = 14.0f;

    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    s.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    s.AntiAliasedLines = true;
    s.AntiAliasedFill = true;

    const Palette& p = g_palette;
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = ToVec4(p.text);
    c[ImGuiCol_TextDisabled] = ToVec4(p.textFaint);
    c[ImGuiCol_WindowBg] = ToVec4(p.windowBg);
    c[ImGuiCol_ChildBg] = ToVec4(p.panel);
    c[ImGuiCol_PopupBg] = ToVec4(IM_COL32(20, 22, 28, 252));
    c[ImGuiCol_Border] = ToVec4(p.border);
    c[ImGuiCol_BorderShadow] = ToVec4(IM_COL32(0, 0, 0, 0));
    c[ImGuiCol_FrameBg] = ToVec4(p.panelHover);
    c[ImGuiCol_FrameBgHovered] = ToVec4(p.panelActive);
    c[ImGuiCol_FrameBgActive] = ToVec4(p.panelActive);
    c[ImGuiCol_TitleBg] = ToVec4(p.sidebarBg);
    c[ImGuiCol_TitleBgActive] = ToVec4(p.sidebarBg);
    c[ImGuiCol_CheckMark] = ToVec4(p.accent);
    c[ImGuiCol_SliderGrab] = ToVec4(p.accent);
    c[ImGuiCol_SliderGrabActive] = ToVec4(p.accent);
    c[ImGuiCol_Button] = ToVec4(p.panelHover);
    c[ImGuiCol_ButtonHovered] = ToVec4(p.panelActive);
    c[ImGuiCol_ButtonActive] = ToVec4(p.accentSoft);
    c[ImGuiCol_Header] = ToVec4(p.accentSoft);
    c[ImGuiCol_HeaderHovered] = ToVec4(p.panelHover);
    c[ImGuiCol_HeaderActive] = ToVec4(p.panelActive);
    c[ImGuiCol_Separator] = ToVec4(p.border);
    c[ImGuiCol_SeparatorHovered] = ToVec4(p.accentDim);
    c[ImGuiCol_SeparatorActive] = ToVec4(p.accent);
    c[ImGuiCol_ScrollbarBg] = ToVec4(IM_COL32(0, 0, 0, 0));
    c[ImGuiCol_ScrollbarGrab] = ToVec4(p.panelActive);
    c[ImGuiCol_ScrollbarGrabHovered] = ToVec4(p.accentDim);
    c[ImGuiCol_ScrollbarGrabActive] = ToVec4(p.accent);
    c[ImGuiCol_FrameBgActive] = ToVec4(p.panelActive);
    c[ImGuiCol_TextSelectedBg] = ToVec4(p.accentSoft);
    c[ImGuiCol_NavHighlight] = ToVec4(p.accentDim);

    // Every size field above is assigned explicitly before scaling, so
    // ScaleAllSizes applies to a fresh base each call (never cumulative).
    const float scale = config::Prefs().uiScale;
    if (scale != 1.0f)
        s.ScaleAllSizes(scale);
    ImGui::GetIO().FontGlobalScale = scale;
}
} // namespace

ImU32 WithAlpha(ImU32 color, float alpha)
{
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(color);
    v.w = alpha;
    return ImGui::ColorConvertFloat4ToU32(v);
}

ImU32 Lerp(ImU32 a, ImU32 b, float t)
{
    ImVec4 va = ImGui::ColorConvertU32ToFloat4(a);
    ImVec4 vb = ImGui::ColorConvertU32ToFloat4(b);
    ImVec4 r(va.x + (vb.x - va.x) * t, va.y + (vb.y - va.y) * t, va.z + (vb.z - va.z) * t,
             va.w + (vb.w - va.w) * t);
    return ImGui::ColorConvertFloat4ToU32(r);
}

void Apply()
{
    g_accentIndex = config::Prefs().accentPreset;
    if (g_accentIndex < 0 || g_accentIndex >= AccentCount())
        g_accentIndex = 0;
    if (!g_useTheme)
        g_accent = kAccents[g_accentIndex].color;
    LoadFonts();
    BuildPalette();
    ApplyStyle();
}

const Palette& Colors()
{
    return g_palette;
}

const Fonts& GetFonts()
{
    return g_fonts;
}

void Refresh()
{
    BuildPalette();
    ApplyStyle();
}

void SetAccent(int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= AccentCount())
        return;
    g_accentIndex = presetIndex;
    g_accent = kAccents[presetIndex].color;
    config::Prefs().accentPreset = presetIndex;
    Refresh();
}

void SetUiScale(float scale)
{
    config::Prefs().uiScale = scale;
    ApplyStyle();
}

int AccentCount()
{
    return static_cast<int>(IM_ARRAYSIZE(kAccents));
}

const AccentPreset& AccentAt(int index)
{
    return kAccents[index];
}

void ApplyLoadedTheme(const LoadedTheme& theme)
{
    g_theme = theme;
    g_useTheme = theme.valid;
    if (theme.valid)
        g_accent = theme.palette.accent; // theme defines its own accent
    Refresh();
}

void UseBuiltIn()
{
    g_useTheme = false;
    g_theme = LoadedTheme{};
    g_accent = kAccents[g_accentIndex].color;
    Refresh();
}

bool IsThemeLoaded()
{
    return g_useTheme && g_theme.valid;
}

const std::string& LoadedThemeName()
{
    return g_theme.name;
}

const std::string& BackgroundPath()
{
    return g_theme.backgroundPath;
}
} // namespace cm::gui::theme
