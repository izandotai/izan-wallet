#pragma once

#include <array>
#include <string>

#include "core/secure/secure_bytes.hpp"
#include "ui/i18n/catalog.hpp"
#include "ui/wallet/store.hpp"

namespace izan::ui {

// The new-wallet flow as a dialog: name, passphrase, confirmation —
// small, focused, immune to however narrow the pane behind it is.
// Owns its buffers and their wiping; validation errors show inline.
class CreateView {
public:
    struct Event {
        enum class Type { None, Submit };
        Type type = Type::None;
        std::string name;
        secure::SecureBytes pass;
    };

    void reset();
    // Call every frame in the window that opened the dialog.
    Event draw_dialog(const i18n::Catalog& tr, bool busy, bool& secret_focus,
        const WalletStore& store);

private:
    std::array<char, 64> m_name {};
    std::array<char, 256> m_pass {};
    std::array<char, 256> m_confirm {};
    const char* m_err = nullptr; // i18n key shown inside the dialog
    bool m_focus_pending = true;
};

}
