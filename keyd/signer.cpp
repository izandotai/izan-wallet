#include "keyd/signer.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

#include <sodium.h>

extern "C" {
#include <ecdsa.h>
#include <memzero.h>
#include <secp256k1.h>
#include <sha3.h>
}

#include "core/crypto/bip39.hpp"
#include "core/crypto/eth.hpp"

namespace izan::keyd {

namespace {

    crypto::HdKey account_key(
        const secure::SecureBytes& entropy, uint32_t account)
    {
        const secure::SecureBytes mnemonic
            = crypto::entropy_to_mnemonic(entropy);
        crypto::Seed seed = crypto::mnemonic_to_seed(
            reinterpret_cast<const char*>(mnemonic.data()), "");
        std::optional<crypto::HdKey> root = crypto::HdKey::from_seed(seed);
        sodium_memzero(seed.data(), seed.size());
        if (!root)
            throw std::runtime_error("signer: seed rejected");
        std::optional<crypto::HdKey> key
            = root->derive(kEthAccountPathPrefix + std::to_string(account));
        if (!key)
            throw std::runtime_error("signer: account underivable");
        return *key;
    }

    // The imported-key path: no derivation, the key IS the identity.
    void sign_with_raw(const secure::SecureBytes& key,
        std::span<const uint8_t, 32> digest, SignedDigest& out)
    {
        if (key.size() != 32)
            throw std::runtime_error("signer: malformed imported key");
        uint8_t sig[64];
        uint8_t parity = 0;
        if (ecdsa_sign_digest(
                &secp256k1, key.data(), digest.data(), sig, &parity, nullptr)
            != 0)
            throw std::runtime_error("signer: signing failed");
        std::memcpy(out.sig.r.data(), sig, 32);
        std::memcpy(out.sig.s.data(), sig + 32, 32);
        out.sig.y_parity = parity;
        memzero(sig, sizeof sig);

        uint8_t pub[65];
        ecdsa_get_public_key65(&secp256k1, key.data(), pub);
        out.signer = crypto::eth_address(std::span<const uint8_t, 65>(pub, 65));
    }

}

ProposalBody parse_proposal(std::span<const uint8_t> payload)
{
    ProposalBody body;
    if (!payload.empty() && payload.front() == kEnvelopeV1) {
        if (payload.size() < 5)
            throw std::invalid_argument("signer: truncated envelope");
        body.account = uint32_t(payload[1]) | uint32_t(payload[2]) << 8
            | uint32_t(payload[3]) << 16 | uint32_t(payload[4]) << 24;
        body.tx = payload.subspan(5);
    } else {
        body.tx = payload;
    }
    if (body.tx.empty())
        throw std::invalid_argument("signer: empty payload");
    return body;
}

SignedDigest sign_payload(
    const vault::Wallet& wallet, std::span<const uint8_t> tx, uint32_t account)
{
    if (tx.empty())
        throw std::invalid_argument("signer: empty payload");

    SignedDigest out;
    keccak_256(tx.data(), tx.size(), out.digest.data());

    if (!wallet.entropy.empty()) {
        const crypto::HdKey key = account_key(wallet.entropy, account);
        const std::optional<crypto::EcdsaSignature> sig
            = key.sign_digest(out.digest);
        if (!sig)
            throw std::runtime_error("signer: signing failed");
        out.sig = *sig;
        out.signer = crypto::eth_address(key.public_key_uncompressed());
    } else if (!wallet.imported.empty()) {
        if (account != 0)
            throw std::invalid_argument(
                "signer: a key wallet has a single address");
        sign_with_raw(wallet.imported.front().key, out.digest, out);
    } else {
        throw std::invalid_argument("signer: wallet holds no signing key");
    }
    return out;
}

std::string account_address(const vault::Wallet& wallet, uint32_t account)
{
    if (!wallet.entropy.empty())
        return crypto::eth_address(
            account_key(wallet.entropy, account).public_key_uncompressed());
    if (!wallet.imported.empty()) {
        if (account != 0)
            throw std::invalid_argument(
                "signer: a key wallet has a single address");
        const secure::SecureBytes& key = wallet.imported.front().key;
        if (key.size() != 32)
            throw std::runtime_error("signer: malformed imported key");
        uint8_t pub[65];
        ecdsa_get_public_key65(&secp256k1, key.data(), pub);
        return crypto::eth_address(std::span<const uint8_t, 65>(pub, 65));
    }
    throw std::invalid_argument("signer: wallet holds no signing key");
}

}
