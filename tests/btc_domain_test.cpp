// The Bitcoin read layer: address validation and the esplora address
// dialect, offline; one live round trip gated behind IZAN_LIVE_TESTS.

#include <doctest/doctest.h>

#include <cstdlib>

#include "domain/btc/esplora.hpp"

TEST_CASE("bitcoin addresses validate by decoding across all four forms")
{
    using izan::btc::valid_address;
    // The genesis coinbase address (P2PKH).
    CHECK(valid_address("1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa"));
    // P2SH, native segwit and taproot — the shipped preset quartet.
    CHECK(valid_address("3D9iyFHi1Zs9KoyynUfrL82rGhJfYTfSG4"));
    CHECK(valid_address("bc1qcr8te4kr609gcawutmrza0j4xv80jy8z306fyu"));
    CHECK(valid_address(
        "bc1p5cyxnuxmeuwuvkwfem96lqzszd02n6xdcjrs20cac6yqjjwudpxqkedrcr"));
    CHECK_FALSE(valid_address(""));
    // One flipped character breaks the checksum, loudly.
    CHECK_FALSE(valid_address("1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNb"));
    CHECK_FALSE(valid_address("bc1qcr8te4kr609gcawutmrza0j4xv80jy8z306fyv"));
    // Other families' addresses are refused, not guessed at.
    CHECK_FALSE(valid_address("0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045"));
    CHECK_FALSE(valid_address("EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v"));
}

TEST_CASE("the esplora address dialect parses and refuses the malformed")
{
    using izan::btc::parse_address_stats;
    CHECK(parse_address_stats(R"({"address":"x","chain_stats":
        {"funded_txo_sum":7500000000,"spent_txo_sum":500000000},
        "mempool_stats":{}})")
              .to_dec()
        == "7000000000");
    CHECK(parse_address_stats(
        R"({"chain_stats":{"funded_txo_sum":0,"spent_txo_sum":0}})")
            .is_zero());
    CHECK_THROWS(parse_address_stats("[]"));
    CHECK_THROWS(parse_address_stats(R"({"chain_stats":{}})"));
    // A ledger where more left than arrived is corrupt, not negative.
    CHECK_THROWS(parse_address_stats(R"({"chain_stats":
        {"funded_txo_sum":1,"spent_txo_sum":2}})"));
}

//   IZAN_LIVE_TESTS=1 build/izan_tests.exe -tc="*genesis*"
TEST_CASE("live: the genesis address still holds its tribute")
{
    if (!std::getenv("IZAN_LIVE_TESTS")) {
        MESSAGE("skipped (set IZAN_LIVE_TESTS=1 to run against mainnet)");
        return;
    }
    izan::chains::ChainSpec spec;
    spec.chain_id = 8332;
    spec.family = "btc";
    spec.rpc = { "https://mempool.space/api", "https://blockstream.info/api" };
    const auto sats
        = izan::btc::native_balance(spec, "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa");
    // 50 BTC coinbase plus two decades of tributes.
    CHECK(sats.to_dec().size() >= 10);
    MESSAGE("genesis sats " << sats.to_dec());
}
