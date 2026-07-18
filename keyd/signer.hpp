#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include "core/crypto/hd.hpp"
#include "core/secure/secure_bytes.hpp"
#include "core/secure/vault.hpp"

namespace izan::keyd {

// A wallet's identities, chosen by its own contents: a seed wallet
// derives the standard Ethereum account line (one address per index —
// HD means endless addresses), a key-only wallet IS its single key.
// The index arrives through the proposal envelope below, never as a
// free-form path an attacker could point at any branch of the tree.
inline constexpr char kEthAccountPathPrefix[] = "m/44'/60'/0'/0/";

struct SignedDigest {
    crypto::EcdsaSignature sig;
    std::array<uint8_t, 32> digest; // keccak of the tx bytes — audit anchor
    std::string signer;             // EIP-55 address the signature recovers to
};

// Proposal payload envelope v1: 0x01 | account(u32 LE) | tx bytes.
// A bare payload (anything not starting 0x01 — typed transactions
// start 0x02) is account #0, so envelope-less submitters keep working.
// The envelope is part of what sits in the queue: the account the
// human approves is the account that signs.
inline constexpr uint8_t kEnvelopeV1 = 0x01;

struct ProposalBody {
    uint32_t account = 0;
    std::span<const uint8_t> tx; // the bytes whose keccak gets signed
};

// Splits an envelope off a queued payload. Throws on a truncated
// envelope or an empty tx body.
ProposalBody parse_proposal(std::span<const uint8_t> payload);

// The moment key material meets a transaction: wallet + account index
// → signing key → sign keccak256(tx). Every intermediate (mnemonic,
// seed, derived key) is wiped before this returns; the caller decides
// what the bytes mean — this function only guarantees the bytes signed
// are exactly the bytes given. Throws on any failure: a wallet with
// nothing to sign with, or an index a key-only wallet cannot have.
SignedDigest sign_payload(const vault::Wallet& wallet,
    std::span<const uint8_t> tx, uint32_t account = 0);

// The account's EIP-55 address — the same key selection as
// sign_payload, stopping at the public half.
std::string account_address(const vault::Wallet& wallet, uint32_t account = 0);

}
