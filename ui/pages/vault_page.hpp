#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <string>

#include "core/secure/secure_bytes.hpp"
#include "keyd/client.hpp"
#include "ui/i18n/catalog.hpp"

struct GLFWwindow;

namespace izan::ui {

// The vault page: create / import / unlock / lock / back up.
//
// Red lines, inherited from the first vault UI and §3.1:
// 1. passphrase bytes leave the input buffer into guarded memory and
//    the buffer is wiped the moment a flow consumes it;
// 2. the IME is physically detached from the window while a secret
//    field has focus — composition strings get cached by the system;
// 3. everything slow (argon2, keyd round-trips) runs on a background
//    job the frame loop polls, never blocks on;
// 4. unlocking goes through keyd: this process never derives the vault
//    key, and backup re-presents the passphrase (Op::Reveal).
class VaultPage {
public:
    VaultPage(std::string vault_path, std::string exe_path);
    ~VaultPage();

    void draw(GLFWwindow* window, const i18n::Catalog& tr);

    bool unlocked() const
    {
        return m_unlocked;
    }

    // The live trust-plane handle, for pages that submit and approve
    // proposals; null until a keyd has been spawned. Ownership stays
    // here — pages borrow, never keep.
    keyd::KeydClient* keyd()
    {
        return m_keyd ? &*m_keyd : nullptr;
    }

private:
    enum class Mode {
        NoVault,
        CreateForm,
        ImportForm,
        ShowMnemonic, // after create or backup
        Locked,
        Unlocked,
    };

    struct Job {
        std::atomic<int> phase { 0 }; // 0 = running, 1 = ok, 2 = failed
        std::string error;            // written before phase goes to 2
        secure::SecureBytes secret;   // mnemonic sentence when revealing
        Mode next = Mode::Locked;     // mode to enter on success
    };

    void draw_no_vault(const i18n::Catalog& tr);
    void draw_create_form(const i18n::Catalog& tr);
    void draw_import_form(const i18n::Catalog& tr);
    void draw_show_mnemonic(const i18n::Catalog& tr);
    void draw_locked(const i18n::Catalog& tr);
    void draw_unlocked(const i18n::Catalog& tr);
    void poll_job();

    // Moves the buffer contents into guarded memory and wipes it.
    secure::SecureBytes take_secret(std::array<char, 256>& buf);
    void wipe_buffers();
    bool ensure_keyd(); // spawn on demand; false + m_status on failure

    std::string m_vault_path;
    std::string m_exe_path;
    Mode m_mode;
    bool m_unlocked = false;
    std::string m_address; // account #0, fetched at unlock
    std::string m_status;  // last message key or verbatim error
    bool m_status_is_key = false;

    std::array<char, 256> m_pass {};
    std::array<char, 256> m_confirm {};
    std::array<char, 1024> m_mnemonic_in {};
    secure::SecureBytes m_mnemonic_show;

    std::optional<keyd::KeydClient> m_keyd;
    std::shared_ptr<Job> m_job;
    bool m_ime_disabled = false;
};

}
