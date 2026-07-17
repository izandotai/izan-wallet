#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace izan::crypto {

// Native segwit P2WPKH address (bech32, witness v0) from a compressed
// secp256k1 public key. hrp is "bc" for mainnet, "tb" for testnet.
std::string btc_p2wpkh_address(
    std::span<const uint8_t, 33> pubkey, const char* hrp = "bc");

}
