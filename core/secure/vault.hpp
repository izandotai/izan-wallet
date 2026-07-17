#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/secure/secure_bytes.hpp"

namespace izan::vault {

using secure::SecureBytes;

// Encrypted vault file for HD wallet material.
//
// File format v1 (all integers little-endian):
//   magic "QVLT"(4) | ver(1)=1 | alg(1)=1 (argon2id + xchacha20poly1305)
//   | opslimit(u64) | memlimit(u64) | salt(16) | nonce(24)
//   | ct_len(u32) | ciphertext+MAC
// Plaintext payload (lives only in SecureBytes):
//   pver(1)=1 | ent_len(1) | entropy | n_imported(1)
//   | { label_len(1) | label | key(32) } * n
//
// Design: store entropy, not the mnemonic (the wordlist regenerates it);
// KDF parameters travel in the header so future retuning cannot orphan
// old files; writes go tmp + atomic rename with the previous file
// rotated to .bak.

struct Imported {
    std::string label; // not secret (e.g. "pm-env")
    SecureBytes key;   // 32-byte raw private key; unrecoverable from the
                       // seed, the vault itself is its only backup
};

struct Wallet {
    SecureBytes entropy; // BIP-39 entropy (16/32 bytes); empty means the
                         // wallet holds imported keys only
    std::vector<Imported> imported;
};

// KDF tiers. Default to sensitive: unlocking takes about a second and
// 1 GB of memory — it is a rare action and slow is the feature. The min
// tier exists for tests only.
struct KdfParams {
    unsigned long long opslimit;
    std::size_t memlimit;
};

KdfParams kdf_sensitive();
KdfParams kdf_min();

// Create or overwrite a vault; an existing file rotates to path.bak.
void save(const std::string& path, const SecureBytes& passphrase,
    const Wallet& wallet, const KdfParams& kdf);

// Open a vault. A wrong passphrase or a tampered file (MAC failure)
// throws.
Wallet open(const std::string& path, const SecureBytes& passphrase);

}
