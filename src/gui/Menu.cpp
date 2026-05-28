#include "Menu.hpp"
#include "Tabs.hpp"
#include "Theme.hpp"
#include "Widgets.hpp"
#include "Notifications.hpp"
#include "Background.hpp"
#include "FeatureList.hpp"
#include "core/Config.hpp"
#include "core/State.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <cstdio>

namespace cm::gui
{
namespace
{
// Layout state shared across the per-section helpers (single-threaded render).
float g_S = 1.0f;
float g_sidebarW = 212.0f;
float g_headerH = 74.0f;
float g_footerH = 92.0f;
ImVec2 g_winPos, g_winSize;
char g_search[64] = ""; // live feature-search query (header search box)

float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

struct NavEntry
{
    const char* label;
    widgets::Icon icon;
    Tab tab;
};
const NavEntry kNav[] = {
    {"Dashboard", widgets::Icon::Dashboard, Tab::Dashboard},
    {"Player", widgets::Icon::Player, Tab::Player},
    {"Combat", widgets::Icon::Combat, Tab::Combat},
    {"Stealth", widgets::Icon::Stealth, Tab::Stealth},
    {"Netrunner", widgets::Icon::Netrunner, Tab::Netrunner},
    {"Buffs", widgets::Icon::Buffs, Tab::Buffs},
    {"Inventory", widgets::Icon::Inventory, Tab::Inventory},
    {"Vehicles", widgets::Icon::Vehicle, Tab::Vehicles},
    {"World", widgets::Icon::World, Tab::World},
    {"Teleport", widgets::Icon::Teleport, Tab::Teleport},
    {"Profiles", widgets::Icon::Profiles, Tab::Profiles},
    {"Settings", widgets::Icon::Settings, Tab::Settings},
};

const char* TabTitle(Tab t)
{
    switch (t)
    {
    case Tab::Dashboard: return "Dashboard";
    case Tab::Player: return "Player";
    case Tab::Combat: return "Combat";
    case Tab::Stealth: return "Stealth & Heat";
    case Tab::Netrunner: return "Netrunner";
    case Tab::Buffs: return "Buff Lab";
    case Tab::Inventory: return "Inventory & Money";
    case Tab::Vehicles: return "Vehicles";
    case Tab::World: return "World & Visuals";
    case Tab::Teleport: return "Teleport";
    case Tab::Profiles: return "Profiles";
    case Tab::Settings: return "Settings";
    default: return "";
    }
}

const char* TabSubtitle(Tab t)
{
    switch (t)
    {
    case Tab::Dashboard: return "Overview and quick actions";
    case Tab::Player: return "Survivability, movement and self";
    case Tab::Combat: return "Damage, ammo and time dilation";
    case Tab::Stealth: return "Detection, alerts and wanted heat";
    case Tab::Netrunner: return "RAM, quickhacks and cooldowns";
    case Tab::Buffs: return "Apply status effects and cocktails";
    case Tab::Inventory: return "Eddies, items and crafting";
    case Tab::Vehicles: return "Spawn, summon and protect";
    case Tab::World: return "Time, weather and camera";
    case Tab::Teleport: return "Waypoint and saved locations";
    case Tab::Profiles: return "Save and load operator loadouts";
    case Tab::Settings: return "Appearance, hotkeys and config";
    default: return "";
    }
}

// On-screen watermark + FPS, drawn every frame (independent of the menu window).
void DrawWatermark()
{
    const auto& prefs = config::Prefs();
    if (!prefs.showWatermark && !prefs.showFps)
        return;

    const auto& P = theme::Colors();
    ImFont* f = theme::GetFonts().caption ? theme::GetFonts().caption : ImGui::GetFont();
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const char* brand = prefs.showWatermark ? "CyberBallz" : "";
    char fps[32] = "";
    if (prefs.showFps)
        std::snprintf(fps, sizeof(fps), prefs.showWatermark ? "  %.0f FPS" : "%.0f FPS", ImGui::GetIO().Framerate);

    const float pad = 9.0f;
    const float bw = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, brand).x;
    const float fw = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, fps).x;
    ImVec2 o(vp->Pos.x + 16.0f, vp->Pos.y + 14.0f);
    ImVec2 e(o.x + pad * 2 + bw + fw, o.y + pad * 1.4f + f->FontSize);
    dl->AddRectFilled(o, e, IM_COL32(0, 0, 0, 150), 6.0f);
    dl->AddRect(o, e, theme::WithAlpha(P.accent, 0.55f), 6.0f, 0, 1.0f);
    ImVec2 tp(o.x + pad, o.y + pad * 0.7f);
    if (prefs.showWatermark)
        dl->AddText(f, f->FontSize, tp, P.accent, brand);
    if (prefs.showFps)
        dl->AddText(f, f->FontSize, ImVec2(tp.x + bw, tp.y), P.textDim, fps);
}
} // namespace

Menu& Menu::Get()
{
    static Menu instance;
    return instance;
}

void Menu::Toggle()
{
    Open(!m_open);
}

void Menu::Open(bool open)
{
    m_open = open;
    State::Get().uiOpen = open; // let the game thread skip polling when closed
}

void Menu::DrawSidebar(float width, float height)
{
    const auto& P = theme::Colors();
    const auto& F = theme::GetFonts();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::BeginChild("##sidebar", ImVec2(width, height), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    // Brand
    const float pad = 22.0f * g_S;
    ImVec2 brand = ImVec2(ImGui::GetWindowPos().x + pad, ImGui::GetWindowPos().y + 26.0f * g_S);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w1 = F.title->CalcTextSizeA(F.title->FontSize, FLT_MAX, 0, "Cyber").x;
    dl->AddText(F.title, F.title->FontSize, brand, P.text, "Cyber");
    dl->AddText(F.title, F.title->FontSize, ImVec2(brand.x + w1, brand.y), P.accent, "Ballz");
    dl->AddText(F.caption, F.caption->FontSize, ImVec2(brand.x + 1, brand.y + F.title->FontSize + 2.0f),
                P.textFaint, "NIGHT CITY  \xe2\x80\xa2  v1.0");

    // Nav list (constrained width so items don't span the window)
    const float navTop = 96.0f * g_S;
    ImGui::SetCursorPos(ImVec2(pad * 0.6f, navTop));
    // Scrollable so the nav never clips as the tab list grows (thin themed bar
    // only appears if the list overflows).
    ImGui::BeginChild("##navlist", ImVec2(width - pad * 1.2f, height - navTop - 56.0f * g_S),
                      ImGuiChildFlags_None, ImGuiWindowFlags_None);
    for (const auto& n : kNav)
    {
        if (widgets::NavItem(n.label, n.icon, m_tab == n.tab))
            m_tab = n.tab;
        ImGui::Dummy(ImVec2(0, 3.0f * g_S));
    }
    ImGui::EndChild();

    // Bottom status
    const auto disp = State::Get().GetDisplay();
    ImVec2 stPos = ImVec2(ImGui::GetWindowPos().x + pad, ImGui::GetWindowPos().y + height - 44.0f * g_S);
    const bool connected = disp.gameLoaded && disp.playerValid;
    const ImU32 dot = connected ? P.success : P.textFaint;
    dl->AddCircleFilled(ImVec2(stPos.x + 4, stPos.y + F.caption->FontSize * 0.5f), 4.0f, dot);
    dl->AddText(F.caption, F.caption->FontSize, ImVec2(stPos.x + 16, stPos.y), P.textDim,
                connected ? "Connected to session" : "Waiting for game...");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void Menu::DrawHeader()
{
    const auto& P = theme::Colors();
    const auto& F = theme::GetFonts();

    ImGui::SetCursorPos(ImVec2(g_sidebarW, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::BeginChild("##header", ImVec2(g_winSize.x - g_sidebarW, g_headerH), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    const float pad = 26.0f * g_S;

    dl->AddText(F.header, F.header->FontSize, ImVec2(wp.x + pad, wp.y + 16.0f * g_S), P.text, TabTitle(m_tab));
    dl->AddText(F.caption, F.caption->FontSize, ImVec2(wp.x + pad + 1, wp.y + 16.0f * g_S + F.header->FontSize + 2.0f),
                P.textDim, TabSubtitle(m_tab));

    // Hotkey hint pill + close button on the right.
    const float closeSz = 26.0f * g_S;
    ImVec2 cl(wp.x + ImGui::GetWindowSize().x - pad - closeSz, wp.y + (g_headerH - closeSz) * 0.5f);
    ImGui::SetCursorScreenPos(cl);
    ImGui::InvisibleButton("##close", ImVec2(closeSz, closeSz));
    bool hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked())
        Open(false);
    dl->AddRectFilled(cl, ImVec2(cl.x + closeSz, cl.y + closeSz),
                      hov ? theme::WithAlpha(P.danger, 0.18f) : P.panelHover, 7.0f);
    const ImU32 xcol = hov ? P.danger : P.textDim;
    const float m = closeSz * 0.32f;
    ImVec2 cc(cl.x + closeSz * 0.5f, cl.y + closeSz * 0.5f);
    dl->AddLine(ImVec2(cc.x - m, cc.y - m), ImVec2(cc.x + m, cc.y + m), xcol, 1.8f);
    dl->AddLine(ImVec2(cc.x - m, cc.y + m), ImVec2(cc.x + m, cc.y - m), xcol, 1.8f);

    // Live search box, left of the close button - filters features across all
    // tabs while non-empty.
    const float searchW = 210.0f * g_S;
    ImGui::SetCursorScreenPos(ImVec2(cl.x - 14.0f * g_S - searchW, wp.y + (g_headerH - ImGui::GetFrameHeight()) * 0.5f));
    ImGui::SetNextItemWidth(searchW);
    ImGui::InputTextWithHint("##search", "Search features...", g_search, sizeof(g_search));

    // separator under header
    dl->AddLine(ImVec2(wp.x, wp.y + g_headerH - 1), ImVec2(wp.x + ImGui::GetWindowSize().x, wp.y + g_headerH - 1),
                P.border, 1.0f);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void Menu::DrawBody()
{
    const float bodyH = g_winSize.y - g_headerH - g_footerH;
    ImGui::SetCursorPos(ImVec2(g_sidebarW, g_headerH));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26.0f * g_S, 18.0f * g_S));
    ImGui::BeginChild("##body", ImVec2(g_winSize.x - g_sidebarW, bodyH),
                      ImGuiChildFlags_AlwaysUseWindowPadding);

    // When the search box has text, the body becomes a live, cross-tab results
    // list instead of the selected tab.
    if (g_search[0] != '\0')
    {
        DrawFeatureList(g_search, false);
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    switch (m_tab)
    {
    case Tab::Dashboard: tabs::Dashboard(); break;
    case Tab::Player: tabs::Player(); break;
    case Tab::Combat: tabs::Combat(); break;
    case Tab::Stealth: tabs::Stealth(); break;
    case Tab::Netrunner: tabs::Netrunner(); break;
    case Tab::Buffs: tabs::Buffs(); break;
    case Tab::Inventory: tabs::Inventory(); break;
    case Tab::Vehicles: tabs::Vehicles(); break;
    case Tab::World: tabs::World(); break;
    case Tab::Teleport: tabs::Teleport(); break;
    case Tab::Profiles: tabs::Profiles(); break;
    case Tab::Settings: tabs::Settings(); break;
    default: break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void Menu::DrawFooter()
{
    const auto& P = theme::Colors();
    const auto disp = State::Get().GetDisplay();

    ImGui::SetCursorPos(ImVec2(g_sidebarW, g_winSize.y - g_footerH));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26.0f * g_S, 14.0f * g_S));
    ImGui::BeginChild("##footer", ImVec2(g_winSize.x - g_sidebarW, g_footerH),
                      ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    dl->AddLine(wp, ImVec2(wp.x + ImGui::GetWindowSize().x, wp.y), P.border, 1.0f);

    const float colW = (ImGui::GetContentRegionAvail().x - 24.0f * g_S) * 0.5f;

    ImGui::BeginGroup();
    {
        char hp[32];
        snprintf(hp, sizeof(hp), "%.0f / %.0f", disp.health, disp.healthMax);
        ImGui::PushItemWidth(colW);
        ImGui::BeginChild("##hpcol", ImVec2(colW, g_footerH - 28.0f * g_S), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar);
        widgets::StatBar("HEALTH", disp.health, disp.healthMax, P.success, disp.playerValid ? hp : "--");
        char st[32];
        snprintf(st, sizeof(st), "%.0f / %.0f", disp.stamina, disp.staminaMax);
        widgets::StatBar("STAMINA", disp.stamina, disp.staminaMax, P.accent, disp.playerValid ? st : "--");
        ImGui::EndChild();
        ImGui::PopItemWidth();
    }
    ImGui::EndGroup();

    ImGui::SameLine(0, 24.0f * g_S);

    ImGui::BeginGroup();
    {
        const auto& F = theme::GetFonts();
        ImVec2 p = ImGui::GetCursorScreenPos();
        char pos[96];
        if (disp.playerValid)
            snprintf(pos, sizeof(pos), "X %.1f    Y %.1f    Z %.1f", disp.posX, disp.posY, disp.posZ);
        else
            snprintf(pos, sizeof(pos), "position unavailable");
        dl->AddText(F.caption, F.caption->FontSize, p, P.textDim, "POSITION");
        dl->AddText(F.caption, F.caption->FontSize, ImVec2(p.x, p.y + F.caption->FontSize + 4.0f), P.text, pos);

        dl->AddText(F.caption, F.caption->FontSize, ImVec2(p.x, p.y + (F.caption->FontSize + 6.0f) * 2.0f), P.textDim,
                    "INSERT to toggle");
    }
    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void Menu::Draw()
{
    ImGuiIO& io = ImGui::GetIO();
    const float target = m_open ? 1.0f : 0.0f;
    // Frame-rate independent; close is faster than open so dismissing feels instant.
    const float speed = m_open ? 17.0f : 24.0f;
    m_anim += (target - m_anim) * Clamp01(io.DeltaTime * speed);
    if (std::fabs(m_anim - target) < 0.004f)
        m_anim = target;

    io.MouseDrawCursor = m_open || m_anim > 0.1f;

    if (m_anim > 0.004f)
    {
        const auto& P = theme::Colors();
        const auto& F = theme::GetFonts();
        ImGuiStyle& style = ImGui::GetStyle();

        g_S = config::Prefs().uiScale;
        g_sidebarW = 212.0f * g_S;
        g_headerH = 74.0f * g_S;
        g_footerH = 92.0f * g_S;

        const float W = 880.0f * g_S;
        const float H = 600.0f * g_S;
        const float ease = 1.0f - std::pow(1.0f - m_anim, 3.0f); // easeOutCubic

        // Centered, with a short slide-up on open / slide-down on close + fade.
        ImGuiViewport* vp = ImGui::GetMainViewport();
        const float slide = (1.0f - ease) * 28.0f * g_S;
        ImGui::SetNextWindowSize(ImVec2(W, H), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(vp->Pos.x + (vp->Size.x - W) * 0.5f, vp->Pos.y + (vp->Size.y - H) * 0.5f + slide),
            ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ease);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, P.windowBg);

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                                       ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("##CyberBallzRoot", nullptr, flags))
        {
            g_winPos = ImGui::GetWindowPos();
            g_winSize = ImGui::GetWindowSize();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 winEnd(g_winPos.x + g_winSize.x, g_winPos.y + g_winSize.y);

            // Themed background image (drawn first; panels/text sit on top).
            const bool bgActive = config::Prefs().animatedBackground && bg::Valid() && bg::CurrentTexture();
            if (bgActive)
            {
                dl->AddImageRounded(bg::CurrentTexture(), g_winPos, winEnd, ImVec2(0, 0), ImVec2(1, 1),
                                    IM_COL32(255, 255, 255, 255), style.WindowRounding);
                dl->AddRectFilled(g_winPos, winEnd, IM_COL32(0, 0, 0, 120), style.WindowRounding);
            }

            // sidebar background (rounded on the left; translucent over a bg image)
            dl->AddRectFilled(g_winPos, ImVec2(g_winPos.x + g_sidebarW, g_winPos.y + g_winSize.y),
                              bgActive ? theme::WithAlpha(P.sidebarBg, 0.66f) : P.sidebarBg,
                              style.WindowRounding, ImDrawFlags_RoundCornersLeft);
            // divider
            dl->AddLine(ImVec2(g_winPos.x + g_sidebarW, g_winPos.y + 3.0f * g_S),
                        ImVec2(g_winPos.x + g_sidebarW, g_winPos.y + g_winSize.y - 1.0f), P.border, 1.0f);

            // neon accent edge: a soft glow under a crisp 1.5px border
            dl->AddRect(g_winPos, winEnd, theme::WithAlpha(P.accent, 0.18f), style.WindowRounding, 0, 4.0f * g_S);
            dl->AddRect(g_winPos, winEnd, theme::WithAlpha(P.accent, 0.90f), style.WindowRounding, 0, 1.5f * g_S);

            DrawSidebar(g_sidebarW, g_winSize.y);
            DrawHeader();
            DrawBody();
            DrawFooter();
        }
        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        (void)F;
    }

    notify::Render();
    DrawWatermark();
    m_firstFrame = false;
}
} // namespace cm::gui
