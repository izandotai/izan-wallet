#include "ui/widgets/menu.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include "ui/widgets/avatar.hpp"
#include "ui/widgets/design.hpp"
#include "ui/widgets/label.hpp"

namespace izan::ui {

namespace {

    // The shell's dropdown numbers, mirrored exactly — one rhythm for
    // every menu in the app (chrome_widgets pushes the same values on
    // the menu bar's own popups).
    void push_menu_style()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 11.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 12.0f));
        ImVec4 popup_bg = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
        popup_bg.w = 1.0f;
        ImGui::PushStyleColor(ImGuiCol_PopupBg, popup_bg);
    }

    void pop_menu_style()
    {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

}

bool kit_menu_begin(const char* id)
{
    push_menu_style();
    const bool open = ImGui::BeginPopup(id);
    if (!open) {
        pop_menu_style();
        return false;
    }
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    return true;
}

void kit_menu_end()
{
    ImGui::PopItemFlag();
    ImGui::EndPopup();
    pop_menu_style();
}

bool kit_menu_item(
    const char* label, const char* trailing, bool selected, bool enabled)
{
    return ImGui::MenuItem(label, trailing, selected, enabled);
}

bool kit_menu_item_icon(const char* avatar_name, const char* label,
    const char* trailing, bool selected)
{
    const float em = ImGui::GetFontSize();
    const float av = em * 1.2f;
    const float row_h = em * 1.7f;
    const float gap = em * 0.5f;
    float w = av + gap + ImGui::CalcTextSize(label).x;
    float trailing_w = 0.0f;
    if (trailing && *trailing) {
        trailing_w
            = ImGui::GetFont()
                  ->CalcTextSizeA(kit_caption_size(), FLT_MAX, 0.0f, trailing)
                  .x;
        w += em * 1.6f + trailing_w;
    }

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked
        = ImGui::Selectable("##row", selected, 0, ImVec2(w, row_h));
    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    kit_avatar_at(ImVec2(pos.x, pos.y + (row_h - av) * 0.5f), avatar_name, av);
    draw->AddText(ImVec2(kit_snap(pos.x + av + gap),
                      kit_snap(pos.y + (row_h - em) * 0.5f)),
        ImGui::GetColorU32(ImGuiCol_Text), label);
    if (trailing_w > 0.0f)
        draw->AddText(ImGui::GetFont(), kit_caption_size(),
            ImVec2(kit_snap(pos.x + w - trailing_w),
                kit_snap(pos.y + (row_h - kit_caption_size()) * 0.5f)),
            ImGui::GetColorU32(ImGuiCol_TextDisabled), trailing);
    return clicked;
}

}
