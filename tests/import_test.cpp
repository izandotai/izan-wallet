// What a pasted wallet secret turns out to be — the import wizard's
// recognition line rides on this classification, so a wrong guess here
// becomes a wallet built around the wrong key.

#include <doctest/doctest.h>

#include <cstring>
#include <string>

#include "core/crypto/secret_import.hpp"

using izan::crypto::detect_secret;
using izan::crypto::SecretKind;

namespace {

// The Bitcoin wiki's canonical key, in both of its costumes.
constexpr const char* kKeyHex
    = "0c28fca386c7a227600b2fe50b7cae11ec86d3bf1fbe471be89827e19d72aa1d";
constexpr const char* kKeyWif
    = "5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ";

}

TEST_CASE("a BIP-39 sentence is recognized as a mnemonic")
{
    const auto hit
        = detect_secret("abandon abandon abandon abandon abandon abandon "
                        "abandon abandon abandon abandon abandon about");
    CHECK(hit.kind == SecretKind::Mnemonic);
    CHECK(hit.key.empty());

    // Pasted text arrives with stray whitespace; the secret is the words.
    const auto padded
        = detect_secret("  legal winner thank year wave sausage worth useful "
                        "legal winner thank yellow\n");
    CHECK(padded.kind == SecretKind::Mnemonic);

    // Right words, wrong checksum: not a mnemonic, and since it is
    // neither hex nor WIF, not anything else either.
    const auto bad
        = detect_secret("abandon abandon abandon abandon abandon abandon "
                        "abandon abandon abandon abandon abandon abandon");
    CHECK(bad.kind == SecretKind::Unrecognized);
}

TEST_CASE("64 hex digits are a raw key, with or without 0x")
{
    const auto plain = detect_secret(kKeyHex);
    REQUIRE(plain.kind == SecretKind::RawKey);
    REQUIRE(plain.key.size() == 32);
    CHECK(plain.key.data()[0] == 0x0c);
    CHECK(plain.key.data()[31] == 0x1d);

    const auto prefixed = detect_secret("0x" + std::string(kKeyHex));
    REQUIRE(prefixed.kind == SecretKind::RawKey);
    CHECK(std::memcmp(prefixed.key.data(), plain.key.data(), 32) == 0);

    // 63 digits is a typo, not a shorter key.
    CHECK(detect_secret(std::string(kKeyHex).substr(1)).kind
        == SecretKind::Unrecognized);
    // The zero scalar cannot sign anything.
    CHECK(detect_secret(std::string(64, '0')).kind == SecretKind::Unrecognized);
}

TEST_CASE("a WIF string decodes to the same key its hex form carries")
{
    const auto wif = detect_secret(kKeyWif);
    REQUIRE(wif.kind == SecretKind::Wif);
    REQUIRE(wif.key.size() == 32);

    const auto hex = detect_secret(kKeyHex);
    CHECK(std::memcmp(wif.key.data(), hex.key.data(), 32) == 0);

    // One flipped character breaks the checksum: refused, not guessed.
    std::string tampered(kKeyWif);
    tampered[10] = tampered[10] == 'D' ? 'E' : 'D';
    CHECK(detect_secret(tampered).kind == SecretKind::Unrecognized);
}

TEST_CASE("anything else is refused")
{
    CHECK(detect_secret("").kind == SecretKind::Unrecognized);
    CHECK(detect_secret("   \n\t ").kind == SecretKind::Unrecognized);
    CHECK(detect_secret("hello world").kind == SecretKind::Unrecognized);
    // A valid Base58Check string of the wrong shape (a P2PKH address).
    CHECK(detect_secret("1BvBMSEYstWetqTFn5Au4m4GFg7xJaNVN2").kind
        == SecretKind::Unrecognized);
}
