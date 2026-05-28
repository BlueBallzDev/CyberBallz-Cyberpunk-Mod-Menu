#include "Widgets.hpp"
#include "Theme.hpp"

#include <imgui_internal.h>

#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <string>

namespace cm::gui::widgets
{
using theme::Colors;
using theme::Lerp;
using theme::WithAlpha;

namespace
{
ImFont* SmallFont()
{
    auto* f = theme::GetFonts().caption;
    return f ? f : ImGui::GetFont();
}

void TextSmall(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text)
{
    ImFont* f = SmallFont();
    dl->AddText(f, f->FontSize, pos, col, text);
}

float Clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}
} // namespace

float Anim(ImGuiID id, float target, float speed)
{
    ImGuiStorage* st = ImGui::GetStateStorage();
    float cur = st->GetFloat(id, target);
    const float dt = ImGui::GetIO().DeltaTime;
    const float t = Clamp01(dt * speed);
    cur += (target - cur) * t;
    if (std::fabs(cur - target) < 0.001f)
        cur = target;
    st->SetFloat(id, cur);
    return cur;
}

// Icons
void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float s, ImU32 col)
{
    const float t = ImMax(1.6f, s * 0.085f);
    switch (icon)
    {
    case Icon::Dashboard:
    {
        const float q = s * 0.30f, g = s * 0.05f;
        ImVec2 tl(c.x - q - g, c.y - q - g), br(c.x + q + g, c.y + q + g);
        dl->AddRect(ImVec2(tl.x, tl.y), ImVec2(tl.x + q, tl.y + q), col, 3.0f, 0, t);
        dl->AddRect(ImVec2(br.x - q, tl.y), ImVec2(br.x, tl.y + q), col, 3.0f, 0, t);
        dl->AddRect(ImVec2(tl.x, br.y - q), ImVec2(tl.x + q, br.y), col, 3.0f, 0, t);
        dl->AddRect(ImVec2(br.x - q, br.y - q), ImVec2(br.x, br.y), col, 3.0f, 0, t);
        break;
    }
    case Icon::Player:
        dl->AddCircle(ImVec2(c.x, c.y - s * 0.22f), s * 0.18f, col, 0, t);
        dl->PathArcTo(ImVec2(c.x, c.y + s * 0.50f), s * 0.40f, IM_PI, IM_PI * 2.0f, 24);
        dl->PathStroke(col, 0, t);
        break;
    case Icon::Inventory:
        dl->AddRect(ImVec2(c.x - s * 0.32f, c.y - s * 0.14f), ImVec2(c.x + s * 0.32f, c.y + s * 0.34f), col,
                    s * 0.08f, 0, t);
        dl->PathArcTo(ImVec2(c.x, c.y - s * 0.14f), s * 0.16f, IM_PI, IM_PI * 2.0f, 16);
        dl->PathStroke(col, 0, t);
        break;
    case Icon::Vehicle:
    {
        ImVec2 pts[] = {ImVec2(c.x - s * 0.34f, c.y + s * 0.10f), ImVec2(c.x - 0.30f * s, c.y - 0.02f * s),
                        ImVec2(c.x - 0.14f * s, c.y - 0.20f * s), ImVec2(c.x + 0.16f * s, c.y - 0.20f * s),
                        ImVec2(c.x + 0.30f * s, c.y - 0.02f * s), ImVec2(c.x + 0.34f * s, c.y + 0.10f * s)};
        dl->AddPolyline(pts, IM_ARRAYSIZE(pts), col, 0, t);
        dl->AddLine(ImVec2(c.x - s * 0.34f, c.y + s * 0.10f), ImVec2(c.x + s * 0.34f, c.y + s * 0.10f), col, t);
        dl->AddCircleFilled(ImVec2(c.x - s * 0.18f, c.y + s * 0.16f), s * 0.085f, col);
        dl->AddCircleFilled(ImVec2(c.x + s * 0.18f, c.y + s * 0.16f), s * 0.085f, col);
        break;
    }
    case Icon::World:
        dl->AddCircle(c, s * 0.36f, col, 28, t);
        dl->AddEllipse(c, ImVec2(s * 0.15f, s * 0.36f), col, 0.0f, 24, t);
        dl->AddLine(ImVec2(c.x - s * 0.36f, c.y), ImVec2(c.x + s * 0.36f, c.y), col, t);
        break;
    case Icon::Teleport:
    {
        // Map pin: ring on top, converging to a point at the bottom.
        const ImVec2 center(c.x, c.y - s * 0.12f);
        const float r = s * 0.24f;
        dl->AddCircle(center, r, col, 20, t);
        dl->AddCircleFilled(center, s * 0.07f, col);
        dl->AddLine(ImVec2(center.x - r * 0.72f, center.y + r * 0.72f), ImVec2(c.x, c.y + s * 0.40f), col, t);
        dl->AddLine(ImVec2(center.x + r * 0.72f, center.y + r * 0.72f), ImVec2(c.x, c.y + s * 0.40f), col, t);
        break;
    }
    case Icon::Settings:
    {
        const float xL = c.x - s * 0.34f, xR = c.x + s * 0.34f;
        const float ys[] = {c.y - s * 0.22f, c.y, c.y + s * 0.22f};
        const float knob[] = {c.x - s * 0.10f, c.x + s * 0.14f, c.x - s * 0.18f};
        for (int i = 0; i < 3; ++i)
        {
            dl->AddLine(ImVec2(xL, ys[i]), ImVec2(xR, ys[i]), WithAlpha(col, 0.8f), t * 0.8f);
            dl->AddCircleFilled(ImVec2(knob[i], ys[i]), s * 0.085f, col);
        }
        break;
    }
    case Icon::Combat:
    {
        // Crosshair / reticle.
        const float r = s * 0.34f;
        dl->AddCircle(c, r, col, 24, t);
        dl->AddLine(ImVec2(c.x, c.y - r - s * 0.10f), ImVec2(c.x, c.y - r * 0.40f), col, t);
        dl->AddLine(ImVec2(c.x, c.y + r * 0.40f), ImVec2(c.x, c.y + r + s * 0.10f), col, t);
        dl->AddLine(ImVec2(c.x - r - s * 0.10f, c.y), ImVec2(c.x - r * 0.40f, c.y), col, t);
        dl->AddLine(ImVec2(c.x + r * 0.40f, c.y), ImVec2(c.x + r + s * 0.10f, c.y), col, t);
        dl->AddCircleFilled(c, s * 0.05f, col);
        break;
    }
    case Icon::Stealth:
    {
        // Eye outline with a pupil (detection).
        dl->PathArcTo(ImVec2(c.x, c.y + s * 0.30f), s * 0.42f, IM_PI * 1.20f, IM_PI * 1.80f, 20);
        dl->PathStroke(col, 0, t);
        dl->PathArcTo(ImVec2(c.x, c.y - s * 0.30f), s * 0.42f, IM_PI * 0.20f, IM_PI * 0.80f, 20);
        dl->PathStroke(col, 0, t);
        dl->AddCircleFilled(c, s * 0.10f, col);
        break;
    }
    case Icon::Netrunner:
    {
        // Microchip: square with pins.
        const float q = s * 0.26f;
        dl->AddRect(ImVec2(c.x - q, c.y - q), ImVec2(c.x + q, c.y + q), col, 2.0f, 0, t);
        for (int i = -1; i <= 1; ++i)
        {
            const float o = i * q * 0.7f;
            dl->AddLine(ImVec2(c.x + o, c.y - q), ImVec2(c.x + o, c.y - q - s * 0.12f), col, t);
            dl->AddLine(ImVec2(c.x + o, c.y + q), ImVec2(c.x + o, c.y + q + s * 0.12f), col, t);
            dl->AddLine(ImVec2(c.x - q, c.y + o), ImVec2(c.x - q - s * 0.12f, c.y + o), col, t);
            dl->AddLine(ImVec2(c.x + q, c.y + o), ImVec2(c.x + q + s * 0.12f, c.y + o), col, t);
        }
        dl->AddCircleFilled(c, s * 0.06f, col);
        break;
    }
    case Icon::Buffs:
    {
        // Flask with a plus.
        dl->AddLine(ImVec2(c.x - s * 0.12f, c.y - s * 0.30f), ImVec2(c.x - s * 0.12f, c.y - s * 0.02f), col, t);
        dl->AddLine(ImVec2(c.x + s * 0.12f, c.y - s * 0.30f), ImVec2(c.x + s * 0.12f, c.y - s * 0.02f), col, t);
        dl->AddLine(ImVec2(c.x - s * 0.20f, c.y - s * 0.30f), ImVec2(c.x + s * 0.20f, c.y - s * 0.30f), col, t);
        dl->PathArcTo(c, s * 0.30f, IM_PI * 0.10f, IM_PI * 0.90f, 18);
        dl->PathStroke(col, 0, t);
        dl->AddLine(ImVec2(c.x, c.y + s * 0.02f), ImVec2(c.x, c.y + s * 0.26f), col, t);
        dl->AddLine(ImVec2(c.x - s * 0.12f, c.y + s * 0.14f), ImVec2(c.x + s * 0.12f, c.y + s * 0.14f), col, t);
        break;
    }
    case Icon::Profiles:
    {
        // Stacked layers.
        for (int i = 0; i < 3; ++i)
        {
            const float y = c.y - s * 0.18f + i * s * 0.18f;
            ImVec2 a(c.x, y - s * 0.10f), b(c.x + s * 0.32f, y), d(c.x, y + s * 0.10f), e(c.x - s * 0.32f, y);
            dl->AddLine(a, b, col, t * 0.85f);
            dl->AddLine(b, d, col, t * 0.85f);
            dl->AddLine(d, e, col, t * 0.85f);
            dl->AddLine(e, a, col, t * 0.85f);
        }
        break;
    }
    }
}

// Sidebar nav item
bool NavItem(const char* label, Icon icon, bool selected)
{
    const auto& P = Colors();
    ImGui::PushID(label);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float w = ImGui::GetContentRegionAvail().x;
    const float h = 38.0f;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1(p0.x + w, p0.y + h);

    ImGui::InvisibleButton("##nav", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();

    const float hv = Anim(ImGui::GetID("hv"), hovered ? 1.0f : 0.0f);
    const float sel = Anim(ImGui::GetID("sel"), selected ? 1.0f : 0.0f);

    if (hv > 0.01f || sel > 0.01f)
    {
        ImU32 fill = Lerp(WithAlpha(P.panelHover, 0.55f * hv), P.accentSoft, sel);
        dl->AddRectFilled(p0, p1, fill, 9.0f);
    }
    if (sel > 0.01f)
    {
        // accent indicator bar on the left edge
        ImVec2 b0(p0.x + 3.0f, p0.y + h * 0.22f);
        ImVec2 b1(p0.x + 3.0f + 3.5f, p0.y + h * 0.78f);
        dl->AddRectFilled(b0, b1, WithAlpha(P.accent, sel), 2.0f);
    }

    const ImU32 fg = Lerp(hovered ? P.text : P.textDim, P.accent, sel);
    DrawIcon(dl, icon, ImVec2(p0.x + 28.0f, p0.y + h * 0.5f), 22.0f, fg);
    ImVec2 txt(p0.x + 50.0f, p0.y + (h - ImGui::GetFontSize()) * 0.5f);
    dl->AddText(txt, fg, label);

    ImGui::PopID();
    return clicked;
}

// Layout helpers
void SectionLabel(const char* text)
{
    const auto& P = Colors();
    ImGui::Dummy(ImVec2(0, 10)); // breathing room above each section
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float fh = SmallFont()->FontSize;
    dl->AddRectFilled(ImVec2(p.x, p.y + 1), ImVec2(p.x + 3, p.y + fh + 1), P.accent, 1.5f);
    TextSmall(dl, ImVec2(p.x + 12, p.y + 1), P.textDim, text);
    ImGui::Dummy(ImVec2(0, fh + 11));
}

bool BeginCard(const char* id, float padding)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 6.0f)); // even row rhythm
    return ImGui::BeginChild(id, ImVec2(0, 0),
                             ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                                 ImGuiChildFlags_AlwaysUseWindowPadding);
}

void EndCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::Dummy(ImVec2(0, 4)); // gap between cards (SectionLabel adds the rest)
}

void Tooltip(const char* text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 18.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void Badge(const char* text, ImU32 color)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* f = SmallFont();
    ImVec2 sz = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, text);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 pad(7, 3);
    ImVec2 end(p.x + sz.x + pad.x * 2, p.y + sz.y + pad.y * 2);
    dl->AddRectFilled(p, end, WithAlpha(color, 0.16f), 5.0f);
    dl->AddText(f, f->FontSize, ImVec2(p.x + pad.x, p.y + pad.y), color, text);
    ImGui::Dummy(ImVec2(sz.x + pad.x * 2, sz.y + pad.y * 2));
}

// Control rows
namespace
{
// Shared row scaffold: draws hover bg + label/subtitle, returns geometry for
// the right-hand control. `controlW` reserves room on the right.
struct RowLayout
{
    ImVec2 p0, p1;
    float rowH;
    bool hovered;
    bool clicked;
    float controlRight;
};

RowLayout BeginRow(const char* label, const char* subtitle, float controlW, bool interactiveRow)
{
    const auto& P = Colors();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w = ImGui::GetContentRegionAvail().x;
    const float rowH = subtitle ? 52.0f : 40.0f;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1(p0.x + w, p0.y + rowH);

    RowLayout L{p0, p1, rowH, false, false, p1.x};
    if (interactiveRow)
    {
        ImGui::InvisibleButton("##row", ImVec2(w, rowH));
        L.hovered = ImGui::IsItemHovered();
        L.clicked = ImGui::IsItemClicked();
    }
    else
    {
        ImGui::Dummy(ImVec2(w, rowH));
        L.hovered = ImGui::IsItemHovered();
    }

    const float hv = Anim(ImGui::GetID("rowhv"), L.hovered ? 1.0f : 0.0f, 14.0f);
    if (hv > 0.01f)
        dl->AddRectFilled(p0, p1, WithAlpha(P.panelHover, 0.5f * hv), 7.0f);

    const float labelY = subtitle ? p0.y + 8.0f : p0.y + (rowH - ImGui::GetFontSize()) * 0.5f;
    dl->AddText(ImVec2(p0.x + 4.0f, labelY), P.text, label);
    if (subtitle)
        TextSmall(dl, ImVec2(p0.x + 4.0f, p0.y + rowH - SmallFont()->FontSize - 8.0f), P.textDim, subtitle);

    L.controlRight = p1.x - 4.0f;
    return L;
}
} // namespace

bool ToggleRow(const char* label, bool* v, const char* subtitle, bool experimental)
{
    const auto& P = Colors();
    ImGui::PushID(label);
    RowLayout L = BeginRow(label, subtitle, 0, true);
    if (L.clicked)
        *v = !*v;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (experimental)
    {
        ImVec2 lblSz = ImGui::CalcTextSize(label);
        ImFont* f = SmallFont();
        const char* tag = "EXPERIMENTAL";
        ImVec2 tsz = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, tag);
        ImVec2 bp(L.p0.x + 4.0f + lblSz.x + 10.0f, (subtitle ? L.p0.y + 8.0f : L.p0.y + (L.rowH - f->FontSize) * 0.5f) - 2.0f);
        dl->AddRectFilled(bp, ImVec2(bp.x + tsz.x + 12, bp.y + f->FontSize + 4), WithAlpha(P.accent, 0.14f), 4.0f);
        dl->AddText(f, f->FontSize, ImVec2(bp.x + 6, bp.y + 2), P.accent, tag);
    }

    // Switch
    const float sw = 42.0f, sh = 22.0f;
    ImVec2 sc(L.controlRight - sw, L.p0.y + (L.rowH - sh) * 0.5f);
    ImVec2 se(sc.x + sw, sc.y + sh);
    const float on = Anim(ImGui::GetID("on"), *v ? 1.0f : 0.0f, 14.0f);
    ImU32 track = Lerp(P.track, P.accent, on);
    dl->AddRectFilled(sc, se, track, sh * 0.5f);
    const float r = sh * 0.5f - 3.0f;
    float knobX = ImLerp(sc.x + r + 3.0f, se.x - r - 3.0f, on);
    dl->AddCircleFilled(ImVec2(knobX, sc.y + sh * 0.5f), r, IM_COL32(255, 255, 255, 255));

    ImGui::PopID();
    return L.clicked;
}

bool SliderRow(const char* label, float* v, float vmin, float vmax, const char* fmt, const char* subtitle)
{
    ImGui::PushID(label);
    RowLayout L = BeginRow(label, subtitle, 0, false);

    const float sliderW = ImClamp(ImGui::GetContentRegionAvail().x * 0.42f, 120.0f, 240.0f);
    ImGui::SetCursorScreenPos(ImVec2(L.controlRight - sliderW, L.p0.y + (L.rowH - ImGui::GetFrameHeight()) * 0.5f));
    ImGui::SetNextItemWidth(sliderW);
    bool changed = ImGui::SliderFloat("##s", v, vmin, vmax, fmt);

    ImGui::PopID();
    return changed;
}

bool IntSliderRow(const char* label, int* v, int vmin, int vmax, const char* subtitle)
{
    ImGui::PushID(label);
    RowLayout L = BeginRow(label, subtitle, 0, false);

    const float sliderW = ImClamp(ImGui::GetContentRegionAvail().x * 0.42f, 120.0f, 240.0f);
    ImGui::SetCursorScreenPos(ImVec2(L.controlRight - sliderW, L.p0.y + (L.rowH - ImGui::GetFrameHeight()) * 0.5f));
    ImGui::SetNextItemWidth(sliderW);
    bool changed = ImGui::SliderInt("##i", v, vmin, vmax);

    ImGui::PopID();
    return changed;
}

bool ButtonRow(const char* label, const char* buttonText, const char* subtitle, bool experimental)
{
    const auto& P = Colors();
    ImGui::PushID(label);
    RowLayout L = BeginRow(label, subtitle, 0, false);

    if (experimental)
    {
        ImVec2 lblSz = ImGui::CalcTextSize(label);
        ImFont* f = SmallFont();
        const char* tag = "EXPERIMENTAL";
        ImVec2 tsz = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, tag);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bp(L.p0.x + 4.0f + lblSz.x + 10.0f, (subtitle ? L.p0.y + 8.0f : L.p0.y + (L.rowH - f->FontSize) * 0.5f) - 2.0f);
        dl->AddRectFilled(bp, ImVec2(bp.x + tsz.x + 12, bp.y + f->FontSize + 4), WithAlpha(P.accent, 0.14f), 4.0f);
        dl->AddText(f, f->FontSize, ImVec2(bp.x + 6, bp.y + 2), P.accent, tag);
    }

    const float bw = 108.0f, bh = 30.0f;
    ImGui::SetCursorScreenPos(ImVec2(L.controlRight - bw, L.p0.y + (L.rowH - bh) * 0.5f));
    bool clicked = AccentButton(buttonText, ImVec2(bw, bh));

    ImGui::PopID();
    return clicked;
}

bool InputTextRow(const char* label, char* buf, size_t bufSize, const char* subtitle)
{
    ImGui::PushID(label);
    RowLayout L = BeginRow(label, subtitle, 0, false);

    const float iw = ImClamp(ImGui::GetContentRegionAvail().x * 0.5f, 160.0f, 320.0f);
    ImGui::SetCursorScreenPos(ImVec2(L.controlRight - iw, L.p0.y + (L.rowH - ImGui::GetFrameHeight()) * 0.5f));
    ImGui::SetNextItemWidth(iw);
    bool changed = ImGui::InputText("##t", buf, bufSize);

    ImGui::PopID();
    return changed;
}

// Standalone widgets
namespace
{
// Custom button: rounded fill + crisp border + perfectly centered label, drawn
// by hand so alignment never depends on ImGui's FramePadding/clip or a loaded
// theme's metrics. `accent` = filled neon style; else a subtle ghost button.
bool DrawButton(const char* label, const ImVec2& sizeArg, bool accent)
{
    const auto& P = Colors();
    ImGui::PushID(label);

    const ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec2 size = sizeArg;
    if (size.x <= 0.0f)
        size.x = ts.x + 28.0f;
    if (size.y <= 0.0f)
        size.y = ts.y + 14.0f;

    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##btn", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked();
    const float hv = Anim(ImGui::GetID("bhv"), hovered ? 1.0f : 0.0f, 16.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 e(p.x + size.x, p.y + size.y);
    const float rounding = ImGui::GetStyle().FrameRounding;

    ImU32 txt;
    if (accent)
    {
        dl->AddRectFilled(p, e, WithAlpha(P.accent, held ? 0.42f : 0.15f + 0.13f * hv), rounding);
        dl->AddRect(p, e, WithAlpha(P.accent, 0.45f + 0.35f * hv), rounding, 0, 1.0f);
        txt = P.accent;
    }
    else
    {
        if (hv > 0.01f || held)
            dl->AddRectFilled(p, e, WithAlpha(P.panelHover, held ? 1.0f : 0.85f * hv), rounding);
        dl->AddRect(p, e, WithAlpha(P.border, 0.6f + 0.4f * hv), rounding, 0, 1.0f);
        txt = hovered ? P.text : P.textDim;
    }

    // Exact-centered label (rounded to whole pixels for crisp text).
    const ImVec2 tp(IM_FLOOR(p.x + (size.x - ts.x) * 0.5f), IM_FLOOR(p.y + (size.y - ts.y) * 0.5f));
    dl->AddText(tp, txt, label);

    ImGui::PopID();
    return clicked;
}
} // namespace

bool AccentButton(const char* label, const ImVec2& size)
{
    return DrawButton(label, size, true);
}

bool GhostButton(const char* label, const ImVec2& size)
{
    return DrawButton(label, size, false);
}

bool KeyCaptureButton(const char* label, int* vkey)
{
    const auto& P = Colors();
    ImGui::PushID(label);
    static ImGuiID s_capturing = 0;
    const ImGuiID self = ImGui::GetID("cap");
    const bool capturing = (s_capturing == self);

    auto keyName = [](int vk) -> std::string
    {
        UINT sc = MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
        wchar_t name[64]{};
        if (vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR ||
            vk == VK_NEXT || vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN)
            sc |= 0x100;
        if (GetKeyNameTextW(static_cast<LONG>(sc) << 16, name, 64) > 0)
        {
            char out[64]{};
            WideCharToMultiByte(CP_UTF8, 0, name, -1, out, sizeof(out), nullptr, nullptr);
            return out;
        }
        char fallback[16];
        snprintf(fallback, sizeof(fallback), "0x%02X", vk);
        return fallback;
    };

    char btn[64];
    snprintf(btn, sizeof(btn), "%s", capturing ? "Press any key..." : keyName(*vkey).c_str());

    bool changed = false;
    ImGui::PushStyleColor(ImGuiCol_Text, capturing ? P.accent : P.text);
    if (ImGui::Button(btn, ImVec2(150, 0)))
        s_capturing = capturing ? 0 : self;
    ImGui::PopStyleColor();

    if (s_capturing == self)
    {
        for (int vk = 0x08; vk <= 0xFE; ++vk)
        {
            if (vk == VK_LBUTTON || vk == VK_RBUTTON)
                continue;
            if (GetAsyncKeyState(vk) & 0x8000)
            {
                if (vk == VK_ESCAPE)
                {
                    s_capturing = 0;
                    break;
                }
                *vkey = vk;
                s_capturing = 0;
                changed = true;
                break;
            }
        }
    }

    ImGui::PopID();
    return changed;
}

void StatBar(const char* label, float value, float maxValue, ImU32 color, const char* overlay)
{
    const auto& P = Colors();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* f = SmallFont();

    ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float barH = 6.0f;
    const float labelH = f->FontSize;

    dl->AddText(f, f->FontSize, p, P.textDim, label);
    if (overlay)
    {
        ImVec2 osz = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, overlay);
        dl->AddText(f, f->FontSize, ImVec2(p.x + w - osz.x, p.y), P.text, overlay);
    }

    ImVec2 b0(p.x, p.y + labelH + 4.0f);
    ImVec2 b1(p.x + w, b0.y + barH);
    dl->AddRectFilled(b0, b1, P.track, barH * 0.5f);
    const float frac = maxValue > 0.0f ? Clamp01(value / maxValue) : 0.0f;
    if (frac > 0.0f)
        dl->AddRectFilled(b0, ImVec2(b0.x + w * frac, b1.y), color, barH * 0.5f);

    ImGui::Dummy(ImVec2(w, labelH + barH + 8.0f));
}
} // namespace cm::gui::widgets
