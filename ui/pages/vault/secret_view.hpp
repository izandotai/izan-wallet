#pragma once

#include "core/secure/secure_bytes.hpp"
#include "keyd/protocol.hpp"
#include "ui/i18n/catalog.hpp"

namespace izan::ui {

// The one surface allowed to show a root secret (a fresh wallet's
// phrase, a backup reveal) — a modal, so nothing else competes for the
// eyes while it is out. Owns the guarded bytes and wipes them the
// moment the person leaves, however they leave.
class SecretView {
public:
    void show(secure::SecureBytes secret, keyd::RevealKind kind);
    void reset();

    bool wants_open() const
    {
        return m_open_pending;
    }

    // Call every frame in the host window; opens itself when armed.
    void draw_dialog(const i18n::Catalog& tr);

private:
    secure::SecureBytes m_secret;
    keyd::RevealKind m_kind = keyd::RevealKind::SeedEntropy;
    bool m_open_pending = false;
};

}
