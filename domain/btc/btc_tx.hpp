#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace izan::btc {

// The transaction the wallet plans and keyd reads back: P2WPKH inputs
// only (v1), any standard single-key output. Everything here is pure
// bytes — no network, shared by the send flow and the trust plane.

// RBF by default: replaceable, still locktime-honest.
inline constexpr uint32_t kSequenceRbf = 0xfffffffd;

struct TxPlan {
    struct In {
        std::array<uint8_t, 32> txid_be {}; // as explorers print it
        uint32_t vout = 0;
        uint64_t value = 0; // satoshi — BIP-143 signs the amount
        uint32_t sequence = kSequenceRbf;
    };

    struct Out {
        std::vector<uint8_t> script;
        uint64_t value = 0;
    };

    std::vector<In> inputs;
    std::vector<Out> outputs;
    uint32_t version = 2;
    uint32_t locktime = 0;
};

// Address ↔ scriptPubKey, both directions. Encode throws on an
// unknown address form; decode returns empty for a script no address
// can name (the whitelist treats that as refusal).
std::vector<uint8_t> script_for_address(std::string_view address);
std::string address_for_script(std::span<const uint8_t> script);

// The unsigned skeleton: what rides the proposal queue and what the
// txid is computed over (witnesses never touch the txid).
std::vector<uint8_t> encode_skeleton(const TxPlan& plan);
TxPlan parse_skeleton(std::span<const uint8_t> bytes); // throws on junk
// Skeleton values are not serialized (BIP-143 needs them; they ride
// the envelope separately) — parse_skeleton leaves them zero.

// BIP-143 digest for one P2WPKH input: what the key actually signs.
// pubkey_hash160 is the spent output's own program.
std::array<uint8_t, 32> sighash_p2wpkh(const TxPlan& plan,
    std::size_t input_index, std::span<const uint8_t, 20> pubkey_hash160);

// The final wire bytes: skeleton plus one witness (signature with the
// SIGHASH_ALL tag, then the 33-byte pubkey) per input, in order.
struct Witness {
    std::vector<uint8_t> signature_der; // without the hashtype byte
    std::array<uint8_t, 33> pubkey {};
};

std::vector<uint8_t> assemble_tx(
    const TxPlan& plan, std::span<const Witness> witnesses);

// txid (big-endian hex, as explorers print it) and virtual size.
std::string txid_of(const TxPlan& plan);
uint64_t vsize_of(const TxPlan& plan, std::span<const Witness> witnesses);

}
