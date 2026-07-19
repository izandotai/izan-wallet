#include <doctest/doctest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "core/codec/abi.hpp"
#include "core/units/decimal.hpp"
#include "domain/assets/balances.hpp"
#include "domain/assets/token_registry.hpp"
#include "domain/chains/rpc_client.hpp"

using izan::chains::ChainRegistry;
using izan::chains::ChainSpec;
using izan::chains::RpcClient;
using izan::units::U256;

namespace {

const char* kZero = "0x0000000000000000000000000000000000000000";
const char* kVitalik = "0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045";
const char* kUsdc = "0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48";

ChainRegistry shipped_registry()
{
    std::ifstream f(IZAN_SOURCE_DIR "/data/chains.json");
    REQUIRE(f);
    std::stringstream ss;
    ss << f.rdbuf();
    return ChainRegistry::from_json(ss.str());
}

}

TEST_CASE("bad addresses never reach the network")
{
    // The endpoint is unreachable on purpose; validation must throw
    // before any connection is attempted.
    ChainSpec spec { .chain_id = 1,
        .name = "T",
        .symbol = "T",
        .rpc = { "https://127.0.0.1:1" } };
    RpcClient rpc(std::move(spec));

    CHECK_THROWS_AS(
        izan::assets::native_balance(rpc, "0x1234"), std::invalid_argument);
    CHECK_THROWS_AS(izan::assets::native_balance(rpc, "vitalik.eth"),
        std::invalid_argument);
    CHECK_THROWS_AS(izan::assets::erc20_balance(rpc, kUsdc, "0xnothex"),
        std::invalid_argument);
    CHECK_THROWS_AS(izan::assets::erc20_balance(rpc, "not-a-token", kZero),
        std::invalid_argument);
}

TEST_CASE("tls-only transport records http endpoints as failures")
{
    ChainSpec spec { .chain_id = 31337,
        .name = "Dev",
        .symbol = "ETH",
        .rpc = { "http://127.0.0.1:8545" } };
    RpcClient rpc(std::move(spec));
    try {
        rpc.call_string("eth_blockNumber", "[]");
        FAIL("expected failure");
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("all endpoints failed")
            != std::string::npos);
    }
}

// End-to-end balance reads against public mainnets. Opt-in:
//   IZAN_LIVE_TESTS=1 build/izan_tests.exe
TEST_CASE("live: mainnet balances through the full stack")
{
    if (!std::getenv("IZAN_LIVE_TESTS")) {
        MESSAGE("skipped (set IZAN_LIVE_TESTS=1 to run against mainnet)");
        return;
    }

    ChainRegistry reg = shipped_registry();
    const ChainSpec* eth = reg.by_id(1);
    REQUIRE(eth);
    RpcClient rpc(*eth);

    // The zero address holds thousands of burned ETH, forever.
    U256 burned = izan::assets::native_balance(rpc, kZero);
    CHECK(burned > izan::units::parse_units("1000", 18));

    // USDC total supply is far beyond ten million dollars; this walks
    // codec → eth_call → decode against a live contract.
    const std::string supplyData
        = izan::codec::CallData("totalSupply()").to_hex();
    U256 supply = izan::codec::decode_u256(rpc.call_string("eth_call",
        std::string("[{\"to\":\"") + kUsdc + "\",\"data\":\"" + supplyData
            + "\"},\"latest\"]"));
    CHECK(supply > izan::units::parse_units("10000000", 6));

    // balanceOf path proof: whatever vitalik.eth holds, it decodes.
    izan::assets::erc20_balance(rpc, kUsdc, kVitalik);
}

TEST_CASE("live: robinhood chain answers on its shipped endpoint")
{
    if (!std::getenv("IZAN_LIVE_TESTS")) {
        MESSAGE("skipped (set IZAN_LIVE_TESTS=1 to run against mainnet)");
        return;
    }

    ChainRegistry reg = shipped_registry();
    const ChainSpec* rh = reg.by_id(4663);
    REQUIRE(rh);
    RpcClient rpc(*rh);
    U256 block = U256::from_hex(rpc.call_string("eth_blockNumber", "[]"));
    CHECK(!block.is_zero());
}

TEST_CASE("user token file folds in, minus nonsense")
{
    const auto chains = izan::chains::ChainRegistry::from_json(R"([
        {"chain_id":1,"name":"Ethereum","symbol":"ETH","decimals":18,
         "rpc":["https://example.invalid"]}
    ])");
    auto tokens = izan::assets::TokenRegistry::from_json(R"([
        {"chain_id":1,"symbol":"USDC","decimals":6,
         "address":"0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48"}
    ])");
    const auto user = izan::assets::TokenRegistry::from_json(R"([
        {"chain_id":1,"symbol":"MEME","decimals":18,
         "address":"0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045"},
        {"chain_id":1,"symbol":"USDC2","decimals":6,
         "address":"0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48"},
        {"chain_id":999,"symbol":"GHOST","decimals":18,
         "address":"0x70997970C51812dc3A010C7d01b50e0d17dc79C8"}
    ])");
    tokens.extend(user, chains);
    // The new token joins; the duplicate address and the token on an
    // unknown chain are both turned away.
    REQUIRE(tokens.all().size() == 2);
    CHECK(tokens.all()[1].symbol == "MEME");
}

//   IZAN_LIVE_TESTS=1 build/izan_tests.exe -tc="*probe*"
TEST_CASE("live: a token describes itself to the add form")
{
    if (!std::getenv("IZAN_LIVE_TESTS")) {
        MESSAGE("skipped (set IZAN_LIVE_TESTS=1 to run against mainnet)");
        return;
    }

    ChainRegistry reg = shipped_registry();
    const ChainSpec* eth = reg.by_id(1);
    REQUIRE(eth);
    RpcClient rpc(*eth);

    // Canonical dynamic string: mainnet USDC.
    const auto usdc = izan::assets::probe_token(
        rpc, "0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48");
    CHECK(usdc.symbol == "USDC");
    CHECK(usdc.decimals == 6);

    // Pre-standard bytes32 symbol: MKR still speaks it.
    const auto mkr = izan::assets::probe_token(
        rpc, "0x9f8F72aA9304c8B593d555F12eF6589cC3A579A2");
    CHECK(mkr.symbol == "MKR");
    CHECK(mkr.decimals == 18);

    // An EOA is not a token; the probe must refuse, not invent.
    CHECK_THROWS(izan::assets::probe_token(
        rpc, "0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045"));
}
