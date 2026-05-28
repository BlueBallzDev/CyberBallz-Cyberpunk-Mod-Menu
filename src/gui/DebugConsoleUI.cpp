#include "Console.hpp"
#include "Theme.hpp"
#include "core/DebugConsole.hpp"

#include <imgui.h>

#include <cstring>

namespace cm::gui
{
void DrawDebugConsole()
{
    if (!debug::IsOpen())
        return;

    // Keep the cursor visible while the console is up (the main menu only does
    // this when it is itself open).
    ImGui::GetIO().MouseDrawCursor = true;

    const auto& P = theme::Colors();
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 30, vp->Pos.y + vp->Size.y - 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640, 330), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(6, 8, 11, 245));
    ImGui::PushStyleColor(ImGuiCol_Border, theme::WithAlpha(P.accent, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text, P.text);

    bool open = true;
    if (ImGui::Begin("CyberBallz Console  (`)", &open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        // Output log.
        const float footer = ImGui::GetFrameHeightWithSpacing() + 4.0f;
        if (ImGui::BeginChild("##log", ImVec2(0, -footer), true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            for (const auto& line : debug::Output())
            {
                ImU32 col = P.textDim;
                if (!line.empty() && line[0] == '>')
                    col = P.accent;
                else if (line.find("not found") != std::string::npos || line.find("unknown") != std::string::npos)
                    col = P.danger;
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopStyleColor();
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f)
                ImGui::SetScrollHereY(1.0f); // autoscroll when pinned to bottom
        }
        ImGui::EndChild();

        // Input line.
        static char buf[256] = "";
        ImGui::SetNextItemWidth(-90.0f);
        bool submit = ImGui::InputText("##cmd", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Run", ImVec2(80, 0)))
            submit = true;

        if (submit && buf[0])
        {
            debug::Submit(buf);
            buf[0] = '\0';
            ImGui::SetKeyboardFocusHere(-1); // keep focus on the input
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(3);

    if (!open)
        debug::SetOpen(false);
}
} // namespace cm::gui
