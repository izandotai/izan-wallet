#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/units/u256.hpp"
#include "domain/chains/chain_spec.hpp"

namespace izan::assets {

// One line of an address's ledger, as a Blockscout txlist reports it:
// native-coin movements only in v1 (token transfers ride a different
// endpoint and a later brick).
struct TxRecord {
    std::string hash;
    std::string counterparty; // the other side of the movement
    units::U256 value;
    uint64_t time = 0;        // unix seconds
    bool incoming = false;
    bool failed = false;
};

// Parses an etherscan-style txlist answer ({"status","result":[…]})
// into records, judged from `self`'s side of each row (case blind —
// explorers answer in lowercase). "No transactions found" is an empty
// vector, not an error; a malformed body throws.
std::vector<TxRecord> parse_txlist(
    std::string_view json, std::string_view self);

// One page of history (newest first) from the chain's configured
// Blockscout instance. A chain without a history base answers empty.
// Throws on transport or parse failure.
std::vector<TxRecord> fetch_history(
    const chains::ChainSpec& chain, const std::string& address);

}
