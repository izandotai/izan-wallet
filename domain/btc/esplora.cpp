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
