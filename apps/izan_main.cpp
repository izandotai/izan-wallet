// Izan Wallet, single binary, two personalities: launched plain it is
// the UI; launched with --keyd-child it is the trust plane, and the
// GUI machinery below is never touched (see keyd/child.hpp).
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>

#include <imgui.h>

#include <GLFW/glfw3.h>

#include "keyd/child.hpp"
#include "ui/i18n/catalog.hpp"
#include "ui/pages/portfolio_page.hpp"
#include "ui/pages/vault_page.hpp"
#include "ui/shell/app.hpp"
#include "ui/shell/chrome_state.hpp"
#include "ui/shell/chrome_widgets.hpp"
#include "ui/shell/constants.hpp"
#include "ui/shell/fonts.hpp"
#include "ui/shell/ime.hpp"
#include "ui/shell/theme.hpp"

namespace {

using namespace izan;

// Runtime preferences, kept beside the exe. Losing this file loses a
// theme choice, nothing more — it holds no secrets and no money facts.
struct Settings {
    std::string language = "en";
    int theme_index = 0;
    float window_opacity = 0.96f;
};

std::filesystem::path settings_path()
{
    return ui::executable_dir() / "izan.settings.json";
}

Settings load_settings()
{
    Settings s;
    std::ifstream f(settings_path(), std::ios::binary);
    if (!f)
        return s;
    std::ostringstream buf;
    buf << f.rdbuf();
    Settings parsed;
    if (!glz::read<glz::opts { .error_on_unknown_keys = false }>(
            parsed, buf.str()))
        s = parsed;
    return s;
}

void save_settings(const Settings& s)
{
    std::string out;
    if (glz::write<glz::opts { .prettify = true }>(s, out))
        return;
    std::ofstream f(settings_path(), std::ios::binary | std::ios::trunc);
    f << out;
}

std::string default_vault_path()
{
    if (const char* appdata = std::getenv("APPDATA")) {
        const auto dir = std::filesystem::path(appdata) / "izan";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (!ec)
            return (dir / "izan.qvlt").string();
    }
    return (ui::executable_dir() / "izan.qvlt").string();
}

std::string self_exe_path()
{
    return (ui::executable_dir() / std::filesystem::path("izan.exe")).string();
}

}

int main(int argc, char** argv)
{
    if (argc > 1 && std::string_view(argv[1]) == "--keyd-child")
        return keyd::child_main(argc, argv);

    Settings settings = load_settings();

    const auto langDir = ui::executable_dir() / "assets" / "lang";
    // Preload code→native-name pairs for the language menu.
    std::vector<std::pair<std::string, std::string>> languages;
    for (const std::string& code : i18n::Catalog::available(langDir)) {
        try {
            languages.emplace_back(
                code, i18n::Catalog::load(langDir, code)("lang.name"));
        } catch (const std::exception&) {
            // an unreadable catalog simply stays out of the menu
        }
    }
    i18n::Catalog tr = [&] {
        try {
            return i18n::Catalog::load(langDir, settings.language);
        } catch (const std::exception&) {
            return i18n::Catalog::load(langDir, i18n::kBaseLanguage);
        }
    }();

    ui::GlfwApp app;
    ui::AppOptions options;
    options.title = "izan";
    options.width = 1600;
    options.height = 900;
    if (!app.init(options))
        return 1;

    ui::ChromeState chrome;
    chrome.theme_index = settings.theme_index;
    chrome.window_opacity = settings.window_opacity;
    ui::apply_theme_style_only(chrome.theme_index);
    glfwSetWindowOpacity(app.window(), chrome.window_opacity);

    ui::VaultPage vault(default_vault_path(), self_exe_path());

    // A broken chain/token config takes down the portfolio pane, not
    // the wallet: the vault stays reachable and the error is shown.
    std::optional<ui::PortfolioPage> portfolio;
    std::string portfolioError;
    try {
        portfolio.emplace(ui::executable_dir() / "data");
    } catch (const std::exception& e) {
        portfolioError = e.what();
    }

    app.set_render_callback([&] {
        app.begin_frame();

        ui::draw_main_window_frame(chrome);
        ui::draw_custom_title_bar(app.window(), chrome, tr("app.title"),
            vault.unlocked() ? tr("vault.state.unlocked")
                             : tr("vault.state.locked"));
        // Theme application is deferred to after the menu bar returns:
        // draw_custom_menu_bar wraps the items in Push/PopStyleColor
        // pairs, and popping would write the old theme's colors right
        // back over a style applied mid-callback.
        int pending_theme = -1;
        ui::draw_custom_menu_bar(chrome, [&] {
            if (ImGui::BeginMenu(tr("menu.view"))) {
                if (ImGui::BeginMenu(tr("menu.theme"))) {
                    for (int i = 0; i < int(ui::kThemeNames.size()); ++i) {
                        if (ImGui::MenuItem(ui::kThemeNames[i], nullptr,
                                chrome.theme_index == i))
                            pending_theme = i;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(tr("menu.language"))) {
                    for (const auto& [code, name] : languages) {
                        if (ImGui::MenuItem(
                                name.c_str(), nullptr, tr.language() == code)) {
                            try {
                                tr = i18n::Catalog::load(langDir, code);
                                settings.language = code;
                                save_settings(settings);
                            } catch (const std::exception&) {
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
        });
        if (pending_theme >= 0) {
            chrome.theme_index = pending_theme;
            ui::apply_theme_style_only(pending_theme);
            settings.theme_index = pending_theme;
            save_settings(settings);
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float top
            = ui::kWindowFrameMargin + ui::kTitleBarHeight + ui::kMenuBarHeight;
        ImGui::SetNextWindowPos(ImVec2(
            viewport->Pos.x + ui::kWindowFrameMargin, viewport->Pos.y + top));
        ImGui::SetNextWindowSize(
            ImVec2(viewport->Size.x - ui::kWindowFrameMargin * 2.0f,
                viewport->Size.y - top - ui::kWindowFrameMargin
                    - ui::kStatusBarHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        ImGui::Begin("##dockhost", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoDocking
                | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoSavedSettings);
        const ImGuiID dockspace = ImGui::GetID("izan-dockspace");
        ImGui::DockSpace(
            dockspace, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
        ui::dock_splitter_dblclick_reset(dockspace);
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        vault.draw(app.window(), tr);
        if (portfolio) {
            portfolio->draw(tr);
        } else {
            ImGui::Begin(
                (std::string(tr("portfolio.title")) + "###portfolio-page")
                    .c_str());
            ImGui::TextWrapped("%s", portfolioError.c_str());
            ImGui::End();
        }

        ui::draw_status_bar(chrome, tr("status.ready"));
        ui::draw_snap_layout_popup(app.window(), chrome);
        ui::draw_menu_popup_shadows(chrome);
        if (chrome.pending_tooltip_visible) {
            ui::draw_simple_tooltip(chrome, "##tooltip",
                chrome.pending_tooltip_text.c_str(),
                chrome.pending_tooltip_anchor);
            chrome.pending_tooltip_visible = false;
        }
        ui::update_ime_position(app.window(), nullptr);

        if (chrome.window_opacity != settings.window_opacity) {
            settings.window_opacity = chrome.window_opacity;
            save_settings(settings);
        }
        if (chrome.request_exit)
            glfwSetWindowShouldClose(app.window(), GLFW_TRUE);

        app.end_frame(ui::theme_clear_color(chrome));
    });
    app.run();
    return 0;
}
