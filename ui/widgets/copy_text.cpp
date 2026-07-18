#include "ui/widgets/copy_text.hpp"

#include <string>

#include <imgui.h>

#include "ui/widgets/design.hpp"
#include "ui/widgets/label.hpp"
#include "ui/widgets/tooltip.hpp"

namespace izan::ui {

namespace {

    constexpr double kFeedbackSeconds = 1.6;

    void copy_text_impl(const char* id, const char* full, const char* hint,
        const char* copied_label, bool right_align, float reserve_right_em)
    {
        ImGuiStorage* storage = ImGui::GetStateStorage();
        const ImGuiID key = ImGui::GetID(id);
        const bool fresh
            = ImGui::GetTime() - double(storage->GetFloat(key, -1000.0f))
            < kFeedbackSeconds;

        // Never glued to the previous item: a breath of space stays
        // even when the row runs tight, and the text shrinks to fit.
        const float gap = ImGui::GetFontSize() * 0.6f;
        const float reserve = ImGui::GetFontSize() * reserve_right_em;
        const float avail = ImGui::GetContentRegionAvail().x - gap - reserve;
        const std::string text
            = kit_elide_middle(fresh ? copied_label : full, avail);
        if (right_align) {
            const float min_x = ImGui::GetCursorPosX() + gap;
            const float edge = ImGui::GetCursorPosX()
                + ImGui::GetContentRegionAvail().x - reserve;
            const float x = edge - ImGui::CalcTextSize(text.c_str()).x;
            ImGui::SetCursorPosX(x > min_x ? x : min_x);
        }
        if (fresh) {
            ImGui::TextColored(kit_accent(), "%s", text.c_str());
            return;
        }
        ImGui::TextUnformatted(text.c_str());
        if (ImGui::IsItemClicked()) {
            ImGui::SetClipboardText(full);
            storage->SetFloat(key, float(ImGui::GetTime()));
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            kit_tooltip_lines(full, hint);
        }
    }

}

void kit_copy_text(const char* id, const char* full, const char* hint,
    const char* copied_label)
{
    copy_text_impl(id, full, hint, copied_label, false, 0.0f);
}

void kit_copy_text_right(const char* id, const char* full, const char* hint,
    const char* copied_label, float reserve_right_em)
{
    copy_text_impl(id, full, hint, copied_label, true, reserve_right_em);
}

}
