#pragma once

#include <functional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace izan::ui {

// Everything the shell wants remembered between runs. The host owns
// storage — where these bytes live is its business — the shell owns
// their meaning: no imgui, dock or window-geometry knowledge leaks
// above this line.
struct UiState {
    // Restored (non-maximized) frame size; maximized rides on top.
    // Position is deliberately absent: monitors come and go, and a
    // centered start is the one that is always visible.
    int window_w = 0;
    int window_h = 0;
    bool window_maximized = false;
    // imgui's window/dock layout (SaveIniSettingsToMemory text), plus
    // the splitter ratios in depth-first split order — the ini's
    // SizeRef pixels are absolute and imgui mangles them when the
    // startup window differs from the saved one, so the ratios ride
    // separately and win.
    std::string layout;
    std::vector<float> dock_ratios;
};

// Restores a saved UiState after GlfwApp::init and watches for changes
// each frame, calling back when something has settled and is worth
// writing. imgui's own ini file stays off throughout; this is the only
// layout persistence there is.
class UiStateKeeper {
public:
    // Call once, after GlfwApp::init and before the first frame:
    // applies the layout and staged splitter ratios, sizes the window
    // (centered, clamped to the work area) and re-maximizes.
    void restore(GLFWwindow* window, const UiState& state);

    // Call once per frame with the live dockspace id. Layout saves
    // follow imgui's own settings timer; geometry saves debounce until
    // the shape has held for a second.
    void update(GLFWwindow* window, unsigned int dockspace_id,
        const std::function<void(const UiState&)>& save);

    // Fresh capture for the exit save (geometry is already current
    // from the last update).
    const UiState& final_state();

private:
    UiState m_state;
    double m_geometry_dirty_since = -1.0;
    unsigned int m_dockspace = 0;
};

}
