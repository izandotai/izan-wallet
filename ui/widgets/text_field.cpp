#include "ui/widgets/text_field.hpp"

#include <cstring>

#include <imgui.h>
#include <sodium.h>

namespace izan::ui {

bool kit_text_field(
    const char* id, const char* hint, char* buf, std::size_t size)
{
    return ImGui::InputTextWithHint(
        id, hint, buf, size, ImGuiInputTextFlags_EnterReturnsTrue);
}

bool secret_field(const char* label, std::array<char, 256>& buf,
    bool& secret_focus, const char* hint)
{
    constexpr ImGuiInputTextFlags kFlags = ImGuiInputTextFlags_Password
        | ImGuiInputTextFlags_AutoSelectAll
        | ImGuiInputTextFlags_EnterReturnsTrue;
    const bool submitted = hint
        ? ImGui::InputTextWithHint(label, hint, buf.data(), buf.size(), kFlags)
        : ImGui::InputText(label, buf.data(), buf.size(), kFlags);
    secret_focus |= ImGui::IsItemActive();
    return submitted;
}

bool kit_paste_box(
    const char* id, char* buf, std::size_t size, float rows, bool& secret_focus)
{
    const bool changed = ImGui::InputTextMultiline(
        id, buf, size, ImVec2(-1.0f, ImGui::GetTextLineHeight() * rows));
    secret_focus |= ImGui::IsItemActive();
    return changed;
}

secure::SecureBytes take_secret(std::array<char, 256>& buf)
{
    const std::size_t len = strnlen(buf.data(), buf.size());
    secure::SecureBytes out(len);
    if (len)
        std::memcpy(out.data(), buf.data(), len);
    sodium_memzero(buf.data(), buf.size());
    return out;
}

}
