#include "ui/shell/fonts.hpp"

#include <imgui_freetype.h>

#include <array>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace izan::ui {

std::filesystem::path executable_dir()
{
#ifdef _WIN32
    std::array<char, MAX_PATH> path {};
    const DWORD length = GetModuleFileNameA(
        nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length == path.size())
        return std::filesystem::current_path();
    return std::filesystem::path(path.data()).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

void load_default_font(ImGuiIO& io)
{
    static const ImWchar emoji_ranges[] = {
        0x00A9,
        0x00AE,
        0x200D,
        0x200D,
        0x203C,
        0x3299,
        0xFE0E,
        0xFE0F,
        0x1F000,
        0x1FAFF,
        0,
    };
    // Yield blocks: the primary face emits no glyph in these ranges so
    // the emoji font (second priority) answers first. Whatever the
    // emoji font lacks (enclosed digits, geometric shapes, stars and
    // other monochrome symbols) falls through to the third-priority
    // fallback merge of the primary face below — the arrangement that
    // finally killed the tofu boxes.
    static const ImWchar main_font_emoji_exclude_ranges[] = {
        0x00A9,
        0x00AE,
        0x2194,
        0x21AA,
        0x231A,
        0x23FF,
        0x2460,
        0x24FF,
        0x25A0,
        0x25FF,
        0x2600,
        0x27BF,
        0x2934,
        0x2935,
        0x2B00,
        0x2BFF,
        0x3030,
        0x303D,
        0x3297,
        0x3299,
        0,
    };

    const std::array<std::filesystem::path, 3> candidates = {
        executable_dir() / kDefaultFontRelativePath,
        std::filesystem::current_path() / kDefaultFontRelativePath,
        std::filesystem::current_path() / ".." / ".."
            / kDefaultFontRelativePath,
    };

    for (const auto& path : candidates) {
        if (!std::filesystem::exists(path))
            continue;

#ifdef _WIN32
        AddFontResourceExA(path.string().c_str(), FR_PRIVATE, nullptr);
#endif

        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 2;
        config.PixelSnapH = false;
        config.GlyphExcludeRanges = main_font_emoji_exclude_ranges;

        ImFont* font = io.Fonts->AddFontFromFileTTF(path.string().c_str(),
            kDefaultFontSize, &config, io.Fonts->GetGlyphRangesChineseFull());

        if (font == nullptr)
            continue;
        io.FontDefault = font;
        if (std::filesystem::exists(kEmojiFontPath)) {
            ImFontConfig emoji_config;
            emoji_config.MergeMode = true;
            emoji_config.PixelSnapH = false;
            emoji_config.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LoadColor;
            io.Fonts->AddFontFromFileTTF(
                kEmojiFontPath, kDefaultFontSize, &emoji_config, emoji_ranges);
        }
        // Third priority: the primary face again without the exclude
        // table, catching every symbol the emoji font has no glyph
        // for — colored where possible, monochrome otherwise, never a
        // tofu box.
        ImFontConfig fallback_config;
        fallback_config.MergeMode = true;
        fallback_config.OversampleH = 2;
        fallback_config.OversampleV = 2;
        fallback_config.PixelSnapH = false;
        io.Fonts->AddFontFromFileTTF(path.string().c_str(), kDefaultFontSize,
            &fallback_config, io.Fonts->GetGlyphRangesChineseFull());
#ifdef _WIN32
        // Fourth priority: Segoe UI Symbol picks up the obscure
        // dingbats and technical symbols even the primary face lacks.
        const char* kSymbolFont = "C:\\Windows\\Fonts\\seguisym.ttf";
        if (std::filesystem::exists(kSymbolFont)) {
            ImFontConfig symbol_config;
            symbol_config.MergeMode = true;
            symbol_config.PixelSnapH = false;
            io.Fonts->AddFontFromFileTTF(
                kSymbolFont, kDefaultFontSize, &symbol_config, nullptr);
        }
#endif
        return;
    }

    io.Fonts->AddFontDefault();
    std::fprintf(
        stderr, "Failed to load UI font: %s\n", kDefaultFontRelativePath);
}

}
