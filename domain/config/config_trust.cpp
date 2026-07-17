#include "domain/config/config_trust.hpp"

#include <cstdint>

extern "C" {
#include <sha2.h>
}

namespace izan::config {

namespace {

    struct Shipped {
        std::string_view name;
        std::string_view sha256;
    };

    // Updated whenever a default config changes; config_trust_test
    // recomputes these from data/ and fails the build on drift, so the
    // table cannot silently rot.
    constexpr Shipped kShipped[] = {
        { "chains.json",
            "c9db90065a36b8e9a060616641aad314e9db0834b98be0435c0c4bcec67b508"
            "1" },
        { "tokens.json",
            "80eec4f97307ccb3720a428b7d94da92f18b3d1aa3f215bca6d1af68416147"
            "9c" },
    };

}

std::string sha256_hex(std::string_view data)
{
    uint8_t digest[SHA256_DIGEST_LENGTH];
    sha256_Raw(
        reinterpret_cast<const uint8_t*>(data.data()), data.size(), digest);
    std::string out;
    out.reserve(sizeof digest * 2);
    constexpr char hex[] = "0123456789abcdef";
    for (const uint8_t b : digest) {
        out += hex[b >> 4];
        out += hex[b & 0xf];
    }
    return out;
}

Trust classify(std::string_view filename, std::string_view contents)
{
    for (const Shipped& s : kShipped)
        if (s.name == filename)
            return sha256_hex(contents) == s.sha256 ? Trust::ShippedDefault
                                                    : Trust::Modified;
    return Trust::Modified;
}

}
