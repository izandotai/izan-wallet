#pragma once

#include <imgui.h>

#include <filesystem>

struct GLFWwindow;

namespace izan::ui {

// Borderless self-drawn window frame on Win32: WS_THICKFRAME keeps
// system drag/snap alive, DWM paints the dark caption, and a custom
// WM_NCHITTEST maps the drawn title bar to caption/resize behavior.
// Everything is a no-op off Windows.

enum class WindowControlIcon {
    Minimize,
    Maximize,
    Restore,
    Close,
};

struct WorkArea {
    int x = 0;
    int y = 0;
    int width = 1280;
    int height = 720;
};

void toggle_window_maximized(GLFWwindow* window);

void install_custom_window_chrome(GLFWwindow* window);
void update_title_bar_hit_regions(GLFWwindow* window, const ImVec2& title_min,
    const ImVec2& title_max, const ImVec2& menu_min, const ImVec2& menu_max,
    const ImVec2& controls_min, const ImVec2& controls_max);
void update_title_bar_button_hit_region(GLFWwindow* window,
    WindowControlIcon icon, const ImVec2& min, const ImVec2& max);

WorkArea current_window_work_area(GLFWwindow* window);
void center_window_on_primary_work_area(GLFWwindow* window);
// Centers by visible bounds (DWM frame compensation, same math as
// snapping). Call after the chrome is installed — before it, the
// not-yet-collapsed non-client area drags the window upward.
void center_window_on_work_area(GLFWwindow* window);
void snap_window_to_work_area(GLFWwindow* window, float x_fraction,
    float y_fraction, float width_fraction, float height_fraction);

void glfw_error_callback(int error, const char* description);

// Custom mouse cursors (win_cursors.cpp): loads a .cur/.ani set from a
// directory and takes over cursor shape via WM_SETCURSOR plus a
// per-frame fallback. Missing directory or missing Arrow returns false
// and the system cursors stay; the registry is never touched.
bool install_custom_cursors(const std::filesystem::path& dir);
bool custom_cursors_active();
void apply_custom_cursor();                      // by ImGui::GetMouseCursor()
void apply_custom_cursor_slot(int imgui_cursor); // explicit slot (frame)

}
