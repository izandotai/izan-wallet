#include "ui/pages/vault/list_view.hpp"

#include <cstring>

#include <imgui.h>
#include <sodium.h>

#include "ui/wallet/presets.hpp"

namespace izan::ui {

namespace {

    const char* kind_badge(const i18n::Catalog& tr, const std::string& kind)
    {
        const char* key = kind_badge_key(kind);
        return *key ? tr(key) : "";
    }

}

WalletListView::Event WalletListView::draw(const i18n::Catalog& tr, bool busy,
    const WalletStore& store, const std::string& active_id,
    bool active_unlocked)
{
    Event ev;
    ImGui::BeginDisabled(busy);

    for (const WalletEntry& w : store.wallets()) {
        ImGui::PushID(w.id.c_str());
        const bool is_active = w.id == active_id;

        std::string subtitle = kind_badge(tr, w.kind);
        if (w.count > 1) {
            if (!subtitle.empty())
                subtitle += " · ";
            subtitle += std::to_string(w.count);
        }
        const char* dot = is_active && active_unlocked ? "●" : "○";
        std::string label = std::string(dot) + " " + w.name;
        if (!subtitle.empty())
            label += "\n   " + subtitle;

        if (ImGui::Selectable(label.c_str(), is_active) && !is_active) {
            ev.type = Event::Type::Activate;
            ev.id = w.id;
        }
        if (ImGui::BeginPopupContextItem("##card-menu")) {
            if (ImGui::MenuItem(tr("wallet.activate")) && !is_active) {
                ev.type = Event::Type::Activate;
                ev.id = w.id;
            }
            if (ImGui::MenuItem(tr("wallet.rename"))) {
                m_target = w.id;
                sodium_memzero(m_rename.data(), m_rename.size());
                std::memcpy(m_rename.data(), w.name.data(),
                    std::min(w.name.size(), m_rename.size() - 1));
                m_open_rename = true;
            }
            if (ImGui::MenuItem(tr("wallet.delete"))) {
                m_target = w.id;
                sodium_memzero(m_confirm.data(), m_confirm.size());
                m_open_delete = true;
            }
            ImGui::EndPopup();
        }
        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button(tr("vault.create")))
        ev.type = Event::Type::Create;
    ImGui::SameLine();
    if (ImGui::Button(tr("vault.import")))
        ev.type = Event::Type::Import;

    // Modals are opened outside the card loop so their ids are stable.
    if (m_open_rename) {
        ImGui::OpenPopup("##rename-wallet");
        m_open_rename = false;
    }
    if (m_open_delete) {
        ImGui::OpenPopup("##delete-wallet");
        m_open_delete = false;
    }

    if (ImGui::BeginPopupModal(
            "##rename-wallet", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText(tr("wallet.name"), m_rename.data(), m_rename.size());
        if (ImGui::Button(tr("wallet.rename"))) {
            ev.type = Event::Type::Rename;
            ev.id = m_target;
            ev.name = std::string(
                m_rename.data(), strnlen(m_rename.data(), m_rename.size()));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr("ui.cancel")))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(
            "##delete-wallet", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string target_name = m_target;
        for (const WalletEntry& w : store.wallets())
            if (w.id == m_target)
                target_name = w.name;
        ImGui::TextWrapped("%s", tr("wallet.delete.warn"));
        ImGui::Spacing();
        ImGui::TextUnformatted(target_name.c_str());
        ImGui::TextDisabled("%s", tr("wallet.delete.confirm"));
        ImGui::InputText("##confirm", m_confirm.data(), m_confirm.size());
        const std::string typed(
            m_confirm.data(), strnlen(m_confirm.data(), m_confirm.size()));
        ImGui::BeginDisabled(typed != target_name);
        if (ImGui::Button(tr("wallet.delete"))) {
            ev.type = Event::Type::Delete;
            ev.id = m_target;
            sodium_memzero(m_confirm.data(), m_confirm.size());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(tr("ui.cancel")))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::EndDisabled();
    return ev;
}

}
