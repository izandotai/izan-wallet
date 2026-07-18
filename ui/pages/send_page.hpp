#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "domain/chains/chain_spec.hpp"
#include "domain/tx/eip1559.hpp"
#include "domain/tx/txflow.hpp"
#include "ui/i18n/catalog.hpp"
#include "ui/pages/vault_page.hpp"

struct GLFWwindow;

namespace izan::ui {

// The send page: form → quote → proposal → human approval (which IS
// the signature) → broadcast → receipt. Everything slow runs on a
// background job the frame loop polls; the passphrase follows the
// vault page's red lines (guarded memory, wiped buffers, IME detached
// while a secret field has focus). The trust-plane handle is borrowed
// from the vault page — keyd only ever signs the queue's own bytes,
// so nothing on this page can move money without the human's
// passphrase at the approval step.
class SendPage {
public:
    SendPage(const std::filesystem::path& data_dir, VaultPage& vault);
    ~SendPage();

    void draw(GLFWwindow* window, const i18n::Catalog& tr);

private:
    enum class Stage {
        Form,
        Quoting,
        Review,
        Approving,  // proposal pending, passphrase field up
        Delivering, // approve → sign → broadcast → receipt, one job
        Done,
        Failed,
    };

    struct Job {
        std::atomic<int> phase { 0 }; // 0 running, 1 ok, 2 failed
        std::atomic<int> step { 0 };  // Delivering: 1 signed, 2 broadcast
        std::string error;
        // quote results
        uint64_t nonce = 0;
        uint64_t gas = 0;
        tx::FeeQuote fees;
        // delivery results (hash written before step goes to 2)
        std::string tx_hash;
        uint64_t block = 0;
        bool tx_success = false;
    };

    void draw_form(const i18n::Catalog& tr);
    void draw_review(const i18n::Catalog& tr);
    void draw_approving(const i18n::Catalog& tr);
    void draw_delivering(const i18n::Catalog& tr);
    void draw_done(const i18n::Catalog& tr);
    void poll_job();
    void reset_to_form();
    const chains::ChainSpec& selected_chain() const;

    chains::ChainRegistry m_registry;
    VaultPage& m_vault;

    Stage m_stage = Stage::Form;
    int m_chain_index = 0;
    std::array<char, 64> m_to {};
    std::array<char, 32> m_amount {};
    std::array<char, 256> m_pass {};
    bool m_ime_disabled = false;

    // The reviewed draft; immutable once the proposal is submitted —
    // keyd signs the queue's copy of exactly these bytes.
    tx::Eip1559Tx m_tx;
    std::string m_wallet_seen; // last active wallet id; a switch resets
    std::string m_from;
    uint32_t m_account = 0;    // captured at review, rides the envelope
    uint8_t m_preset = 0;      // derivation preset, captured with the account
    std::string m_to_checked;
    uint64_t m_proposal = 0;

    std::string m_status; // key or verbatim error
    bool m_status_is_key = false;
    std::shared_ptr<Job> m_job;
};

}
