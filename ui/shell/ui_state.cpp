#include "ui/shell/ui_state.hpp"

#include "ui/shell/chrome_widgets.hpp"
#include "ui/shell/fonts.hpp"
#include "ui/shell/win_chrome.hpp"

#include <imgui.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace izan::ui {

namespace {

    // One-time adoption of a layout left by the retired beside-the-exe
    // ini scheme; the file is consumed, not left to shadow the real
    // store.
    void adopt_legacy_ini(UiState& state)
    {
        const auto path = executable_dir() / "izan.imgui.ini";
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return;
        std::ostringstream buf;
        buf << f.rdbuf();
        state.layout = buf.str();
        f.close();
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

}

void UiStateKeeper::restore(GLFWwindow* window, const UiState& state)
{
    m_state = state;
    if (m_state.layout.empty())
        adopt_legacy_ini(m_state);
    if (!m_state.layout.empty())
        ImGui::LoadIniSettingsFromMemory(
            m_state.layout.data(), m_state.layout.size());
    dock_ratio_ledger_import(m_state.dock_ratios);

    // Size through the raw-rect call: glfw's decoration math no longer
    // matches the borderless chrome and would grow the frame a border's
    // width per launch.
    if (m_state.window_w > 0 && m_state.window_h > 0) {
        const WorkArea wa = current_window_work_area(window);
        const int w = std::min(m_state.window_w, wa.width);
        const int h = std::min(m_state.window_h, wa.height);
        set_window_screen_rect(window, wa.x + (wa.width - w) / 2,
            wa.y + (wa.height - h) / 2, w, h);
    }
    if (m_state.window_maximized)
        glfwMaximizeWindow(window);
}

void UiStateKeeper::update(GLFWwindow* window, unsigned int dockspace_id,
    const std::function<void(const UiState&)>& save)
{
    m_dockspace = dockspace_id;
    bool want_save = false;

    // imgui raises this (throttled by its settings timer) whenever the
    // layout changed; with no ini file of its own, persisting is ours.
    if (ImGui::GetIO().WantSaveIniSettings) {
        ImGui::GetIO().WantSaveIniSettings = false;
        m_state.layout = ImGui::SaveIniSettingsToMemory();
        m_state.dock_ratios = dock_ratio_ledger_export(dockspace_id);
        want_save = true;
    }

    // Geometry debounces: a border drag fires every frame, the store
    // hears about it once the shape holds for a second (the exit save
    // catches whatever is still pending).
    const bool maxed = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) != 0;
    bool changed = maxed != m_state.window_maximized;
    m_state.window_maximized = maxed;
    if (!maxed) {
        int w = 0, h = 0;
        glfwGetWindowSize(window, &w, &h);
        if (w > 0 && h > 0
            && (w != m_state.window_w || h != m_state.window_h)) {
            m_state.window_w = w;
            m_state.window_h = h;
            changed = true;
        }
    }
    if (changed)
        m_geometry_dirty_since = glfwGetTime();
    if (m_geometry_dirty_since >= 0.0
        && glfwGetTime() - m_geometry_dirty_since > 1.0) {
        m_geometry_dirty_since = -1.0;
        want_save = true;
    }

    if (want_save)
        save(m_state);
}

const UiState& UiStateKeeper::final_state()
{
    m_state.layout = ImGui::SaveIniSettingsToMemory();
    m_state.dock_ratios = dock_ratio_ledger_export(m_dockspace);
    return m_state;
}

}
