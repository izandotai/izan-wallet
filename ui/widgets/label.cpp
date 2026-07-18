#include "ui/widgets/label.hpp"

#include <cstring>
#include <vector>

#include "ui/widgets/design.hpp"

namespace izan::ui {

float kit_title_size()
{
    return kit_snap(ImGui::GetFontSize() * design().title_scale);
}

float kit_heading_size()
{
    return kit_snap(ImGui::GetFontSize() * design().heading_scale);
}

float kit_caption_size()
{
    return kit_snap(ImGui::GetFontSize() * design().caption_scale);
}

void kit_title(const char* text)
{
    ImGui::PushFont(nullptr, kit_title_size());
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
}

void kit_heading(const char* text)
{
    ImGui::PushFont(nullptr, kit_heading_size());
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
}

void kit_caption(const char* text)
{
    ImGui::PushFont(nullptr, kit_caption_size());
    ImGui::TextDisabled("%s", text);
    ImGui::PopFont();
}

void kit_vspace(float em)
{
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * em));
}

namespace {

    float measure_at(const char* s, float font_size)
    {
        if (font_size > 0.0f)
            return ImGui::GetFont()
                ->CalcTextSizeA(font_size, FLT_MAX, 0.0f, s)
                .x;
        return ImGui::CalcTextSize(s).x;
    }

}

std::string kit_elide_middle(const char* text, float budget, float font_size)
{
    const auto measure
        = [&](const char* s) { return measure_at(s, font_size); };
    if (measure(text) <= budget)
        return text;
    const std::size_t len = std::strlen(text);
    const std::size_t tail = len < 6 ? len : 6;
    for (std::size_t head = len; head > 4; --head) {
        std::string out(text, head);
        out += "…";
        out.append(text + len - tail, tail);
        if (measure(out.c_str()) <= budget)
            return out;
    }
    // Even the minimal head-and-tail pair overflows: degrade to a
    // shrinking head, and to a bare ellipsis at the bitter end — the
    // component never draws wider than its slot.
    for (std::size_t head = 4; head > 0; --head) {
        std::string out(text, head);
        out += "…";
        if (measure(out.c_str()) <= budget)
            return out;
    }
    return "…";
}

std::string kit_elide_end(const char* text, float budget, float font_size)
{
    if (measure_at(text, font_size) <= budget)
        return text;
    // Codepoint start offsets, so the cut never lands inside a
    // multibyte character.
    std::vector<std::size_t> starts;
    for (const char* c = text; *c; ++c)
        if ((*c & 0xC0) != 0x80)
            starts.push_back(std::size_t(c - text));
    for (std::size_t n = starts.size(); n > 1; --n) {
        std::string out(text, starts[n - 1]);
        out += "…";
        if (measure_at(out.c_str(), font_size) <= budget)
            return out;
    }
    return "…";
}

}
