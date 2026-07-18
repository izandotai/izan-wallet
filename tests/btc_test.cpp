#include <doctest/doctest.h>

#include <cstring>
#include <string>

#include "core/crypto/bip39.hpp"
#include "core/crypto/btc.hpp"
#include "core/crypto/hd.hpp"

#include "data/bip84_vectors.inc"

TEST_CASE("BIP-84 official vectors: mnemonic → P2WPKH addresses")
{
    const auto seed = izan::crypto::mnemonic_to_seed(kBip84Mnemonic, "");
    const auto root = izan::crypto::HdKey::from_seed(seed);
    REQUIRE(root);
    for (const auto& v : kBip84Vectors) {
        CAPTURE(v.path);
        const auto key = root->derive(v.path);
        REQUIRE(key);
        CHECK(izan::crypto::btc_p2wpkh_address(key->public_key_compressed())
            == v.address);
    }
}

TEST_CASE("WIF decodes to its key or to nothing")
{
    // The Bitcoin wiki's canonical example: uncompressed WIF for key
    // 0x0C28FCA3…AA1D.
    const char* wif = "5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ";
    const uint8_t expect[32]
        = { 0x0C, 0x28, 0xFC, 0xA3, 0x86, 0xC7, 0xA2, 0x27, 0x60, 0x0B, 0x2F,
              0xE5, 0x0B, 0x7C, 0xAE, 0x11, 0xEC, 0x86, 0xD3, 0xBF, 0x1F, 0xBE,
              0x47, 0x1B, 0xE8, 0x98, 0x27, 0xE1, 0x9D, 0x72, 0xAA, 0x1D };
    auto key = izan::crypto::wif_to_key(wif);
    REQUIRE(key);
    REQUIRE(key->size() == 32);
    CHECK(std::memcmp(key->data(), expect, 32) == 0);

    // One flipped character breaks the checksum: refused, not guessed.
    std::string tampered(wif);
    tampered[10] = tampered[10] == 'D' ? 'E' : 'D';
    CHECK(!izan::crypto::wif_to_key(tampered));

    CHECK(!izan::crypto::wif_to_key(""));
    CHECK(!izan::crypto::wif_to_key("not-a-wif"));
    // A valid Base58Check string of the wrong shape (a P2PKH address)
    // is not a key either.
    CHECK(!izan::crypto::wif_to_key("1BvBMSEYstWetqTFn5Au4m4GFg7xJaNVN2"));
}
