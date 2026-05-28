#include "WndProc.hpp"
#include "gui/Menu.hpp"
#include "gui/Notifications.hpp"
#include "core/Config.hpp"
#include "core/State.hpp"
#include "core/DebugConsole.hpp"

#include <imgui.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace cm::render::input
{
namespace
{
WNDPROC g_originalWndProc = nullptr;
HWND g_hwnd = nullptr;

bool IsInputMessage(UINT msg)
{
    switch (msg)
    {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL:
    case WM_KEYDOWN:     case WM_KEYUP:
    case WM_SYSKEYDOWN:  case WM_SYSKEYUP:
    case WM_CHAR:        case WM_SETCURSOR:
    case WM_INPUT:
        return true;
    default:
        return false;
    }
}

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Toggle on key release of the configured hotkey.
    if (msg == WM_KEYUP && static_cast<int>(wParam) == cm::config::Prefs().toggleKey)
    {
        // When opening, center the OS cursor first (while m_open is still false,
        // so our SetCursorPos block doesn't swallow it) so it's instantly visible.
        if (!cm::gui::Menu::Get().IsOpen())
        {
            RECT rc{};
            if (GetClientRect(hwnd, &rc))
            {
                POINT c{(rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2};
                ClientToScreen(hwnd, &c);
                SetCursorPos(c.x, c.y);
            }
        }
        cm::gui::Menu::Get().Toggle();
        return 0;
    }

    // Backtick toggles the developer console (independent of the main menu).
    if (msg == WM_KEYUP && static_cast<int>(wParam) == VK_OEM_3)
    {
        cm::debug::Toggle();
        return 0;
    }

    // Global feature hotkeys (only when the menu is closed, so they never eat
    // keys the UI might want). Fire on key release.
    if (msg == WM_KEYUP && !cm::gui::Menu::Get().IsOpen() && !cm::debug::IsOpen())
    {
        auto& st = cm::State::Get();
        switch (static_cast<int>(wParam))
        {
        case VK_F5:
            st.godMode = !st.godMode.load();
            cm::gui::notify::Success(st.godMode.load() ? "God Mode ON" : "God Mode OFF");
            return 0;
        case VK_F6:
            st.Push({cm::ActionType::HealPlayer});
            cm::gui::notify::Info("Restoring player");
            return 0;
        case VK_F7:
            st.undetectable = !st.undetectable.load();
            cm::gui::notify::Success(st.undetectable.load() ? "Undetectable ON" : "Undetectable OFF");
            return 0;
        case VK_F8:
            st.Push({cm::ActionType::ClearHeat});
            cm::gui::notify::Info("Clearing heat");
            return 0;
        case VK_F9:
            st.infiniteAmmoNoReload = !st.infiniteAmmoNoReload.load();
            cm::gui::notify::Success(st.infiniteAmmoNoReload.load() ? "Infinite Ammo ON" : "Infinite Ammo OFF");
            return 0;
        default:
            break;
        }
    }

    if (cm::gui::Menu::Get().IsOpen() || cm::debug::IsOpen())
    {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
        // While open, route input to the overlay and stop the game from
        // consuming the same input (camera/movement/shooting).
        if (IsInputMessage(msg))
            return 0;
    }

    return CallWindowProcW(g_originalWndProc, hwnd, msg, wParam, lParam);
}
} // namespace

void Install(HWND hwnd)
{
    if (g_originalWndProc || !hwnd)
        return;
    g_hwnd = hwnd;
    g_originalWndProc =
        reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));
}

void Uninstall()
{
    if (g_originalWndProc && g_hwnd)
    {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
        g_originalWndProc = nullptr;
        g_hwnd = nullptr;
    }
}
} // namespace cm::render::input
