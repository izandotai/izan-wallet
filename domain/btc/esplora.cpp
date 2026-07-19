#include "domain/btc/esplora.hpp"

#include <stdexcept>
#include <string>

#include <glaze/glaze.hpp>

// Address validation lives in the crypto layer (core/crypto/btc.cpp):
// trezor's headers and OpenSSL's cannot share a TU — the SHA256_CTX
// name collision again — and this file needs the HTTPS client.
#include "core/crypto/btc.hpp"
#include "platform/net/http_client.hpp"

namespace izan::btc {

bool valid_address(std::string_view text)
{
    return crypto::valid_btc_address(text);
}

units::U256 parse_address_stats(std::string_view json)
{
    glz::json_t doc;
    if (glz::read_json(doc, json) || !doc.is_object())
        throw std::runtime_error("btc: address answer not an object");
    const auto& obj = doc.get_object();
    const auto it = obj.find("chain_stats");
    if (it == obj.end() || !it->second.is_object())
        throw std::runtime_error("btc: address answer missing chain_stats");
    const auto& stats = it->second.get_object();
    auto number = [&](const char* name) -> double {
        const auto f = stats.find(name);
        if (f == stats.end() || !f->second.is_number())
            throw std::runtime_error(
                std::string("btc: chain_stats missing ") + name);
        return f->second.get_number();
    };
    const double funded = number("funded_txo_sum");
    const double spent = number("spent_txo_sum");
    if (spent > funded)
        throw std::runtime_error("btc: spent exceeds funded");
    // Satoshi totals sit far inside a double's exact-integer range —
    // 21 million BTC is 2.1e15 of 2^53.
    return units::U256::from_u64(uint64_t(funded - spent));
}

units::U256 native_balance(
    const chains::ChainSpec& spec, std::string_view address)
{
    if (!valid_address(address))
        throw std::invalid_argument(
            "not a bitcoin address: " + std::string(address));
    std::string last_error = "no esplora endpoint configured";
    for (const std::string& base : spec.rpc) {
        try {
            const net::HttpsUrl url = net::parse_https_url(base);
            net::HttpsClient client(url.host, url.port);
            const std::string base_path
                = url.target == "/" ? std::string() : url.target;
            const net::HttpResponse res
                = client.get(base_path + "/address/" + std::string(address),
                    { { "Accept", "application/json" } });
            if (res.status != 200)
                throw std::runtime_error(
                    "esplora answered " + std::to_string(res.status));
            return parse_address_stats(res.body);
        } catch (const std::exception& e) {
            last_error = e.what(); // next endpoint gets its turn
        }
    }
    throw std::runtime_error("btc: " + last_error);
}

}

namespace izan::btc {

std::vector<BtcTx> parse_txs(std::string_view json, std::string_view self)
{
    glz::json_t doc;
    if (glz::read_json(doc, json) || !doc.is_array())
        throw std::runtime_error("btc: txs answer not an array");
    const std::string me(self);
    std::vector<BtcTx> out;
    for (const glz::json_t& tx : doc.get_array()) {
        if (!tx.is_object())
            continue;
        const auto& obj = tx.get_object();
        auto text = [&](const char* name) -> std::string {
            const auto it = obj.find(name);
            return it != obj.end() && it->second.is_string()
                ? it->second.get_string()
                : std::string();
        };
        BtcTx rec;
        rec.txid = text("txid");
        if (rec.txid.empty())
            continue;
        const auto status = obj.find("status");
        if (status == obj.end() || !status->second.is_object())
            continue;
        const auto& st = status->second.get_object();
        const auto confirmed = st.find("confirmed");
        if (confirmed == st.end() || !confirmed->second.is_boolean()
            || !confirmed->second.get_boolean())
            continue; // mempool money is not money yet
        const auto when = st.find("block_time");
        if (when == st.end() || !when->second.is_number())
            continue;
        rec.time = uint64_t(when->second.get_number());

        // Walk both sides: what left the address, what arrived, and
        // the first foreign face on either side.
        double sent = 0, received = 0;
        std::string foreign_in, foreign_out;
        const auto vin = obj.find("vin");
        if (vin != obj.end() && vin->second.is_array())
            for (const glz::json_t& in : vin->second.get_array()) {
                if (!in.is_object())
                    continue;
                const auto prev = in.get_object().find("prevout");
                if (prev == in.get_object().end() || !prev->second.is_object())
                    continue; // coinbase has no prevout
                const auto& po = prev->second.get_object();
                const auto addr = po.find("scriptpubkey_address");
                const auto val = po.find("value");
                if (addr == po.end() || !addr->second.is_string()
                    || val == po.end() || !val->second.is_number())
                    continue;
                if (addr->second.get_string() == me)
                    sent += val->second.get_number();
                else if (foreign_in.empty())
                    foreign_in = addr->second.get_string();
            }
        const auto vout = obj.find("vout");
        if (vout != obj.end() && vout->second.is_array())
            for (const glz::json_t& o : vout->second.get_array()) {
                if (!o.is_object())
                    continue;
                const auto& oo = o.get_object();
                const auto addr = oo.find("scriptpubkey_address");
                const auto val = oo.find("value");
                if (addr == oo.end() || !addr->second.is_string()
                    || val == oo.end() || !val->second.is_number())
                    continue;
                if (addr->second.get_string() == me)
                    received += val->second.get_number();
                else if (foreign_out.empty())
                    foreign_out = addr->second.get_string();
            }
        const double net = received - sent;
        if (net == 0)
            continue; // self-shuffle; nothing this ledger can say
        rec.incoming = net > 0;
        rec.amount = units::U256::from_u64(uint64_t(net > 0 ? net : -net));
        rec.counterparty = rec.incoming ? foreign_in : foreign_out;
        out.push_back(std::move(rec));
    }
    return out;
}

std::vector<BtcTx> fetch_txs(
    const chains::ChainSpec& spec, std::string_view address)
{
    if (!valid_address(address))
        throw std::invalid_argument(
            "not a bitcoin address: " + std::string(address));
    std::string last_error = "no esplora endpoint configured";
    for (const std::string& base : spec.rpc) {
        try {
            const net::HttpsUrl url = net::parse_https_url(base);
            net::HttpsClient client(url.host, url.port);
            const std::string base_path
                = url.target == "/" ? std::string() : url.target;
            const net::HttpResponse res = client.get(
                base_path + "/address/" + std::string(address) + "/txs",
                { { "Accept", "application/json" } });
            if (res.status != 200)
                throw std::runtime_error(
                    "esplora answered " + std::to_string(res.status));
            return parse_txs(res.body, address);
        } catch (const std::exception& e) {
            last_error = e.what();
        }
    }
    throw std::runtime_error("btc: " + last_error);
}

}

namespace izan::btc {

namespace {

    // One esplora round trip over the endpoint list, first answer wins.
    template <typename Fn>
    auto esplora_walk(const chains::ChainSpec& spec, Fn&& fn)
    {
        std::string last_error = "no esplora endpoint configured";
        for (const std::string& base : spec.rpc) {
            try {
                const net::HttpsUrl url = net::parse_https_url(base);
                net::HttpsClient client(url.host, url.port);
                const std::string base_path
                    = url.target == "/" ? std::string() : url.target;
                return fn(client, base_path);
            } catch (const std::exception& e) {
                last_error = e.what();
            }
        }
        throw std::runtime_error("btc: " + last_error);
    }

}

FeeTiers parse_fee_estimates(std::string_view json)
{
    glz::json_t doc;
    if (glz::read_json(doc, json) || !doc.is_object())
        throw std::runtime_error("btc: fee estimates not an object");
    const auto& obj = doc.get_object();
    auto tier = [&](const char* blocks) -> uint64_t {
        const auto it = obj.find(blocks);
        if (it == obj.end() || !it->second.is_number()
            || it->second.get_number() <= 0)
            return 0;
        const double v = it->second.get_number();
        return uint64_t(v) + (v > double(uint64_t(v)) ? 1 : 0);
    };
    FeeTiers t;
    t.fast = tier("1");
    if (!t.fast)
        t.fast = tier("2");
    t.normal = tier("6");
    t.slow = tier("144");
    if (!t.fast)
        t.fast = 1;
    if (!t.normal)
        t.normal = t.fast;
    if (!t.slow)
        t.slow = t.normal;
    // The market never quotes a slower tier above a faster one; a
    // clamp keeps a sparse answer honest.
    t.normal = std::min(t.normal, t.fast);
    t.slow = std::min(t.slow, t.normal);
    return t;
}

FeeTiers fee_tiers(const chains::ChainSpec& spec)
{
    return esplora_walk(spec, [](net::HttpsClient& c, const std::string& p) {
        const net::HttpResponse res
            = c.get(p + "/fee-estimates", { { "Accept", "application/json" } });
        if (res.status != 200)
            throw std::runtime_error(
                "esplora answered " + std::to_string(res.status));
        return parse_fee_estimates(res.body);
    });
}

std::string broadcast_tx(const chains::ChainSpec& spec,
    std::span<const uint8_t> tx, std::string_view expected_txid)
{
    if (tx.empty())
        throw std::invalid_argument("btc: empty transaction");
    static constexpr char kHex[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(tx.size() * 2);
    for (const uint8_t b : tx) {
        hex.push_back(kHex[b >> 4]);
        hex.push_back(kHex[b & 0xf]);
    }
    return esplora_walk(
        spec, [&](net::HttpsClient& c, const std::string& p) -> std::string {
            const net::HttpResponse res
                = c.post(p + "/tx", hex, {}, "text/plain");
            if (res.status != 200)
                throw std::runtime_error(
                    "esplora refused the tx: " + res.body.substr(0, 200));
            std::string txid = res.body;
            while (
                !txid.empty() && (txid.back() == '\n' || txid.back() == '\r'))
                txid.pop_back();
            // The node's echo must match our own arithmetic — a
            // mismatch means somebody is confused about what was sent.
            if (txid != expected_txid)
                throw std::runtime_error("btc: node echoed txid " + txid);
            return txid;
        });
}

bool tx_confirmed(const chains::ChainSpec& spec, std::string_view txid)
{
    return esplora_walk(spec, [&](net::HttpsClient& c, const std::string& p) {
        const net::HttpResponse res
            = c.get(p + "/tx/" + std::string(txid) + "/status",
                { { "Accept", "application/json" } });
        if (res.status != 200)
            throw std::runtime_error(
                "esplora answered " + std::to_string(res.status));
        glz::json_t doc;
        if (glz::read_json(doc, res.body) || !doc.is_object())
            throw std::runtime_error("btc: status answer not an object");
        const auto& obj = doc.get_object();
        const auto it = obj.find("confirmed");
        return it != obj.end() && it->second.is_boolean()
            && it->second.get_boolean();
    });
}

}
