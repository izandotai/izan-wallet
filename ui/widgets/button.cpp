#include "ui/widgets/button.hpp"

#include "ui/widgets/design.hpp"

namespace izan::ui {

namespace {

    // The shape token: pill mode swaps the theme rounding for full
    // capsule ends. Returns the rounding in force for the overlay pass.
    float push_button_shape()
    {
        if (!design().button_pill)
            return ImGui::GetStyle().FrameRounding;
        const float r = ImGui::GetFrameHeight() * 0.5f;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, r);
        return r;
    }

    void pop_button_shape()
    {
        if (design().button_pill)
            ImGui::PopStyleVar();
    }

    // The finish token: a light cap falling from the top edge, a shade
    // rising from the bottom, a crisp rim under the crown — the
    // three-stroke approximation of brushed metal.
    void paint_gloss(float rounding)
    {
        const float g = design().button_gloss;
        if (g <= 0.0f)
            return;
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float h = max.y - min.y;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(ImVec2(min.x + 1.0f, min.y + 1.0f),
            ImVec2(max.x - 1.0f, min.y + h * 0.48f),
            IM_COL32(255, 255, 255, int(34.0f * g)), rounding - 1.0f,
            ImDrawFlags_RoundCornersTop);
        draw->AddRectFilled(ImVec2(min.x + 1.0f, max.y - h * 0.34f),
            ImVec2(max.x - 1.0f, max.y - 1.0f),
            IM_COL32(0, 0, 0, int(28.0f * g)), rounding - 1.0f,
            ImDrawFlags_RoundCornersBottom);
        draw->AddLine(ImVec2(min.x + rounding * 0.8f, min.y + 1.0f),
            ImVec2(max.x - rounding * 0.8f, min.y + 1.0f),
            IM_COL32(255, 255, 255, int(64.0f * g)));
    }

    bool filled_button(const char* label, float width, const ImVec4& fill)
    {
        const ImVec4 hover = kit_blend(fill, ImVec4(1, 1, 1, fill.w), 0.12f);
        const ImVec4 active = kit_blend(fill, ImVec4(0, 0, 0, fill.w), 0.12f);
        ImGui::PushStyleColor(ImGuiCol_Button, fill);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.96f));
        const float rounding = push_button_shape();
        const bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
        pop_button_shape();
        ImGui::PopStyleColor(4);
        paint_gloss(rounding);
        return clicked;
    }

}

bool kit_primary_button(const char* label, float width)
{
    ImVec4 accent = kit_accent();
    accent.w = 1.0f;
    return filled_button(label, width, accent);
}

bool kit_danger_button(const char* label, float width)
{
    return filled_button(label, width, kit_danger());
}

bool kit_subtle_button(const char* label, float width)
{
    const float rounding = push_button_shape();
    const bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    pop_button_shape();
    paint_gloss(rounding);
    return clicked;
}

bool kit_link_button(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Text, kit_accent());
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
    const float rounding = push_button_shape();
    (void)rounding; // text buttons carry the shape, not the metal
    const bool clicked = ImGui::Button(label);
    pop_button_shape();
    ImGui::PopStyleColor(4);
    return clicked;
}

}
