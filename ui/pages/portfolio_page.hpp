#pragma once

#include <array>
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "domain/assets/portfolio.hpp"
#include "ui/i18n/catalog.hpp"

namespace izan::ui {

// Watch-only portfolio: paste an address, see the assets. The Ship-0
// kernel in window form. Snapshots run on a background job — a slow
// RPC must never freeze the frame loop — and a chain that fails to
// answer stays visible as an unreadable row, never as a blank.
class PortfolioPage {
public:
    // Loads chains.json / tokens.json from data_dir. A malformed
    // config throws (a wallet must not limp along); a config that
    // merely differs from the shipped defaults loads fine but raises
    // the modified-config warning in the page (§ config trust).
    explicit PortfolioPage(const std::filesystem::path& data_dir);

    void draw(const i18n::Catalog& tr);

private:
    struct Row {
        std::string chain;
        std::string symbol;
        std::string amount; // formatted; empty when !ok
        std::string error;
        bool ok = false;
    };

    struct Job {
        std::atomic<int> phase { 0 }; // 0 running, 1 ok, 2 failed
        std::string error;
        std::vector<Row> rows;
    };

    std::shared_ptr<assets::PortfolioReader> m_reader;
    std::array<char, 128> m_address {};
    std::vector<Row> m_rows;
    std::shared_ptr<Job> m_job;
    std::string m_status;
    bool m_status_is_key = false;
    bool m_config_modified = false;
};

}
