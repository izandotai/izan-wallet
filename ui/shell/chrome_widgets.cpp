#include "ui/shell/chrome_widgets.hpp"

#include "ui/shell/constants.hpp"
#include "ui/shell/theme.hpp"

#include <imgui_internal.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace izan::ui {

namespace {

    bool rects_overlap(const ImRect& lhs, const ImRect& rhs)
    {
        return lhs.Min.x < rhs.Max.x && lhs.Max.x > rhs.Min.x
            && lhs.Min.y < rhs.Max.y && lhs.Max.y > rhs.Min.y;
    }

    void append_rect_minus_blocker(
        std::vector<ImRect>& output, const ImRect& rect, const ImRect& blocker)
    {
        if (!rects_overlap(rect, blocker)) {
            output.push_back(rect);
            return;
        }

        const float ix0 = std::max(rect.Min.x, blocker.Min.x);
        const float iy0 = std::max(rect.Min.y, blocker.Min.y);
        const float ix1 = std::min(rect.Max.x, blocker.Max.x);
        const float iy1 = std::min(rect.Max.y, blocker.Max.y);

        if (rect.Min.y < iy0)
            output.emplace_back(
                ImVec2(rect.Min.x, rect.Min.y), ImVec2(rect.Max.x, iy0));
        if (iy1 < rect.Max.y)
            output.emplace_back(
                ImVec2(rect.Min.x, iy1), ImVec2(rect.Max.x, rect.Max.y));
        if (rect.Min.x < ix0)
            output.emplace_back(ImVec2(rect.Min.x, iy0), ImVec2(ix0, iy1));
        if (ix1 < rect.Max.x)
            output.emplace_back(ImVec2(ix1, iy0), ImVec2(rect.Max.x, iy1));
    }

    void draw_shadow_rect_clipped(ImDrawList* draw_list, const ImRect& rect,
        const std::vector<ImRect>& blockers, ImU32 color, float rounding,
        ImDrawFlags flags)
    {
        std::vector<ImRect> segments { rect };
        for (const ImRect& blocker : blockers) {
            std::vector<ImRect> next_segments;
            for (const ImRect& segment : segments)
                append_rect_minus_blocker(next_segments, segment, blocker);
            segments = std::move(next_segments);
            if (segments.empty())
                return;
        }

        for (const ImRect& segment : segments) {
            if (segment.GetWidth() <= 0.0f || segment.GetHeight() <= 0.0f)
                continue;
            draw_list->AddRectFilled(
                segment.Min, segment.Max, color, rounding, flags);
        }
    }

    void draw_popup_shadow_for_rect(ImDrawList* draw_list,
        const ChromeState& app, const ImRect& rect,
        const std::vector<ImRect>& blockers)
    {
        constexpr std::array<float, 12> kShadowAlpha
            = { 0.020f, 0.017f, 0.014f, 0.011f, 0.0085f, 0.0065f, 0.0050f,
                  0.0038f, 0.0028f, 0.0020f, 0.0014f, 0.0010f };
        const float base_rounding = ImGui::GetStyle().PopupRounding;
        for (int index = static_cast<int>(kShadowAlpha.size()) - 1; index >= 0;
            --index) {
            const float layer = static_cast<float>(index + 1);
            const float spread = 2.0f + layer * 1.75f;
            const ImU32 color = theme_popup_shadow_color(
                app, kShadowAlpha[static_cast<std::size_t>(index)]);
            const float rounding = base_rounding + spread;

            draw_shadow_rect_clipped(draw_list,
                ImRect(ImVec2(rect.Min.x - spread, rect.Min.y - spread),
                    ImVec2(rect.Max.x + spread, rect.Max.y + spread)),
                blockers, color, rounding, 0);
        }
    }

    void draw_snap_preview(const ChromeState& app, ImDrawList* draw_list,
        const ImVec2& min, const ImVec2& max,
        const std::vector<ImVec4>& regions, bool hovered)
    {
        const ImU32 border = theme_snap_preview_border_color(app, hovered);
        const ImU32 fill = theme_snap_preview_fill_color(app, hovered);
        draw_list->AddRectFilled(
            min, max, theme_snap_preview_background_color(app), 4.0f);
        draw_list->AddRect(min, max, border, 4.0f, 0, 1.5f);
        for (const ImVec4& region : regions) {
            const ImVec2 region_min(min.x + (max.x - min.x) * region.x,
                min.y + (max.y - min.y) * region.y);
            const ImVec2 region_max(min.x + (max.x - min.x) * region.z,
                min.y + (max.y - min.y) * region.w);
            draw_list->AddRectFilled(region_min, region_max, fill, 2.0f);
            draw_list->AddRect(region_min, region_max, border, 2.0f, 0, 1.0f);
        }
    }

    bool snap_layout_item(const ChromeState& app, const char* id,
        const ImVec2& size, const std::vector<ImVec4>& regions)
    {
        ImGui::InvisibleButton(id, size);
        const bool hovered = ImGui::IsItemHovered();
        draw_snap_preview(app, ImGui::GetWindowDrawList(),
            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), regions, hovered);
        return ImGui::IsItemClicked();
    }

}

void draw_menu_popup_shadows(const ChromeState& app)
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr)
        return;
    std::vector<std::pair<ImGuiWindow*, ImRect>> popup_rects;
    for (ImGuiWindow* window : context->Windows) {
        if (window == nullptr || !window->WasActive)
            continue;
        const bool popup = (window->Flags & ImGuiWindowFlags_Popup) != 0;
        const bool modal = (window->Flags & ImGuiWindowFlags_Modal) != 0;
        const bool tooltip = (window->Flags & ImGuiWindowFlags_Tooltip) != 0;
        if (!popup || modal || tooltip)
            continue;
        popup_rects.emplace_back(window,
            ImRect(window->Pos,
                ImVec2(window->Pos.x + window->Size.x,
                    window->Pos.y + window->Size.y)));
    }
    if (popup_rects.empty())
        return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoBackground;
    ImGui::Begin("izan-popup-shadow-layer", nullptr, flags);
    ImGuiWindow* shadow_window = ImGui::GetCurrentWindow();
    if (shadow_window != nullptr)
        ImGui::BringWindowToDisplayBehind(
            shadow_window, popup_rects.front().first);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRectFullScreen();

    for (std::size_t index = 0; index < popup_rects.size(); ++index) {
        std::vector<ImRect> blockers;
        for (std::size_t blocker_index = 0; blocker_index < popup_rects.size();
            ++blocker_index) {
            if (blocker_index != index)
                blockers.push_back(popup_rects[blocker_index].second);
        }
        draw_popup_shadow_for_rect(
            draw_list, app, popup_rects[index].second, blockers);
    }
    draw_list->PopClipRect();
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void draw_main_window_frame(const ChromeState& app)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList(viewport);
    const ImVec2 min(viewport->Pos.x + kWindowFrameMargin,
        viewport->Pos.y + kWindowFrameMargin);
    const ImVec2 max(viewport->Pos.x + viewport->Size.x - kWindowFrameMargin,
        viewport->Pos.y + viewport->Size.y - kWindowFrameMargin);
    const ImVec2 chrome_max(max.x, min.y + kTitleBarHeight + kMenuBarHeight);

    draw_list->AddRectFilled(min, max, theme_frame_background_color(app), 8.0f);
    draw_list->AddRectFilled(min, chrome_max,
        theme_chrome_background_color(app), 8.0f, ImDrawFlags_RoundCornersTop);
    draw_list->AddLine(ImVec2(min.x, min.y + kTitleBarHeight),
        ImVec2(max.x, min.y + kTitleBarHeight),
        theme_chrome_line_color(app, 200), 1.0f);
    draw_list->AddLine(ImVec2(min.x, min.y + kTitleBarHeight + kMenuBarHeight),
        ImVec2(max.x, min.y + kTitleBarHeight + kMenuBarHeight),
        theme_chrome_line_color(app, 180), 1.0f);
}

bool draw_window_control_button(
    const char* id, const ImVec2& size, WindowControlIcon icon)
{
    const bool clicked = ImGui::Button(id, size);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    const float cx = (min.x + max.x) * 0.5f;
    const float cy = (min.y + max.y) * 0.5f;
    const float stroke = 2.0f;

    switch (icon) {
    case WindowControlIcon::Minimize:
        draw_list->AddLine(ImVec2(cx - 9.0f, cy + 7.0f),
            ImVec2(cx + 9.0f, cy + 7.0f), color, stroke);
        break;
    case WindowControlIcon::Maximize:
        draw_list->AddRect(ImVec2(cx - 9.0f, cy - 9.0f),
            ImVec2(cx + 9.0f, cy + 9.0f), color, 0.0f, 0, stroke);
        break;
    case WindowControlIcon::Restore:
        draw_list->AddRect(ImVec2(cx - 6.0f, cy - 10.0f),
            ImVec2(cx + 10.0f, cy + 6.0f), color, 0.0f, 0, stroke);
        draw_list->AddRect(ImVec2(cx - 10.0f, cy - 6.0f),
            ImVec2(cx + 6.0f, cy + 10.0f), color, 0.0f, 0, stroke);
        break;
    case WindowControlIcon::Close:
        draw_list->AddLine(ImVec2(cx - 8.0f, cy - 8.0f),
            ImVec2(cx + 8.0f, cy + 8.0f), color, stroke);
        draw_list->AddLine(ImVec2(cx + 8.0f, cy - 8.0f),
            ImVec2(cx - 8.0f, cy + 8.0f), color, stroke);
        break;
    }

    return clicked;
}

void draw_simple_tooltip(const ChromeState& app, const char* id,
    const char* text, const ImVec2& anchor)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 padding(10.0f, 6.0f);
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    const ImVec2 tooltip_size(
        text_size.x + padding.x * 2.0f, text_size.y + padding.y * 2.0f);
    ImVec2 tooltip_pos(anchor.x - tooltip_size.x * 0.5f, anchor.y);
    tooltip_pos.x = std::clamp(tooltip_pos.x, viewport->Pos.x + 8.0f,
        viewport->Pos.x + viewport->Size.x - tooltip_size.x - 8.0f);
    tooltip_pos.y = std::clamp(tooltip_pos.y, viewport->Pos.y + 8.0f,
        viewport->Pos.y + viewport->Size.y - tooltip_size.y - 8.0f);

    ImGui::SetNextWindowPos(tooltip_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(tooltip_size, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(viewport->ID);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
    ImGui::PushStyleColor(ImGuiCol_Border, theme_chrome_line_color(app, 180));
    ImGui::Begin(id, nullptr, flags);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImGui::TextUnformatted(text);
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void queue_simple_tooltip(
    ChromeState& app, const char* text, const ImVec2& anchor)
{
    app.pending_tooltip_visible = true;
    app.pending_tooltip_text = text;
    app.pending_tooltip_anchor = anchor;
}

void draw_snap_layout_popup(GLFWwindow* window, ChromeState& app)
{
    if (!app.snap_layout_open)
        return;

    // The frame is sized from its contents: 4 columns × 2 rows plus the
    // inter-row Dummy(10).
    const ImVec2 item_size(70.0f, 52.0f);
    const float gap = 12.0f;
    const ImVec2 pad(16.0f, 14.0f);
    const float spacing_y = ImGui::GetStyle().ItemSpacing.y;
    const ImVec2 popup_size(pad.x * 2.0f + item_size.x * 4.0f + gap * 3.0f,
        pad.y * 2.0f + item_size.y * 2.0f + 10.0f + spacing_y * 2.0f);
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 popup_pos(app.snap_layout_anchor.x - popup_size.x + 56.0f,
        app.snap_layout_anchor.y + 8.0f);
    popup_pos.x = std::clamp(popup_pos.x, viewport->Pos.x + 12.0f,
        viewport->Pos.x + viewport->Size.x - popup_size.x - 12.0f);
    popup_pos.y = std::clamp(popup_pos.y, viewport->Pos.y + 12.0f,
        viewport->Pos.y + viewport->Size.y - popup_size.y - 12.0f);

    ImGui::SetNextWindowPos(popup_pos);
    ImGui::SetNextWindowSize(popup_size);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, pad);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
    ImGui::PushStyleColor(ImGuiCol_Border, theme_chrome_line_color(app, 170));
    ImGui::Begin("izan-snap-layouts", nullptr, flags);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    bool used_layout = false;

    if (snap_layout_item(app, "##snap-left", item_size,
            { ImVec4(0.0f, 0.0f, 0.5f, 1.0f) })) {
        snap_window_to_work_area(window, 0.0f, 0.0f, 0.5f, 1.0f);
        used_layout = true;
    }
    ImGui::SameLine(0.0f, gap);
    if (snap_layout_item(app, "##snap-right", item_size,
            { ImVec4(0.5f, 0.0f, 1.0f, 1.0f) })) {
        snap_window_to_work_area(window, 0.5f, 0.0f, 0.5f, 1.0f);
        used_layout = true;
    }
    ImGui::SameLine(0.0f, gap);
    if (snap_layout_item(app, "##snap-full", item_size,
            { ImVec4(0.0f, 0.0f, 1.0f, 1.0f) })) {
        glfwMaximizeWindow(window);
        used_layout = true;
    }
    ImGui::SameLine(0.0f, gap);
    if (snap_layout_item(
            app, "##snap-top", item_size, { ImVec4(0.0f, 0.0f, 1.0f, 0.5f) })) {
        snap_window_to_work_area(window, 0.0f, 0.0f, 1.0f, 0.5f);
        used_layout = true;
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    if (snap_layout_item(app, "##snap-left-two-thirds", item_size,
            { ImVec4(0.0f, 0.0f, 0.66f, 1.0f) })) {
        snap_window_to_work_area(window, 0.0f, 0.0f, 0.66f, 1.0f);
        used_layout = true;
    }
    ImGui::SameLine(0.0f, gap);
    if (snap_layout_item(app, "##snap-right-third", item_size,
            { ImVec4(0.66f, 0.0f, 1.0f, 1.0f) })) {
        snap_window_to_work_area(window, 0.66f, 0.0f, 0.34f, 1.0f);
        used_layout = true;
    }
    ImGui::SameLine(0.0f, gap);
    if (snap_layout_item(app, "##snap-bottom", item_size,
            { ImVec4(0.0f, 0.5f, 1.0f, 1.0f) })) {
        snap_window_to_work_area(window, 0.0f, 0.5f, 1.0f, 0.5f);
        used_layout = true;
    }
    ImGui::SameLine(0.0f, gap);
    if (snap_layout_item(app, "##snap-center", item_size,
            { ImVec4(0.18f, 0.12f, 0.82f, 0.88f) })) {
        snap_window_to_work_area(window, 0.15f, 0.10f, 0.70f, 0.80f);
        used_layout = true;
    }

    const bool popup_hovered
        = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);

    if (used_layout) {
        app.snap_layout_open = false;
        app.snap_layout_hover_started = -1.0;
        return;
    }
    const ImVec2 button_min(app.snap_layout_button_min.x - 10.0f,
        app.snap_layout_button_min.y - 10.0f);
    const ImVec2 button_max(app.snap_layout_button_max.x + 10.0f,
        app.snap_layout_button_max.y + 16.0f);
    const bool button_hovered
        = ImGui::IsMouseHoveringRect(button_min, button_max);
    if (popup_hovered || button_hovered) {
        app.snap_layout_last_hovered = ImGui::GetTime();
        return;
    }
    if (app.snap_layout_last_hovered < 0.0
        || ImGui::GetTime() - app.snap_layout_last_hovered > 0.20) {
        app.snap_layout_open = false;
        app.snap_layout_hover_started = -1.0;
        app.snap_layout_last_hovered = -1.0;
    }
}

void draw_custom_title_bar(GLFWwindow* window, ChromeState& app,
    const char* title_text, const char* subtitle_text)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 title_min(viewport->Pos.x + kWindowFrameMargin,
        viewport->Pos.y + kWindowFrameMargin);
    const ImVec2 title_size(
        viewport->Size.x - kWindowFrameMargin * 2.0f, kTitleBarHeight);
    const ImVec2 title_max(
        title_min.x + title_size.x, title_min.y + title_size.y);
    const ImGuiStyle& style = ImGui::GetStyle();
    const float text_y = title_min.y
        + std::floor((kTitleBarHeight - ImGui::GetTextLineHeight()) * 0.5f);
    const float frame_height = ImGui::GetFrameHeight();
    const float frame_y
        = title_min.y + std::floor((kTitleBarHeight - frame_height) * 0.5f);
    const bool show_subtitle = title_size.x >= 620.0f;
    const float button_width = 46.0f;
    const float button_height = frame_height;
    const float slider_width = 112.0f;
    const float opacity_label_width = ImGui::CalcTextSize("opacity").x;
    const float window_buttons_width
        = button_width * 3.0f + style.ItemSpacing.x * 2.0f;
    const float opacity_width = slider_width + style.ItemSpacing.x
        + opacity_label_width + style.ItemSpacing.x;
    const bool show_opacity = title_size.x >= 900.0f;
    const float controls_width
        = window_buttons_width + (show_opacity ? opacity_width : 0.0f);
    const float controls_x = title_max.x - controls_width - 10.0f;
    const ImVec2 empty_region {};
    const ImVec2 controls_min(controls_x - 8.0f, title_min.y);
    const ImVec2 controls_max(title_max.x, title_max.y);

    ImGui::SetNextWindowPos(title_min);
    ImGui::SetNextWindowSize(title_size);
    ImGui::SetNextWindowViewport(viewport->ID);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    update_title_bar_hit_regions(window, title_min, title_max, empty_region,
        empty_region, controls_min, controls_max);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, theme_transparent_chrome_window_color(app));
    ImGui::Begin("izan-titlebar", nullptr, flags);

    ImGui::SetCursorScreenPos(ImVec2(title_min.x + 16.0f, text_y));
    ImGui::TextUnformatted(title_text);
    if (show_subtitle && subtitle_text != nullptr) {
        // Subtitle follows the title's measured width; a fixed offset
        // overlaps once the title runs long.
        const float subtitle_x
            = title_min.x + 16.0f + ImGui::CalcTextSize(title_text).x + 24.0f;
        ImGui::SetCursorScreenPos(ImVec2(subtitle_x, text_y));
        ImGui::TextDisabled("%s", subtitle_text);
    }

    ImGui::SetCursorScreenPos(ImVec2(controls_x, frame_y));
    if (show_opacity) {
        int opacity_percent
            = static_cast<int>(std::lround(app.window_opacity * 100.0f));
        ImGui::SetNextItemWidth(slider_width);
        if (ImGui::SliderInt(
                "##window-opacity", &opacity_percent, 62, 100, "%d%%")) {
            app.window_opacity = static_cast<float>(opacity_percent) / 100.0f;
            glfwSetWindowOpacity(window, app.window_opacity);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("opacity");
        ImGui::SameLine();
    }
    if (draw_window_control_button("##window-minimize",
            ImVec2(button_width, button_height), WindowControlIcon::Minimize))
        glfwIconifyWindow(window);
    update_title_bar_button_hit_region(window, WindowControlIcon::Minimize,
        ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        queue_simple_tooltip(app, "Minimize",
            ImVec2(ImGui::GetItemRectMin().x + button_width * 0.5f,
                ImGui::GetItemRectMax().y + 8.0f));
    ImGui::SameLine();
    const WindowControlIcon maximize_icon
        = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE
        ? WindowControlIcon::Restore
        : WindowControlIcon::Maximize;
    if (draw_window_control_button("##window-maximize",
            ImVec2(button_width, button_height), maximize_icon))
        toggle_window_maximized(window);
    const bool maximize_hovered
        = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    const ImVec2 maximize_min = ImGui::GetItemRectMin();
    const ImVec2 maximize_max = ImGui::GetItemRectMax();
    app.snap_layout_button_min = maximize_min;
    app.snap_layout_button_max = maximize_max;
    update_title_bar_button_hit_region(window, maximize_icon,
        ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    if (maximize_hovered) {
        app.snap_layout_last_hovered = ImGui::GetTime();
        if (app.snap_layout_hover_started < 0.0)
            app.snap_layout_hover_started = ImGui::GetTime();
        app.snap_layout_anchor
            = ImVec2((maximize_min.x + maximize_max.x) * 0.5f, maximize_max.y);
        if (ImGui::GetTime() - app.snap_layout_hover_started > 0.25) {
            app.snap_layout_open = true;
        } else if (!app.snap_layout_open) {
            queue_simple_tooltip(app,
                maximize_icon == WindowControlIcon::Restore ? "Restore"
                                                            : "Maximize",
                ImVec2(
                    app.snap_layout_anchor.x, app.snap_layout_anchor.y + 8.0f));
        }
    } else if (!app.snap_layout_open) {
        app.snap_layout_hover_started = -1.0;
        app.snap_layout_last_hovered = -1.0;
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(196, 43, 59, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(150, 30, 46, 255));
    if (draw_window_control_button("##window-close",
            ImVec2(button_width, button_height), WindowControlIcon::Close))
        app.request_exit = true;
    update_title_bar_button_hit_region(window, WindowControlIcon::Close,
        ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        queue_simple_tooltip(app, "Close",
            ImVec2(ImGui::GetItemRectMin().x + button_width * 0.5f,
                ImGui::GetItemRectMax().y + 8.0f));
    ImGui::PopStyleColor(2);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void draw_custom_menu_bar(
    ChromeState& app, const std::function<void()>& draw_items)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 menu_min(viewport->Pos.x + kWindowFrameMargin,
        viewport->Pos.y + kWindowFrameMargin + kTitleBarHeight);
    const ImVec2 menu_size(
        viewport->Size.x - kWindowFrameMargin * 2.0f, kMenuBarHeight);

    ImGui::SetNextWindowPos(menu_min);
    ImGui::SetNextWindowSize(menu_size);
    ImGui::SetNextWindowViewport(viewport->ID);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(16.0f, 0.0f));
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, theme_chrome_background_color(app));
    ImGui::PushStyleColor(
        ImGuiCol_MenuBarBg, theme_chrome_background_color(app));
    ImGui::Begin("izan-menubar", nullptr, flags);

    if (ImGui::BeginMenuBar()) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 11.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
        ImVec4 popup_bg = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
        popup_bg.w = 1.0f;
        ImVec4 window_bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        window_bg.w = 1.0f;
        ImGui::PushStyleColor(ImGuiCol_PopupBg, popup_bg);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, window_bg);
        if (draw_items)
            draw_items();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
        ImGui::EndMenuBar();
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(5);
}

void draw_status_bar(const ChromeState& app, const char* status_text)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    constexpr float height = kStatusBarHeight;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + kWindowFrameMargin,
        viewport->WorkPos.y + viewport->WorkSize.y - kWindowFrameMargin
            - height));
    ImGui::SetNextWindowSize(
        ImVec2(viewport->WorkSize.x - kWindowFrameMargin * 2.0f, height));
    ImGui::SetNextWindowViewport(viewport->ID);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, theme_chrome_background_color(app));
    ImGui::Begin("StatusBar", nullptr, flags);
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowSize().x, min.y);
    ImGui::GetWindowDrawList()->AddLine(
        min, max, theme_chrome_line_color(app, 150), 1.0f);
    ImGui::TextUnformatted(status_text);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ---- Dock ratio guardian ----
// ImGui's native dock allocation lets the central node absorb every
// size delta while other panes stay pixel-locked, so an outer resize
// or a drag on one splitter skews unrelated panes. The ratio ledger is
// the source of truth: enforced every frame, only the dragged splitter
// learns, and a double-click rewrites that splitter to 0.5.

namespace {

    std::unordered_map<unsigned int, float> g_dock_ratio; // node → ratio

    bool dock_is_split(ImGuiDockNode* n)
    {
        return n != nullptr && n->ChildNodes[0] != nullptr
            && n->ChildNodes[1] != nullptr && n->ChildNodes[0]->IsVisible
            && n->ChildNodes[1]->IsVisible;
    }

    float dock_cur_ratio(ImGuiDockNode* n)
    {
        ImGuiDockNode* a = n->ChildNodes[0];
        ImGuiDockNode* b = n->ChildNodes[1];
        const bool horiz = n->SplitAxis == ImGuiAxis_X;
        const float sa = horiz ? a->Size.x : a->Size.y;
        const float sb = horiz ? b->Size.x : b->Size.y;
        const float t = sa + sb;
        return t > 1.0f ? sa / t : 0.5f;
    }

    void dock_apply_ratio(ImGuiDockNode* n, float r)
    {
        ImGuiDockNode* a = n->ChildNodes[0];
        ImGuiDockNode* b = n->ChildNodes[1];
        if (n->SplitAxis == ImGuiAxis_X) {
            const float t = a->Size.x + b->Size.x;
            a->Size.x = a->SizeRef.x = t * r;
            b->Size.x = b->SizeRef.x = t - a->Size.x;
        } else {
            const float t = a->Size.y + b->Size.y;
            a->Size.y = a->SizeRef.y = t * r;
            b->Size.y = b->SizeRef.y = t - a->Size.y;
        }
    }

    // The node owning the seam under the mouse (±8px band); shared by
    // double-click reset and drag learning.
    ImGuiDockNode* dock_seam_hit(ImGuiDockNode* n, const ImVec2& m)
    {
        if (n == nullptr)
            return nullptr;
        if (dock_is_split(n)) {
            ImGuiDockNode* a = n->ChildNodes[0];
            ImGuiDockNode* b = n->ChildNodes[1];
            ImRect seam;
            if (n->SplitAxis == ImGuiAxis_X) {
                seam.Min = ImVec2(
                    a->Pos.x + a->Size.x - 8.0f, ImMax(a->Pos.y, b->Pos.y));
                seam.Max = ImVec2(b->Pos.x + 8.0f,
                    ImMin(a->Pos.y + a->Size.y, b->Pos.y + b->Size.y));
            } else {
                seam.Min = ImVec2(
                    ImMax(a->Pos.x, b->Pos.x), a->Pos.y + a->Size.y - 8.0f);
                seam.Max
                    = ImVec2(ImMin(a->Pos.x + a->Size.x, b->Pos.x + b->Size.x),
                        b->Pos.y + 8.0f);
            }
            if (seam.Contains(m))
                return n;
        }
        if (ImGuiDockNode* r = dock_seam_hit(n->ChildNodes[0], m))
            return r;
        return dock_seam_hit(n->ChildNodes[1], m);
    }

    // Double-click resets only the clicked splitter. Resetting the
    // whole subtree was tried and reverted: with the guardian active no
    // drag can skew other splitters anymore, and the collateral reset
    // clobbered panes the user had deliberately collapsed.
    void dock_set_ratio_one(ImGuiDockNode* n, float r)
    {
        if (n != nullptr && dock_is_split(n))
            g_dock_ratio[n->ID] = r;
    }

    // Rewrites SizeRef through the tree for a given node size, using
    // the ledger where it has an entry and the current ratio where it
    // does not.
    void dock_prepass_apply(ImGuiDockNode* n, ImVec2 avail)
    {
        if (n == nullptr)
            return;
        if (!dock_is_split(n)) {
            dock_prepass_apply(n->ChildNodes[0], avail);
            dock_prepass_apply(n->ChildNodes[1], avail);
            return;
        }
        ImGuiDockNode* a = n->ChildNodes[0];
        ImGuiDockNode* b = n->ChildNodes[1];
        const int axis = int(n->SplitAxis);
        const auto it = g_dock_ratio.find(n->ID);
        const float r
            = it != g_dock_ratio.end() ? it->second : dock_cur_ratio(n);
        const float spacing = ImGui::GetStyle().DockingSeparatorSize;
        const float total = ImMax((&avail.x)[axis] - spacing, 0.0f);
        const float sa = ImTrunc(total * r);
        (&a->SizeRef.x)[axis] = sa;
        (&b->SizeRef.x)[axis] = total - sa;
        ImVec2 availA = avail, availB = avail;
        (&availA.x)[axis] = sa;
        (&availB.x)[axis] = total - sa;
        dock_prepass_apply(a, availA);
        dock_prepass_apply(b, availB);
    }

    // Ratios staged by import, waiting for their split nodes to
    // materialize; indexed by depth-first split order.
    std::vector<float> g_dock_pending;

    // Structural split test — children exist, visibility ignored. At
    // startup the ledger must be seeded before any window has been
    // submitted, when nothing is visible yet.
    bool dock_is_split_structural(ImGuiDockNode* n)
    {
        return n != nullptr && n->ChildNodes[0] != nullptr
            && n->ChildNodes[1] != nullptr;
    }

    // First meeting with a split the session holds no opinion on:
    // install the imported ratio for its depth-first position, falling
    // back to the ratio the ini restored into SizeRef. Without this an
    // unseeded ledger learns whatever imgui's restore produced —
    // mangled sizes or the 0.5 fallback: the "sidebars snap back to
    // half on every launch" bug. Nothing here overwrites what a drag
    // has already taught this session.
    void dock_seed_ledger(ImGuiDockNode* n, std::size_t& k)
    {
        if (n == nullptr)
            return;
        if (dock_is_split_structural(n)) {
            if (!g_dock_ratio.contains(n->ID)) {
                const float pending
                    = k < g_dock_pending.size() ? g_dock_pending[k] : 0.0f;
                if (pending > 0.02f && pending < 0.98f) {
                    g_dock_ratio.emplace(n->ID, pending);
                } else {
                    const int axis = int(n->SplitAxis);
                    const float sa = (&n->ChildNodes[0]->SizeRef.x)[axis];
                    const float sb = (&n->ChildNodes[1]->SizeRef.x)[axis];
                    if (sa + sb > 1.0f)
                        g_dock_ratio.emplace(n->ID, sa / (sa + sb));
                }
            }
            ++k;
        }
        dock_seed_ledger(n->ChildNodes[0], k);
        dock_seed_ledger(n->ChildNodes[1], k);
    }

    void dock_collect_ratios(ImGuiDockNode* n, std::vector<float>& out)
    {
        if (n == nullptr)
            return;
        if (dock_is_split_structural(n)) {
            const auto it = g_dock_ratio.find(n->ID);
            out.push_back(
                it != g_dock_ratio.end() ? it->second : dock_cur_ratio(n));
        }
        dock_collect_ratios(n->ChildNodes[0], out);
        dock_collect_ratios(n->ChildNodes[1], out);
    }

    void dock_walk_keep(ImGuiDockNode* n, ImGuiDockNode* learning)
    {
        if (n == nullptr)
            return;
        if (dock_is_split(n)) {
            const float cur = dock_cur_ratio(n);
            auto it = g_dock_ratio.find(n->ID);
            if (n == learning || it == g_dock_ratio.end()) {
                g_dock_ratio[n->ID] = cur;       // dragged or first seen: learn
            } else if (cur < it->second - 0.002f || cur > it->second + 0.002f) {
                dock_apply_ratio(n, it->second); // others: hold the ledger
            }
        }
        dock_walk_keep(n->ChildNodes[0], learning);
        dock_walk_keep(n->ChildNodes[1], learning);
    }

}

std::vector<float> dock_ratio_ledger_export(unsigned int dockspace_id)
{
    std::vector<float> out;
    dock_collect_ratios(
        ImGui::DockBuilderGetNode(static_cast<ImGuiID>(dockspace_id)), out);
    return out;
}

void dock_ratio_ledger_import(const std::vector<float>& ratios)
{
    g_dock_pending = ratios;
}

void dock_ratio_guard_prepass(unsigned int dockspace_id, const ImVec2& size)
{
    ImGuiDockNode* root
        = ImGui::DockBuilderGetNode(static_cast<ImGuiID>(dockspace_id));
    if (root == nullptr)
        return;
    std::size_t k = 0;
    dock_seed_ledger(root, k);
    if (root->Size.x == size.x && root->Size.y == size.y)
        return; // host size stable: the post-pass ledger is in charge
    dock_prepass_apply(root, size);
}

void dock_splitter_dblclick_reset(unsigned int dockspace_id)
{
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    ImGuiDockNode* root
        = ImGui::DockBuilderGetNode(static_cast<ImGuiID>(dockspace_id));
    if (ctx == nullptr || root == nullptr)
        return;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    // Splitter interaction in flight? (SplitterBehavior holds ActiveId
    // and its window hosts a dock node.)
    const bool on_splitter = ctx->ActiveIdWindow != nullptr
        && ctx->ActiveIdWindow->DockNodeAsHost != nullptr;
    ImGuiDockNode* seam = on_splitter ? dock_seam_hit(root, mouse) : nullptr;
    if (on_splitter && seam != nullptr
        && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        dock_set_ratio_one(seam, 0.5f);
        ImGui::ClearActiveID();
        seam = nullptr; // enforce the fresh ledger this same frame
        ImGui::MarkIniSettingsDirty();
    }
    dock_walk_keep(root, seam);
}

}
