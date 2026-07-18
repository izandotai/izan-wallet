#include "ui/widgets/label.hpp"

#include "ui/widgets/design.hpp"

namespace izan::ui {

float kit_title_size()
{
    return ImGui::GetFontSize() * design().title_scale;
}

float kit_heading_size()
{
    return ImGui::GetFontSize() * design().heading_scale;
}

float kit_caption_size()
{
    return ImGui::GetFontSize() * design().caption_scale;
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

}
