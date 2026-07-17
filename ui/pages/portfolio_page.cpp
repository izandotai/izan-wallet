#include "ui/pages/portfolio_page.hpp"

#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>

#include <imgui.h>

#include "core/units/decimal.hpp"
#include "domain/config/config_trust.hpp"

namespace izan::ui {

namespace {

    std::string slurp(const std::filesystem::path& path)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            throw std::runtime_error("cannot read " + path.string());
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

}

PortfolioPage::PortfolioPage(const std::filesystem::path& data_dir)
{
    const std::string chainsJson = slurp(data_dir / "chains.json");
    const std::string tokensJson = slurp(data_dir / "tokens.json");
    m_config_modified = config::classify("chains.json", chainsJson)
            != config::Trust::ShippedDefault
        || config::classify("tokens.json", tokensJson)
            != config::Trust::ShippedDefault;

    m_reader = std::make_shared<assets::PortfolioReader>(
        chains::ChainRegistry::from_json(chainsJson),
        assets::TokenRegistry::from_json(tokensJson));
}

void PortfolioPage::draw(const i18n::Catalog& tr)
{
    if (m_job) {
        const int phase = m_job->phase.load();
        if (phase == 1) {
            m_rows = std::move(m_job->rows);
            m_status.clear();
            m_job.reset();
        } else if (phase == 2) {
            if (m_job->error.find("address") != std::string::npos) {
                m_status = "portfolio.err.address";
                m_status_is_key = true;
            } else {
                m_status = m_job->error;
                m_status_is_key = false;
            }
            m_job.reset();
        }
    }

    ImGui::Begin(
        (std::string(tr("portfolio.title")) + "###portfolio-page").c_str());

    if (m_config_modified) {
        ImGui::TextWrapped("%s", tr("portfolio.warn.config"));
        ImGui::Spacing();
    }

    const bool busy = m_job != nullptr;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 24.0f);
    ImGui::InputTextWithHint("##address", tr("portfolio.address"),
        m_address.data(), m_address.size());
    ImGui::SameLine();
    ImGui::BeginDisabled(busy);
    if (ImGui::Button(tr("portfolio.refresh")) && m_address[0] != '\0') {
        m_status.clear();
        auto job = std::make_shared<Job>();
        m_job = job;
        // The reader is single-driver: the refresh button stays
        // disabled until the worker reports back.
        auto reader = m_reader;
        const std::string address(
            m_address.data(), strnlen(m_address.data(), m_address.size()));
        std::thread([job, reader, address] {
            try {
                for (const auto& h : reader->snapshot(address)) {
                    Row row;
                    row.chain = h.chain;
                    row.symbol = h.symbol;
                    row.ok = h.ok;
                    if (h.ok)
                        row.amount = units::format_units(h.amount, h.decimals);
                    else
                        row.error = h.error;
                    job->rows.push_back(std::move(row));
                }
                job->phase.store(1);
            } catch (const std::exception& e) {
                job->error = e.what();
                job->phase.store(2);
            }
        }).detach();
    }
    ImGui::EndDisabled();
    if (busy) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", tr("portfolio.busy"));
    }

    if (!m_status.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped(
            "%s", m_status_is_key ? tr(m_status.c_str()) : m_status.c_str());
    }

    if (!m_rows.empty()
        && ImGui::BeginTable("##holdings", 3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
                | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn(tr("portfolio.col.chain"));
        ImGui::TableSetupColumn(tr("portfolio.col.asset"));
        ImGui::TableSetupColumn(tr("portfolio.col.balance"));
        ImGui::TableHeadersRow();
        for (const Row& row : m_rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.chain.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.symbol.c_str());
            ImGui::TableNextColumn();
            if (row.ok) {
                ImGui::TextUnformatted(row.amount.c_str());
            } else {
                ImGui::TextDisabled(
                    "%s — %s", tr("portfolio.unreadable"), row.error.c_str());
            }
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

}
