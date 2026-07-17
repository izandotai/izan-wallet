#include "core/crypto/btc.hpp"

#include <array>

extern "C" {
#include <ripemd160.h>
#include <segwit_addr.h>
#include <sha2.h>
}

namespace izan::crypto {

std::string btc_p2wpkh_address(
    std::span<const uint8_t, 33> pubkey, const char* hrp)
{
    // hash160 = ripemd160(sha256(pubkey)), the witness program.
    std::array<uint8_t, 32> sha;
    sha256_Raw(pubkey.data(), pubkey.size(), sha.data());
    std::array<uint8_t, 20> h160;
    ripemd160(sha.data(), sha.size(), h160.data());

    char addr[93];
    if (segwit_addr_encode(addr, hrp, 0, h160.data(), h160.size()) != 1)
        return {};
    return addr;
}

}
