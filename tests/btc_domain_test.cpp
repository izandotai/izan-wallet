// The Bitcoin read layer: address validation and the esplora address
// dialect, offline; one live round trip gated behind IZAN_LIVE_TESTS.

#include <doctest/doctest.h>

#include <cstdlib>

#include "domain/btc/btc_tx.hpp"
#include "domain/btc/coin_select.hpp"
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

TEST_CASE("an esplora tx page reads as our side of the ledger")
{
    // One incoming payment, one outgoing with change, one unconfirmed
    // straggler, one self-shuffle — only the first two are stories.
    const char* self = "1BvBMSEYstWetqTFn5Au4m4GFg7xJaNVN2";
    const std::string json = std::string(R"([
      {"txid":"aa11","status":{"confirmed":true,"block_time":1700000100},
       "vin":[{"prevout":{"scriptpubkey_address":"1SenderFace","value":5000}}],
       "vout":[{"scriptpubkey_address":")")
        + self + R"(","value":3000},
               {"scriptpubkey_address":"1SenderFace","value":1900}]},
      {"txid":"bb22","status":{"confirmed":true,"block_time":1700000200},
       "vin":[{"prevout":{"scriptpubkey_address":")"
        + self + R"(","value":10000}}],
       "vout":[{"scriptpubkey_address":"3DestFace","value":7000},
               {"scriptpubkey_address":")"
        + self + R"(","value":2500}]},
      {"txid":"cc33","status":{"confirmed":false},
       "vin":[],"vout":[{"scriptpubkey_address":")"
        + self + R"(","value":1}]},
      {"txid":"dd44","status":{"confirmed":true,"block_time":1700000300},
       "vin":[{"prevout":{"scriptpubkey_address":")"
        + self + R"(","value":800}}],
       "vout":[{"scriptpubkey_address":")"
        + self + R"(","value":800}]}
    ])";
    const auto txs = izan::btc::parse_txs(json, self);
    REQUIRE(txs.size() == 2);
    CHECK(txs[0].txid == "aa11");
    CHECK(txs[0].incoming);
    CHECK(txs[0].amount.to_dec() == "3000");
    CHECK(txs[0].counterparty == "1SenderFace");
    CHECK(txs[0].time == 1700000100);
    CHECK(txs[1].txid == "bb22");
    CHECK(!txs[1].incoming);
    // 10000 out, 2500 came home: the world got 7500, fee included.
    CHECK(txs[1].amount.to_dec() == "7500");
    CHECK(txs[1].counterparty == "3DestFace");

    CHECK_THROWS(izan::btc::parse_txs("{}", self));
}

TEST_CASE("the coin selector is deterministic and honest about dust")
{
    using izan::btc::Utxo;
    const std::vector<Utxo> purse = {
        { "aa", 0, 50000 },
        { "bb", 1, 30000 },
        { "cc", 0, 10000 },
        { "dd", 2, 546 },
    };
    // One big coin covers it: 1 input, 2 outputs, fee = 141 * 2.
    auto s = izan::btc::select_coins(purse, 20000, 2);
    REQUIRE(s.inputs.size() == 1);
    CHECK(s.inputs[0].txid == "aa");
    CHECK(s.fee == izan::btc::p2wpkh_vsize(1, 2) * 2);
    CHECK(s.change == 50000 - 20000 - s.fee);

    // Needs two coins; greedy takes the biggest first.
    auto s2 = izan::btc::select_coins(purse, 70000, 1);
    REQUIRE(s2.inputs.size() == 2);
    CHECK(s2.inputs[1].txid == "bb");
    CHECK(s2.change == 80000 - 70000 - s2.fee);

    // Change below dust folds into the fee and the outputs slim to one.
    const std::vector<Utxo> tight = { { "ee", 0, 21000 } };
    auto s3 = izan::btc::select_coins(tight, 20500, 1);
    CHECK(s3.change == 0);
    CHECK(s3.fee == 500); // everything left over, honestly labelled

    // Cannot afford: amount + fee over the whole purse.
    CHECK_THROWS(izan::btc::select_coins(purse, 90000, 2));
    CHECK_THROWS(izan::btc::select_coins(purse, 0, 2));

    // The sweep: everything in, one output, no change.
    auto sw = izan::btc::sweep_coins(purse, 2);
    CHECK(sw.inputs.size() == 4);
    CHECK(sw.fee == izan::btc::p2wpkh_vsize(4, 1) * 2);
    CHECK(sw.change == 0);
    CHECK_THROWS(
        izan::btc::sweep_coins({ { "ff", 0, 600 } }, 10)); // fee eats it

    // The esplora utxo page: unconfirmed entries stay out.
    const char* json = R"([
      {"txid":"a1","vout":0,"value":1000,"status":{"confirmed":true}},
      {"txid":"a2","vout":1,"value":2000,"status":{"confirmed":false}},
      {"txid":"a3","vout":0,"value":3000,"status":{"confirmed":true}}
    ])";
    const auto coins = izan::btc::parse_utxos(json);
    REQUIRE(coins.size() == 2);
    CHECK(coins[0].txid == "a1");
    CHECK(coins[1].value == 3000);
    CHECK_THROWS(izan::btc::parse_utxos("{}"));
}

namespace {

std::vector<uint8_t> unhex_btc(const char* h)
{
    std::vector<uint8_t> out;
    for (const char* p = h; p[0] && p[1]; p += 2) {
        auto nib
            = [](char c) { return uint8_t(c <= '9' ? c - '0' : c - 'a' + 10); };
        out.push_back(uint8_t(nib(p[0]) << 4 | nib(p[1])));
    }
    return out;
}

std::array<uint8_t, 32> txid_be_of_wire(const char* wire_le_hex)
{
    const auto le = unhex_btc(wire_le_hex);
    std::array<uint8_t, 32> be {};
    for (int i = 0; i < 32; ++i)
        be[std::size_t(i)] = le[std::size_t(31 - i)];
    return be;
}

}

TEST_CASE("BIP-143 signs exactly what the spec says it signs")
{
    // The Native P2WPKH example from the BIP itself, verbatim.
    izan::btc::TxPlan plan;
    plan.version = 1;
    plan.locktime = 17;
    izan::btc::TxPlan::In in0;
    in0.txid_be = txid_be_of_wire("fff7f7881a8099afa6940d42d1e7f636"
                                  "2bec38171ea3edf433541db4e4ad969f");
    in0.vout = 0;
    in0.sequence = 0xffffffee;
    izan::btc::TxPlan::In in1;
    in1.txid_be = txid_be_of_wire("ef51e1b804cc89d182d279655c3aa89e"
                                  "815b1b309fe287d9b2b55d57b90ec68a");
    in1.vout = 1;
    in1.value = 600000000;
    in1.sequence = 0xffffffff;
    plan.inputs = { in0, in1 };
    izan::btc::TxPlan::Out out0;
    out0.value = 112340000;
    out0.script
        = unhex_btc("76a9148280b37df378db99f66f85c95a783a76ac7a6d5988ac");
    izan::btc::TxPlan::Out out1;
    out1.value = 223450000;
    out1.script
        = unhex_btc("76a9143bde42dbee7e4dbe6a21b2d50ce2f0167faa815988ac");
    plan.outputs = { out0, out1 };

    const auto h160 = unhex_btc("1d0f172a0ecb48aee1be1f2687d2963ae33f71a1");
    const auto digest = izan::btc::sighash_p2wpkh(
        plan, 1, std::span<const uint8_t, 20>(h160.data(), 20));
    CHECK(std::vector<uint8_t>(digest.begin(), digest.end())
        == unhex_btc("c37af31116d1b27caf68aae9e3ac82f1"
                     "477929014d5b917657d0eb49478cb670"));

    // The skeleton round-trips (values do not ride the wire).
    const auto skel = izan::btc::encode_skeleton(plan);
    const auto back = izan::btc::parse_skeleton(skel);
    CHECK(back.inputs.size() == 2);
    CHECK(back.inputs[1].vout == 1);
    CHECK(back.inputs[0].sequence == 0xffffffee);
    CHECK(back.outputs[1].value == 223450000);
    CHECK(back.locktime == 17);
    CHECK(izan::btc::encode_skeleton(back) == skel);
    auto broken = skel;
    broken.pop_back();
    CHECK_THROWS(izan::btc::parse_skeleton(broken));
}

TEST_CASE("addresses and scripts are inverses across every costume")
{
    const char* faces[] = {
        "1BvBMSEYstWetqTFn5Au4m4GFg7xJaNVN2",
        "3NVZWnhKt53ukKw4Qm217Zk57FE8VnKjH2",
        "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4",
        "bc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qccfmv3",
        "bc1p4qhjn9zdvkux4e44uhx8tc55attvtyu358kutcqkudyccelu0was9fqzwh",
    };
    for (const char* face : faces) {
        const auto script = izan::btc::script_for_address(face);
        CHECK(izan::btc::address_for_script(script) == face);
    }
    CHECK_THROWS(izan::btc::script_for_address("not-an-address"));
    const uint8_t junk[] = { 0x6a, 0x02, 0xaa, 0xbb }; // OP_RETURN
    CHECK(izan::btc::address_for_script(junk).empty());
}

TEST_CASE("the fee market reads in tiers and never runs backwards")
{
    const auto t = izan::btc::parse_fee_estimates(
        R"({"1":25.7,"6":12.1,"144":3.0,"504":1.2})");
    CHECK(t.fast == 26);
    CHECK(t.normal == 13);
    CHECK(t.slow == 3);
    // Sparse answers borrow the faster tier; emptiness floors at 1.
    const auto sparse = izan::btc::parse_fee_estimates(R"({"1":8.0})");
    CHECK(sparse.fast == 8);
    CHECK(sparse.normal == 8);
    CHECK(sparse.slow == 8);
    const auto empty = izan::btc::parse_fee_estimates("{}");
    CHECK(empty.fast == 1);
    // An inverted market (slow above fast) is clamped monotone.
    const auto odd
        = izan::btc::parse_fee_estimates(R"({"1":2.0,"6":9.0,"144":20.0})");
    CHECK(odd.fast == 2);
    CHECK(odd.normal == 2);
    CHECK(odd.slow == 2);
    CHECK_THROWS(izan::btc::parse_fee_estimates("[]"));
}
