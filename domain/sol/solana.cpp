#include "domain/sol/solana.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

#include <glaze/glaze.hpp>
#include <sodium.h>

#include "core/crypto/sol.hpp"

extern "C" {
#include <base58.h>
}

namespace izan::sol {

bool valid_address(std::string_view text)
{
    return crypto::valid_sol_address(text);
}

units::U256 parse_balance_result(std::string_view result_json)
{
    glz::json_t doc;
    if (glz::read_json(doc, result_json) || !doc.is_object())
        throw std::runtime_error("sol: getBalance result not an object");
    const auto& obj = doc.get_object();
    const auto it = obj.find("value");
    if (it == obj.end() || !it->second.is_number())
        throw std::runtime_error("sol: getBalance result missing value");
    const double v = it->second.get_number();
    if (v < 0)
        throw std::runtime_error("sol: negative balance");
    // Lamport totals fit a double exactly up to 2^53 — about nine
    // billion SOL, versus a supply near six hundred million.
    return units::U256::from_u64(uint64_t(v));
}

units::U256 native_balance(chains::RpcClient& rpc, std::string_view address)
{
    if (!valid_address(address))
        throw std::invalid_argument(
            "not a solana address: " + std::string(address));
    return parse_balance_result(
        rpc.call("getBalance", "[\"" + std::string(address) + "\"]"));
}

std::vector<SolSig> parse_signatures(std::string_view result_json)
{
    glz::json_t doc;
    if (glz::read_json(doc, result_json) || !doc.is_array())
        throw std::runtime_error(
            "sol: getSignaturesForAddress result not an array");
    std::vector<SolSig> out;
    for (const glz::json_t& entry : doc.get_array()) {
        if (!entry.is_object())
            continue;
        const auto& obj = entry.get_object();
        const auto sig = obj.find("signature");
        if (sig == obj.end() || !sig->second.is_string())
            continue;
        SolSig rec;
        rec.signature = sig->second.get_string();
        const auto when = obj.find("blockTime");
        if (when != obj.end() && when->second.is_number()
            && when->second.get_number() > 0)
            rec.time = uint64_t(when->second.get_number());
        const auto err = obj.find("err");
        rec.failed = err != obj.end() && !err->second.is_null();
        out.push_back(std::move(rec));
    }
    return out;
}

std::vector<SolSig> recent_signatures(
    chains::RpcClient& rpc, std::string_view address)
{
    if (!valid_address(address))
        throw std::invalid_argument(
            "not a solana address: " + std::string(address));
    return parse_signatures(rpc.call("getSignaturesForAddress",
        "[\"" + std::string(address) + "\",{\"limit\":25}]"));
}

}

namespace izan::sol {

std::array<uint8_t, 32> parse_blockhash_result(std::string_view result_json)
{
    glz::json_t doc;
    if (glz::read_json(doc, result_json) || !doc.is_object())
        throw std::runtime_error("sol: getLatestBlockhash not an object");
    const auto& obj = doc.get_object();
    const auto value = obj.find("value");
    if (value == obj.end() || !value->second.is_object())
        throw std::runtime_error("sol: blockhash answer missing value");
    const auto& v = value->second.get_object();
    const auto hash = v.find("blockhash");
    if (hash == v.end() || !hash->second.is_string())
        throw std::runtime_error("sol: blockhash answer missing blockhash");
    std::array<uint8_t, 32> out {};
    std::size_t sz = out.size();
    if (!b58tobin(out.data(), &sz, hash->second.get_string().c_str())
        || sz != out.size())
        throw std::runtime_error("sol: blockhash not 32 bytes of base58");
    return out;
}

std::array<uint8_t, 32> latest_blockhash(chains::RpcClient& rpc)
{
    return parse_blockhash_result(
        rpc.call("getLatestBlockhash", "[{\"commitment\":\"finalized\"}]"));
}

std::string send_transaction(
    chains::RpcClient& rpc, std::span<const uint8_t> tx)
{
    if (tx.empty())
        throw std::invalid_argument("sol: empty transaction");
    std::string b64(
        sodium_base64_ENCODED_LEN(tx.size(), sodium_base64_VARIANT_ORIGINAL),
        '\0');
    sodium_bin2base64(b64.data(), b64.size(), tx.data(), tx.size(),
        sodium_base64_VARIANT_ORIGINAL);
    b64.resize(std::strlen(b64.c_str()));
    const std::string answer = rpc.call(
        "sendTransaction", "[\"" + b64 + "\",{\"encoding\":\"base64\"}]");
    glz::json_t doc;
    if (glz::read_json(doc, answer) || !doc.is_string())
        throw std::runtime_error("sol: sendTransaction answered no signature");
    return doc.get_string();
}

SigStatus parse_signature_status(std::string_view result_json)
{
    glz::json_t doc;
    if (glz::read_json(doc, result_json) || !doc.is_object())
        throw std::runtime_error("sol: getSignatureStatuses not an object");
    const auto& obj = doc.get_object();
    const auto value = obj.find("value");
    if (value == obj.end() || !value->second.is_array()
        || value->second.get_array().empty())
        throw std::runtime_error("sol: status answer missing value");
    const glz::json_t& entry = value->second.get_array().front();
    if (entry.is_null())
        return SigStatus::Unknown;
    if (!entry.is_object())
        throw std::runtime_error("sol: status entry malformed");
    const auto& st = entry.get_object();
    const auto err = st.find("err");
    if (err != st.end() && !err->second.is_null())
        return SigStatus::Failed;
    const auto level = st.find("confirmationStatus");
    if (level != st.end() && level->second.is_string()) {
        const std::string& s = level->second.get_string();
        if (s == "finalized")
            return SigStatus::Finalized;
        if (s == "confirmed")
            return SigStatus::Confirmed;
    }
    return SigStatus::Processed;
}

SigStatus signature_status(chains::RpcClient& rpc, std::string_view signature)
{
    return parse_signature_status(rpc.call(
        "getSignatureStatuses", "[[\"" + std::string(signature) + "\"]]"));
}

uint64_t rent_exempt_minimum(chains::RpcClient& rpc)
{
    const std::string answer
        = rpc.call("getMinimumBalanceForRentExemption", "[0]");
    glz::json_t doc;
    if (glz::read_json(doc, answer) || !doc.is_number() || doc.get_number() < 0)
        throw std::runtime_error("sol: rent minimum unreadable");
    return uint64_t(doc.get_number());
}

}
