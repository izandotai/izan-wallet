#include "ui/widgets/amount_field.hpp"

#include <cstring>

#include <imgui.h>

#include "ui/widgets/design.hpp"

namespace izan::ui {

bool kit_amount_field(const char* id, char* buf, std::size_t size)
{
    const float em = ImGui::GetFontSize();
    const float big = kit_snap(em * 2.1f);
    ImGui::PushFont(nullptr, big);

    // The field hugs its digits and sits on the center axis; an empty
    // entry shows a quiet zero.
    const char* shown = buf[0] ? buf : "0";
    const float text_w = ImGui::CalcTextSize(shown).x;
    const float min_w = ImGui::CalcTextSize("000").x;
    const float w = (text_w > min_w ? text_w : min_w) + em;
    const float slack = ImGui::GetContentRegionAvail().x - w;
    if (slack > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + slack * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::SetNextItemWidth(w);
    const bool submitted = ImGui::InputTextWithHint(id, "0", buf, size,
        ImGuiInputTextFlags_CharsDecimal
            | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    return submitted;
}

}
