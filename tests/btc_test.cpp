#include <doctest/doctest.h>

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
