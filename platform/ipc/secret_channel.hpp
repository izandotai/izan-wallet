#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/secure/secure_bytes.hpp"
#include "platform/ipc/pipe.hpp"

namespace izan::ipc {

// Authenticated encrypted duplex channel over a pair of pipe ends,
// keyed by a one-shot 32-byte session key handed over at spawn.
// Transport is libsodium secretstream (XChaCha20-Poly1305): a fake
// endpoint without the key cannot forge a single byte, and any
// tampered frame kills the session.
//
// Wire format per direction: 24-byte stream header once, then frames
// of u32-LE ciphertext length + ciphertext. Both sides write their own
// header eagerly and read the peer's lazily on first receive, so a
// single thread can drive both ends without deadlock.
class SecretChannel {
public:
    static constexpr std::size_t kKeyBytes = 32;
    // A control frame is tiny; anything bigger than a vault payload is
    // an attack or a bug.
    static constexpr std::size_t kMaxFrame = 1 << 20;

    SecretChannel(PipeEnd in, PipeEnd out, const secure::SecureBytes& key);
    ~SecretChannel();

    SecretChannel(const SecretChannel&) = delete;
    SecretChannel& operator=(const SecretChannel&) = delete;

    // false = peer gone.
    bool send(const uint8_t* data, std::size_t size);

    // All incoming plaintext lands in guarded memory — the channel
    // carries passphrases and cannot know in advance which frames are
    // secret. nullopt = peer closed cleanly or pipe broke; a frame
    // that fails authentication throws.
    std::optional<secure::SecureBytes> recv();

    void close();

private:
    PipeEnd m_in;
    PipeEnd m_out;
    // crypto_secretstream_xchacha20poly1305_state, kept opaque so this
    // header stays free of <sodium.h>.
    struct State;
    State* m_state;
    bool m_pull_ready = false;
};

}
