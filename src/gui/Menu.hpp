#pragma once

// The top-level overlay window: brand, sidebar navigation, content router,
// live footer, and open/close animation. Drawn once per frame by the Renderer.
namespace cm::gui
{
enum class Tab
{
    Dashboard,
    Player,
    Combat,
    Stealth,
    Netrunner,
    Buffs,
    Inventory,
    Vehicles,
    World,
    Teleport,
    Profiles,
    Settings,
    Count,
};

class Menu
{
public:
    static Menu& Get();

    void Draw();
    void Toggle();
    void Open(bool open);
    bool IsOpen() const { return m_open; }
    // True while the overlay needs to draw (open or mid open/close animation).
    bool NeedsRender() const { return m_open || m_anim > 0.004f; }
    void SelectTab(Tab t) { m_tab = t; } // used by the preview harness

private:
    Menu() = default;

    void DrawSidebar(float width, float height);
    void DrawHeader();
    void DrawFooter();
    void DrawBody();

    bool m_open = false;
    float m_anim = 0.0f; // 0 closed .. 1 open
    Tab m_tab = Tab::Dashboard;
    bool m_firstFrame = true;
};
} // namespace cm::gui
