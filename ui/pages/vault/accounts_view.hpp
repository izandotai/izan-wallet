#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/secure/secure_bytes.hpp"
#include "ui/i18n/catalog.hpp"

namespace izan::ui {

// The unlocked wallet: scheme badge, the account table — selection
// mark, editable per-account note, middle-shortened address that
// copies on click — a derive button for HD wallets, lock and backup.
// Backup spends the passphrase again — the field here feeds it.
class AccountsView {
public:
    struct Event {
        enum class Type { None, Select, Add, Lock, Backup, LabelEdit };
        Type type = Type::None;
        const char* err = nullptr;
        uint32_t index = 0;       // Select / LabelEdit
        std::string label;        // LabelEdit
        secure::SecureBytes pass; // Backup
    };

    void reset();

    // Loads the note buffers from the sidecar's labels; the page calls
    // this whenever the account line changes hands (unlock, switch,
    // derive).
    void set_labels(std::span<const std::string> labels, std::size_t count);

    Event draw(const i18n::Catalog& tr, bool busy, bool& secret_focus,
        std::span<const std::string> addresses, uint32_t active, bool hd,
        uint8_t preset);

private:
    std::array<char, 256> m_pass {};
    std::vector<std::array<char, 48>> m_labels;
};

}
