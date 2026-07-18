#include <doctest/doctest.h>

#include "domain/assets/history.hpp"

using izan::assets::parse_txlist;

namespace {

constexpr const char* kSelf = "0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045";

}

TEST_CASE("txlist rows judged from our side of the ledger")
{
    const auto rows = parse_txlist(R"({"status":"1","message":"OK","result":[
        {"hash":"0xaaa","from":"0xd8da6bf26964af9d7eed9e03e53415d37aa96045",
         "to":"0x70997970c51812dc3a010c7d01b50e0d17dc79c8",
         "value":"1000000000000000","timeStamp":"1789000000","isError":"0"},
        {"hash":"0xbbb","from":"0x70997970c51812dc3a010c7d01b50e0d17dc79c8",
         "to":"0xd8da6bf26964af9d7eed9e03e53415d37aa96045",
         "value":"2000000000000000","timeStamp":"1789000100","isError":"0"},
        {"hash":"0xccc","from":"0xd8da6bf26964af9d7eed9e03e53415d37aa96045",
         "to":"0x70997970c51812dc3a010c7d01b50e0d17dc79c8",
         "value":"0","timeStamp":"1789000200","isError":"1"}
    ]})",
        kSelf);
    REQUIRE(rows.size() == 3);
    CHECK_FALSE(rows[0].incoming);
    CHECK(rows[0].counterparty == "0x70997970c51812dc3a010c7d01b50e0d17dc79c8");
    CHECK(rows[0].time == 1789000000);
    CHECK_FALSE(rows[0].failed);
    CHECK(rows[1].incoming);
    CHECK(rows[1].counterparty == "0x70997970c51812dc3a010c7d01b50e0d17dc79c8");
    CHECK(rows[2].failed);
}

TEST_CASE("txlist oddities")
{
    // "No transactions found" is an empty ledger, not an error.
    CHECK(parse_txlist(
        R"({"status":"0","message":"No transactions found","result":[]})",
        kSelf)
            .empty());
    CHECK(parse_txlist(
        R"({"status":"0","message":"NOTOK","result":"Max rate limit"})", kSelf)
            .empty());
    CHECK_THROWS(parse_txlist("not json", kSelf));
    CHECK_THROWS(parse_txlist(R"({"status":"1"})", kSelf));
    // A row without a readable value is dropped, not invented.
    const auto rows = parse_txlist(R"({"status":"1","result":[
        {"hash":"0xddd","from":"0x01","to":"0x02","value":"nope",
         "timeStamp":"1","isError":"0"},
        {"hash":"0xeee","from":"0x01",
         "to":"0xd8da6bf26964af9d7eed9e03e53415d37aa96045",
         "value":"5","timeStamp":"2","isError":"0"}
    ]})",
        kSelf);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].hash == "0xeee");
    CHECK(rows[0].incoming);
}
