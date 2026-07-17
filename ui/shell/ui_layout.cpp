#include "ui/shell/ui_layout.hpp"

#include "ui/shell/fonts.hpp"
#include "ui/shell/win_chrome.hpp"

#include <imgui_internal.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace izan::ui {

void toggle_window_maximized(GLFWwindow* window)
{
    if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE)
        glfwRestoreWindow(window);
    else
        glfwMaximizeWindow(window);
}

void snap_window_to_work_area(GLFWwindow* window, float x_fraction,
    float y_fraction, float width_fraction, float height_fraction)
{
    const WorkArea area = current_window_work_area(window);
    if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE)
        glfwRestoreWindow(window);

    const int x = area.x
        + static_cast<int>(
            std::lround(static_cast<float>(area.width) * x_fraction));
    const int y = area.y
        + static_cast<int>(
            std::lround(static_cast<float>(area.height) * y_fraction));
    const int right = area.x
        + static_cast<int>(std::lround(
            static_cast<float>(area.width) * (x_fraction + width_fraction)));
    const int bottom = area.y
        + static_cast<int>(std::lround(
            static_cast<float>(area.height) * (y_fraction + height_fraction)));
    const int width = std::max(320, right - x);
    const int height = std::max(240, bottom - y);
    set_window_visible_bounds(window, x, y, width, height);
}

void center_window_on_work_area(GLFWwindow* window)
{
    const WorkArea area = current_window_work_area(window);
    int w = 0, h = 0;
    glfwGetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0 || area.width <= 0 || area.height <= 0)
        return;
    set_window_visible_bounds(window,
        area.x + std::max(0, (area.width - w) / 2),
        area.y + std::max(0, (area.height - h) / 2), w, h);
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

namespace {

    // One-time adoption of a layout left by the retired beside-the-exe
    // ini scheme; the file is consumed, not left to shadow the real
    // store.
    void adopt_legacy_ini(LayoutState& state)
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

void LayoutKeeper::restore(GLFWwindow* window, const LayoutState& state)
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

void LayoutKeeper::update(GLFWwindow* window, unsigned int dockspace_id,
    const std::function<void(const LayoutState&)>& save)
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

const LayoutState& LayoutKeeper::final_state()
{
    m_state.layout = ImGui::SaveIniSettingsToMemory();
    m_state.dock_ratios = dock_ratio_ledger_export(m_dockspace);
    return m_state;
}

}
