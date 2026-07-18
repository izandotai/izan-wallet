#pragma once

#include <string_view>

#include "core/secure/secure_bytes.hpp"

namespace izan::crypto {

// What a pasted wallet secret turned out to be. Content decides: a
// valid BIP-39 sentence, exactly 64 hex digits, a WIF string — anything
// else is refused, never guessed at.
enum class SecretKind {
    Unrecognized,
    Mnemonic, // valid BIP-39 sentence; the words stay in the caller's buffer
    RawKey,   // 64 hex digits, optional 0x prefix
    Wif,      // Base58Check, version 0x80
};

struct DetectedSecret {
    SecretKind kind = SecretKind::Unrecognized;
    secure::SecureBytes key; // RawKey/Wif: the 32-byte scalar; else empty
};

DetectedSecret detect_secret(std::string_view text);

}
