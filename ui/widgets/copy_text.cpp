#include "ui/widgets/copy_text.hpp"

#include <imgui.h>

#include "ui/widgets/design.hpp"
#include "ui/widgets/label.hpp"
#include "ui/widgets/tooltip.hpp"

namespace izan::ui {

namespace {

    constexpr double kFeedbackSeconds = 1.6;

    void copy_text_impl(const char* id, const char* shown, const char* full,
        const char* hint, const char* copied_label, bool right_align)
    {
        ImGuiStorage* storage = ImGui::GetStateStorage();
        const ImGuiID key = ImGui::GetID(id);
        const bool fresh
            = ImGui::GetTime() - double(storage->GetFloat(key, -1000.0f))
            < kFeedbackSeconds;

        const char* text = fresh ? copied_label : shown;
        ImVec2 size = ImGui::CalcTextSize(text);
        if (right_align) {
            const float edge
                = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(size.x < edge ? edge - size.x : 0.0f);
        }
        if (fresh) {
            ImGui::TextColored(kit_accent(), "%s", text);
            return;
        }
        ImGui::TextUnformatted(text);
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

void kit_copy_text(const char* id, const char* shown, const char* full,
    const char* hint, const char* copied_label)
{
    copy_text_impl(id, shown, full, hint, copied_label, false);
}

void kit_copy_text_right(const char* id, const char* shown, const char* full,
    const char* hint, const char* copied_label)
{
    copy_text_impl(id, shown, full, hint, copied_label, true);
}

}
