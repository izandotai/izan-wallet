#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "domain/chains/chain_spec.hpp"

namespace izan::assets {

// One ERC-20 the wallet watches. Tokens are config, not code, same as
// chains.
struct TokenSpec {
    uint64_t chain_id {};
    std::string address; // stored checksummed
    std::string symbol;
    uint8_t decimals {};
};

class TokenRegistry {
public:
    // Parses a JSON array and validates it as a whole: addresses must
    // checksum-validate (and are normalized to EIP-55 form), symbols
    // present, decimals within U256 range, no duplicate
    // (chain, address) pairs. Throws on any violation.
    static TokenRegistry from_json(std::string_view json);

    std::vector<const TokenSpec*> tokens_for(uint64_t chain_id) const;

    // Folds another registry in — the user's own token file over the
    // shipped one. Entries already known (same chain and address) and
    // entries naming a chain the registry of chains has never heard of
    // are skipped: a typo in a user file must not take the page down.
    void extend(
        const TokenRegistry& extra, const chains::ChainRegistry& chains);

    const std::vector<TokenSpec>& all() const
    {
        return m_tokens;
    }

private:
    std::vector<TokenSpec> m_tokens;
};

}
