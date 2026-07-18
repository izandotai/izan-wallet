#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "core/secure/secure_bytes.hpp"

namespace izan::crypto {

// Native segwit P2WPKH address (bech32, witness v0) from a compressed
// secp256k1 public key. hrp is "bc" for mainnet, "tb" for testnet.
std::string btc_p2wpkh_address(
    std::span<const uint8_t, 33> pubkey, const char* hrp = "bc");

// WIF (Base58Check, version 0x80) → the raw 32-byte secp256k1 key in
// guarded memory; the trailing 0x01 compressed marker is accepted and
// dropped — the key is the key. Anything else (bad checksum, wrong
// version, wrong length, zero scalar) is nullopt, never a guess.
std::optional<secure::SecureBytes> wif_to_key(std::string_view wif);

}
