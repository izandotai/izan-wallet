#pragma once

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace izan::ui {

// Everything about window size, placement and the internal dock
// layout lives here: the placement verbs the chrome triggers, the
// splitter-ratio guardian, and the persistence of it all. Drawing
// stays in chrome_widgets; the platform rect primitives stay in
// win_chrome. This component is the policy in between.

// ---- Placement verbs ----

void toggle_window_maximized(GLFWwindow* window);

// Places the window on a fraction of the current work area (snap
// layouts: halves, thirds, centered float). Leaves maximized state
// first so the result is a normal frame.
void snap_window_to_work_area(GLFWwindow* window, float x_fraction,
    float y_fraction, float width_fraction, float height_fraction);

// Centers by visible bounds (DWM frame compensation, same math as
// snapping). Call after the chrome is installed — before it, the
// not-yet-collapsed non-client area drags the window upward.
void center_window_on_work_area(GLFWwindow* window);

// ---- Dock splitter ratio guardian ----
// ImGui's native dock allocation lets the central node absorb every
// size delta while other panes stay pixel-locked, so an outer resize
// or a drag on one splitter skews unrelated panes. The ratio ledger is
// the source of truth: enforced every frame, only the dragged splitter
// learns, and a double-click rewrites that splitter to 0.5.

// Call right AFTER DockSpace(): holds the ledger against native
// allocation drift, learns from an in-flight splitter drag, resets the
// double-clicked seam.
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

// The ledger is session memory; these let the host persist it across
// runs. The wire form is positional — splitter ratios in depth-first
// order over the dockspace's split nodes — because nothing else is
// stable: node ids get renumbered between the live tree and the saved
// ini, and SizeRef is overwritten with already-mangled sizes by
// imgui's lock-size-once paths before the first prepass can read it.
// Imported ratios are staged and installed as each split materializes;
// values outside (0.02, 0.98) are ignored there.
std::vector<float> dock_ratio_ledger_export(unsigned int dockspace_id);
void dock_ratio_ledger_import(const std::vector<float>& ratios);

// ---- Persistence ----

// Everything the shell wants remembered between runs. The host owns
// storage — where these bytes live is its business — the shell owns
// their meaning: no imgui, dock or window-geometry knowledge leaks
// above this line.
struct LayoutState {
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

// Restores a saved LayoutState after GlfwApp::init and watches for
// changes each frame, calling back when something has settled and is
// worth writing. imgui's own ini file stays off throughout; this is
// the only layout persistence there is.
class LayoutKeeper {
public:
    // Call once, after GlfwApp::init and before the first frame:
    // applies the layout and staged splitter ratios, sizes the window
    // (centered, clamped to the work area) and re-maximizes.
    void restore(GLFWwindow* window, const LayoutState& state);

    // Call once per frame with the live dockspace id. Layout saves
    // follow imgui's own settings timer; geometry saves debounce until
    // the shape has held for a second.
    void update(GLFWwindow* window, unsigned int dockspace_id,
        const std::function<void(const LayoutState&)>& save);

    // Fresh capture for the exit save (geometry is already current
    // from the last update).
    const LayoutState& final_state();

private:
    LayoutState m_state;
    double m_geometry_dirty_since = -1.0;
    unsigned int m_dockspace = 0;
};

}
