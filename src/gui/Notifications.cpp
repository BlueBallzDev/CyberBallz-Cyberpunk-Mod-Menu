#include "Notifications.hpp"
#include "Theme.hpp"
#include "Widgets.hpp"

#include <imgui_internal.h>

#include <deque>
#include <mutex>

namespace cm::gui::notify
{
namespace
{
struct Toast
{
    Type type;
    std::string message;
    float age = 0.0f;
    float life = 4.0f;
};

std::mutex g_mutex;
std::deque<Toast> g_toasts;

ImU32 AccentFor(Type t)
{
    const auto& P = theme::Colors();
    switch (t)
    {
    case Type::Success: return P.success;
    case Type::Warning: return IM_COL32(255, 196, 64, 255);
    case Type::Error:   return P.danger;
    default:            return P.accent;
    }
}
} // namespace

void Add(Type type, std::string message)
{
    std::scoped_lock lock(g_mutex);
    g_toasts.push_back({type, std::move(message)});
    if (g_toasts.size() > 6)
        g_toasts.pop_front();
}

bool HasActive()
{
    std::scoped_lock lock(g_mutex);
    return !g_toasts.empty();
}

void Render()
{
    std::scoped_lock lock(g_mutex);
    if (g_toasts.empty())
        return;

    ImFont* f = theme::GetFonts().caption;
    if (!f)
        f = ImGui::GetFont();
    if (!f)
        return; // fonts not ready yet - skip this frame rather than deref null

    const auto& P = theme::Colors();
    const float dt = ImGui::GetIO().DeltaTime;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 vp = ImGui::GetMainViewport()->Size;
    const ImVec2 vpPos = ImGui::GetMainViewport()->Pos;

    const float margin = 18.0f;
    const float width = 320.0f;
    float y = vpPos.y + margin;

    for (auto it = g_toasts.begin(); it != g_toasts.end();)
    {
        it->age += dt;
        if (it->age >= it->life)
        {
            it = g_toasts.erase(it);
            continue;
        }

        // Fade in/out.
        float alpha = 1.0f;
        const float fade = 0.35f;
        if (it->age < fade)
            alpha = it->age / fade;
        else if (it->age > it->life - fade)
            alpha = (it->life - it->age) / fade;
        alpha = ImClamp(alpha, 0.0f, 1.0f);

        ImVec2 textSize = f->CalcTextSizeA(f->FontSize, width - 44.0f, width - 44.0f, it->message.c_str());
        const float h = ImMax(44.0f, textSize.y + 24.0f);

        ImVec2 p0(vpPos.x + vp.x - margin - width + (1.0f - alpha) * 20.0f, y);
        ImVec2 p1(p0.x + width, p0.y + h);

        dl->AddRectFilled(p0, p1, theme::WithAlpha(IM_COL32(20, 22, 28, 255), alpha), 10.0f);
        dl->AddRect(p0, p1, theme::WithAlpha(P.border, alpha), 10.0f);
        ImU32 ac = AccentFor(it->type);
        dl->AddRectFilled(p0, ImVec2(p0.x + 4.0f, p1.y), theme::WithAlpha(ac, alpha), 10.0f,
                          ImDrawFlags_RoundCornersLeft);
        dl->AddText(f, f->FontSize, ImVec2(p0.x + 18.0f, p0.y + 12.0f),
                    theme::WithAlpha(P.text, alpha), it->message.c_str(), nullptr, width - 44.0f);

        y += h + 10.0f;
        ++it;
    }
}
} // namespace cm::gui::notify
