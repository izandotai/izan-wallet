#include <doctest/doctest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "core/units/decimal.hpp"
#include "core/units/u256.hpp"
#include "domain/chains/chain_spec.hpp"
#include "domain/chains/jsonrpc.hpp"
#include "platform/net/http_client.hpp"

using izan::net::parse_https_url;
using izan::units::U256;

TEST_CASE("https url parsing")
{
    auto plain = parse_https_url("https://ethereum-rpc.publicnode.com");
    CHECK(plain.host == "ethereum-rpc.publicnode.com");
    CHECK(plain.port == "443");
    CHECK(plain.target == "/");

    auto pathed = parse_https_url("https://arb1.arbitrum.io/rpc");
    CHECK(pathed.host == "arb1.arbitrum.io");
    CHECK(pathed.target == "/rpc");

    auto ported = parse_https_url("https://node.example:8545/a/b?c=1");
    CHECK(ported.host == "node.example");
    CHECK(ported.port == "8545");
    CHECK(ported.target == "/a/b?c=1");

    CHECK_THROWS_AS(
        parse_https_url("http://insecure.example"), std::invalid_argument);
    CHECK_THROWS_AS(parse_https_url("wss://ws.example"), std::invalid_argument);
    CHECK_THROWS_AS(parse_https_url("https:///nohost"), std::invalid_argument);
    CHECK_THROWS_AS(parse_https_url("https://host:"), std::invalid_argument);
}

// End-to-end against real mainnet RPC: registry → envelope → TLS →
// parse → U256. Needs the network, so it only runs when asked:
//   IZAN_LIVE_TESTS=1 build/izan_tests.exe
TEST_CASE("live: mainnet block number and burned-address balance")
{
    if (!std::getenv("IZAN_LIVE_TESTS")) {
        MESSAGE("skipped (set IZAN_LIVE_TESTS=1 to run against mainnet)");
        return;
    }

    std::ifstream f(IZAN_SOURCE_DIR "/data/chains.json");
    REQUIRE(f);
    std::stringstream ss;
    ss << f.rdbuf();
    auto reg = izan::chains::ChainRegistry::from_json(ss.str());
    const auto* eth = reg.by_id(1);
    REQUIRE(eth);

    std::string failures;
    for (const std::string& endpoint : eth->rpc) {
        try {
            auto url = parse_https_url(endpoint);
            izan::net::HttpsClient client(url.host, url.port);

            auto blockResp = client.post(url.target,
                izan::chains::make_request("eth_blockNumber", "[]", 1));
            REQUIRE(blockResp.status == 200);
            U256 block = U256::from_hex(
                izan::chains::result_string(blockResp.body, 1));
            // Mainnet passed twenty million blocks long ago.
            CHECK(block > U256::from_u64(20'000'000));

            auto balResp = client.post(url.target,
                izan::chains::make_request("eth_getBalance",
                    R"(["0x0000000000000000000000000000000000000000",)"
                    R"("latest"])",
                    2));
            REQUIRE(balResp.status == 200);
            U256 burned
                = U256::from_hex(izan::chains::result_string(balResp.body, 2));
            // The zero address holds thousands of burned ETH.
            CHECK(burned > izan::units::parse_units("1000", 18));
            return; // one live endpoint is proof enough
        } catch (const std::exception& e) {
            failures += endpoint + ": " + e.what() + "\n";
        }
    }
    FAIL("no rpc endpoint reachable:\n" << failures);
}
