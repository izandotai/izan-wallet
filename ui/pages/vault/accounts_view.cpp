#include "ui/pages/vault/accounts_view.hpp"

#include <cstring>

#include <imgui.h>
#include <sodium.h>

#include "ui/wallet/presets.hpp"
#include "ui/widgets/secret_field.hpp"

namespace izan::ui {

namespace {

    // 0x1234567…abcdef — enough of each end to recognize, hover for
    // the whole thing.
    std::string shortened(const std::string& addr)
    {
        if (addr.size() <= 20)
            return addr;
        return addr.substr(0, 10) + "…" + addr.substr(addr.size() - 6);
    }

}

void AccountsView::reset()
{
    sodium_memzero(m_pass.data(), m_pass.size());
    m_labels.clear();
}

void AccountsView::set_labels(
    std::span<const std::string> labels, std::size_t count)
{
    m_labels.assign(count, {});
    for (std::size_t i = 0; i < count && i < labels.size(); ++i)
        std::memcpy(m_labels[i].data(), labels[i].data(),
            std::min(labels[i].size(), m_labels[i].size() - 1));
}

AccountsView::Event AccountsView::draw(const i18n::Catalog& tr, bool busy,
    bool& secret_focus, std::span<const std::string> addresses, uint32_t active,
    bool hd, uint8_t preset)
{
    Event ev;
    ImGui::TextUnformatted(tr("vault.state.unlocked"));

    // The scheme badge: always meaningful for an HD wallet; for a key
    // wallet only when the preset names an address format (a vendor
    // path label would be noise on a wallet that derives nothing).
    if (hd
        || keyd::preset_family(keyd::DerivePreset(preset))
            != keyd::ChainFamily::Eth) {
        ImGui::TextDisabled("%s", tr("wallet.preset"));
        ImGui::SameLine();
        ImGui::TextUnformatted(preset_name(keyd::DerivePreset(preset)));
    }

    ImGui::TextDisabled("%s", tr("vault.address"));
    if (m_labels.size() < addresses.size())
        m_labels.resize(addresses.size());
    for (uint32_t i = 0; i < addresses.size(); ++i) {
        ImGui::PushID(int(i));
        const bool selected = i == active;
        if (ImGui::RadioButton("##acct", selected) && !selected) {
            ev.type = Event::Type::Select;
            ev.index = i;
        }
        ImGui::SameLine();
        ImGui::Text("#%u", i);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
        ImGui::InputTextWithHint("##note", tr("wallet.note"),
            m_labels[i].data(), m_labels[i].size());
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ev.type = Event::Type::LabelEdit;
            ev.index = i;
            ev.label = std::string(m_labels[i].data(),
                strnlen(m_labels[i].data(), m_labels[i].size()));
        }
        ImGui::SameLine();
        const std::string& full = addresses[std::size_t(i)];
        ImGui::TextUnformatted(shortened(full).c_str());
        if (ImGui::IsItemClicked())
            ImGui::SetClipboardText(full.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%s", full.c_str(), tr("ui.copy"));
        ImGui::PopID();
    }
    if (hd && ImGui::Button(tr("wallet.account.add")))
        ev.type = Event::Type::Add;
    ImGui::Spacing();

    ImGui::BeginDisabled(busy);
    if (ImGui::Button(tr("vault.lock")))
        ev.type = Event::Type::Lock;
    ImGui::SameLine();
    secret_field(tr("vault.passphrase"), m_pass, secret_focus);
    if (ImGui::Button(tr("vault.backup"))) {
        if (strnlen(m_pass.data(), m_pass.size()) == 0) {
            ev.err = "vault.msg.empty_pass";
        } else {
            ev.type = Event::Type::Backup;
            ev.pass = take_secret(m_pass);
        }
    }
    ImGui::EndDisabled();
    if (busy)
        ImGui::TextDisabled("%s", tr("vault.busy"));
    return ev;
}

}
