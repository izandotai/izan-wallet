#include "ui/pages/vault/secret_view.hpp"

#include <imgui.h>

#include "ui/widgets/kit.hpp"

namespace izan::ui {

void SecretView::show(secure::SecureBytes secret, keyd::RevealKind kind)
{
    m_secret = std::move(secret);
    m_kind = kind;
    m_open_pending = true;
}

void SecretView::reset()
{
    m_secret.reset();
    m_open_pending = false;
}

void SecretView::draw_dialog(const i18n::Catalog& tr)
{
    if (m_open_pending) {
        kit_dialog_open("##show-secret");
        m_open_pending = false;
    }
    bool dismissed = false;
    if (!kit_dialog_begin("##show-secret", &dismissed))
        return;
    const bool seed = m_kind == keyd::RevealKind::SeedEntropy;
    kit_dialog_header_icon("🔑",
        seed ? tr("vault.mnemonic") : tr("vault.detect.key"),
        tr("vault.msg.created"));
    if (!m_secret.empty()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextWrapped(
            "%s", reinterpret_cast<const char*>(m_secret.data()));
        ImGui::PopTextWrapPos();
    }
    kit_vspace(0.3f);
    ImGui::PushFont(nullptr, kit_caption_size());
    ImGui::TextColored(kit_danger(), "%s",
        seed ? tr("vault.warn.backup") : tr("vault.warn.backup.key"));
    ImGui::PopFont();
    kit_vspace(0.3f);
    if (dismissed)
        reset();
    if (kit_subtle_button(tr("ui.back"))) {
        reset();
        kit_dialog_close();
    }
    kit_dialog_end();
}

}
