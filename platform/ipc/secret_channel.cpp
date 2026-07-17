#include "platform/ipc/secret_channel.hpp"

#include <cstring>
#include <stdexcept>

#include <sodium.h>

namespace izan::ipc {

struct SecretChannel::State {
    crypto_secretstream_xchacha20poly1305_state push;
    crypto_secretstream_xchacha20poly1305_state pull;
    uint8_t key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
};

SecretChannel::SecretChannel(
    PipeEnd in, PipeEnd out, const secure::SecureBytes& key)
    : m_in(std::move(in))
    , m_out(std::move(out))
    , m_state(new State)
{
    if (sodium_init() < 0)
        throw std::runtime_error("sodium_init failed");
    if (key.size() != kKeyBytes)
        throw std::invalid_argument("secret channel: bad key size");
    std::memcpy(m_state->key, key.data(), kKeyBytes);

    uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_init_push(
        &m_state->push, header, m_state->key);
    if (!m_out.write_all(header, sizeof header))
        throw std::runtime_error("secret channel: peer gone at handshake");
}

SecretChannel::~SecretChannel()
{
    close();
    if (m_state) {
        sodium_memzero(m_state, sizeof(State));
        delete m_state;
    }
}

void SecretChannel::close()
{
    m_in.close();
    m_out.close();
}

bool SecretChannel::send(const uint8_t* data, std::size_t size)
{
    if (size > kMaxFrame - crypto_secretstream_xchacha20poly1305_ABYTES)
        throw std::invalid_argument("secret channel: frame too large");
    std::vector<uint8_t> ct(
        size + crypto_secretstream_xchacha20poly1305_ABYTES);
    unsigned long long ctLen = 0;
    crypto_secretstream_xchacha20poly1305_push(
        &m_state->push, ct.data(), &ctLen, data, size, nullptr, 0, 0);
    uint8_t len[4] = { uint8_t(ctLen), uint8_t(ctLen >> 8),
        uint8_t(ctLen >> 16), uint8_t(ctLen >> 24) };
    return m_out.write_all(len, 4) && m_out.write_all(ct.data(), ctLen);
}

std::optional<secure::SecureBytes> SecretChannel::recv()
{
    if (!m_pull_ready) {
        uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
        if (!m_in.read_all(header, sizeof header))
            return std::nullopt;
        if (crypto_secretstream_xchacha20poly1305_init_pull(
                &m_state->pull, header, m_state->key)
            != 0)
            throw std::runtime_error("secret channel: bad stream header");
        m_pull_ready = true;
    }

    uint8_t len[4];
    if (!m_in.read_all(len, 4))
        return std::nullopt;
    const std::size_t ctLen = std::size_t(len[0]) | std::size_t(len[1]) << 8
        | std::size_t(len[2]) << 16 | std::size_t(len[3]) << 24;
    if (ctLen < crypto_secretstream_xchacha20poly1305_ABYTES
        || ctLen > kMaxFrame)
        throw std::runtime_error("secret channel: bad frame length");
    std::vector<uint8_t> ct(ctLen);
    if (!m_in.read_all(ct.data(), ctLen))
        return std::nullopt;

    secure::SecureBytes msg(
        ctLen - crypto_secretstream_xchacha20poly1305_ABYTES);
    unsigned long long msgLen = 0;
    uint8_t tag = 0;
    if (crypto_secretstream_xchacha20poly1305_pull(&m_state->pull, msg.data(),
            &msgLen, &tag, ct.data(), ctLen, nullptr, 0)
        != 0)
        throw std::runtime_error("secret channel: frame failed auth");
    return msg;
}

}
