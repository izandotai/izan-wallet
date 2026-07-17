#pragma once

#include "ui/shell/chrome_state.hpp"
#include "ui/shell/win_chrome.hpp"

#include <imgui.h>

#include <functional>

struct GLFWwindow;

namespace izan::ui {

// The self-drawn window frame: main frame fill, title bar (window
// buttons, opacity slider, snap-layout popup), menu bar shell, status
// bar, layered soft popup shadows and a lightweight tooltip. Business
// text (title, subtitle, status, menu contents) comes from the caller.

void draw_menu_popup_shadows(const ChromeState& app);
void draw_main_window_frame(const ChromeState& app);
bool draw_window_control_button(
    const char* id, const ImVec2& size, WindowControlIcon icon);
void draw_simple_tooltip(const ChromeState& app, const char* id,
    const char* text, const ImVec2& anchor);
void queue_simple_tooltip(
    ChromeState& app, const char* text, const ImVec2& anchor);
void draw_snap_layout_popup(GLFWwindow* window, ChromeState& app);
void draw_custom_title_bar(GLFWwindow* window, ChromeState& app,
    const char* title_text, const char* subtitle_text);
void draw_custom_menu_bar(
    ChromeState& app, const std::function<void()>& draw_items);
void draw_status_bar(const ChromeState& app, const char* status_text);

// Double-clicking a dock splitter resets it to 50/50 (VS Code sash
// feel). Call once per frame right after DockSpace(). Also acts as the
// ratio guardian: ImGui's native dock resize lets the central node
// absorb all growth while other panes stay pixel-locked, so any outer
// resize or splitter drag would skew unrelated panes — this keeps a
// per-splitter ratio ledger, enforces it every frame (proportional
// adaptation), lets only the dragged splitter learn a new ratio, and
// rewrites the clicked splitter to 0.5 on double-click.
void dock_splitter_dblclick_reset(unsigned int dockspace_id);

// Call right BEFORE DockSpace() with the size the dockspace is about
// to get. The post-pass above corrects ratios one frame late — during
// a caption-drag restore the modal move loop renders exactly one frame
// at the new size and then nothing until release, so that one frame
// must already be right. When the host size is about to change, this
// rewrites the tree's SizeRef to the ledger ratios at the new size so
// the same frame lays out correctly. Splitter drags never change the
// host size, so this stays silent and cannot fight them.
void dock_ratio_guard_prepass(unsigned int dockspace_id, const ImVec2& size);

}
