#include "core/crypto/btc.hpp"

#include <array>
#include <cstring>

#include <sodium.h>

extern "C" {
#include <base58.h>
#include <hasher.h>
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

std::optional<secure::SecureBytes> wif_to_key(std::string_view wif)
{
    // Base58Check payloads: 0x80 || key(32) [|| 0x01 compressed marker].
    if (wif.size() < 50 || wif.size() > 53)
        return std::nullopt;
    const std::string z(wif); // decode wants a terminator
    uint8_t decoded[40];
    const int n
        = base58_decode_check(z.c_str(), HASHER_SHA2D, decoded, sizeof decoded);
    std::optional<secure::SecureBytes> out;
    if ((n == 33 || n == 34) && decoded[0] == 0x80
        && (n == 33 || decoded[33] == 0x01)) {
        bool nonzero = false;
        for (int i = 1; i <= 32; ++i)
            nonzero = nonzero || decoded[i];
        if (nonzero) {
            out.emplace(32);
            std::memcpy(out->data(), decoded + 1, 32);
        }
    }
    sodium_memzero(decoded, sizeof decoded);
    return out;
}

}
