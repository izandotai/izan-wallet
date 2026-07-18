#include "ui/widgets/tooltip.hpp"

#include <imgui.h>

#include "ui/widgets/label.hpp"

namespace izan::ui {

void kit_tooltip(const char* text)
{
    kit_tooltip_lines(text, nullptr);
}

void kit_tooltip_lines(const char* text, const char* hint)
{
    if (!ImGui::BeginTooltip())
        return;
    ImGui::TextUnformatted(text);
    if (hint && *hint)
        kit_caption(hint);
    ImGui::EndTooltip();
}

}
