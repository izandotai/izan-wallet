#include "core/secure/vault.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <sodium.h>

namespace izan::vault {

namespace {

    constexpr char kMagic[4] = { 'Q', 'V', 'L', 'T' };
    constexpr uint8_t kVer = 1;
    constexpr uint8_t kAlg = 1; // argon2id + xchacha20poly1305 secretbox
    constexpr std::size_t kSalt = crypto_pwhash_SALTBYTES; // 16
    constexpr std::size_t kNonce
        = crypto_secretbox_xchacha20poly1305_NONCEBYTES;   // 24
    constexpr std::size_t kMac = crypto_secretbox_xchacha20poly1305_MACBYTES;
    constexpr std::size_t kKey = crypto_secretbox_xchacha20poly1305_KEYBYTES;

    void put_u64(std::string& out, uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
            out.push_back(char((v >> (8 * i)) & 0xff));
    }

    uint64_t get_u64(const uint8_t* p)
    {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v |= uint64_t(p[i]) << (8 * i);
        return v;
    }

    // Payload serialization stays inside SecureBytes end to end; routing
    // it through std::string would leave plaintext copies behind.
    SecureBytes pack_payload(const Wallet& wallet)
    {
        std::size_t n = 1 + 1 + wallet.entropy.size() + 1;
        for (const Imported& im : wallet.imported) {
            if (im.label.size() > 255)
                throw std::runtime_error("label too long");
            if (im.key.size() != 32)
                throw std::runtime_error("imported key != 32B");
            n += 1 + im.label.size() + 32;
        }
        if (wallet.imported.size() > 255)
            throw std::runtime_error("too many keys");
        if (wallet.entropy.size() != 0 && wallet.entropy.size() != 16
            && wallet.entropy.size() != 32)
            throw std::runtime_error("entropy must be 0/16/32 bytes");
        SecureBytes out(n);
        uint8_t* p = out.data();
        *p++ = 1; // payload version
        *p++ = uint8_t(wallet.entropy.size());
        if (!wallet.entropy.empty()) {
            std::memcpy(p, wallet.entropy.data(), wallet.entropy.size());
            p += wallet.entropy.size();
        }
        *p++ = uint8_t(wallet.imported.size());
        for (const Imported& im : wallet.imported) {
            *p++ = uint8_t(im.label.size());
            std::memcpy(p, im.label.data(), im.label.size());
            p += im.label.size();
            std::memcpy(p, im.key.data(), 32);
            p += 32;
        }
        return out;
    }

    Wallet unpack_payload(const SecureBytes& payload)
    {
        const uint8_t* p = payload.data();
        const uint8_t* end = p + payload.size();
        auto need = [&](std::size_t k) {
            if (std::size_t(end - p) < k)
                throw std::runtime_error("vault payload truncated");
        };
        need(2);
        if (*p++ != 1)
            throw std::runtime_error("unknown payload version");
        const std::size_t ent = *p++;
        Wallet wallet;
        if (ent) {
            need(ent);
            wallet.entropy = SecureBytes(ent);
            std::memcpy(wallet.entropy.data(), p, ent);
            p += ent;
        }
        need(1);
        const std::size_t count = *p++;
        for (std::size_t i = 0; i < count; ++i) {
            need(1);
            const std::size_t labelLen = *p++;
            need(labelLen + 32);
            Imported im;
            im.label.assign(reinterpret_cast<const char*>(p), labelLen);
            p += labelLen;
            im.key = SecureBytes(32);
            std::memcpy(im.key.data(), p, 32);
            p += 32;
            wallet.imported.push_back(std::move(im));
        }
        return wallet;
    }

    // The passphrase follows C-string semantics (cut at the first '\0') so
    // a prompt and a future UI cannot derive two different KDF keys from
    // "with or without the trailing NUL" length disagreements.
    SecureBytes derive_key(const SecureBytes& passphrase, const uint8_t* salt,
        uint64_t opslimit, uint64_t memlimit)
    {
        const char* pw = reinterpret_cast<const char*>(passphrase.data());
        const std::size_t pwlen = pw ? strnlen(pw, passphrase.size()) : 0;
        if (!pwlen)
            throw std::runtime_error("empty passphrase");
        SecureBytes key(kKey);
        if (crypto_pwhash(key.data(), kKey, pw, pwlen, salt, opslimit,
                static_cast<std::size_t>(memlimit),
                crypto_pwhash_ALG_ARGON2ID13)
            != 0)
            throw std::runtime_error("Argon2id failed (out of memory?)");
        return key;
    }

}

KdfParams kdf_sensitive()
{
    return { crypto_pwhash_OPSLIMIT_SENSITIVE,
        crypto_pwhash_MEMLIMIT_SENSITIVE };
}

KdfParams kdf_min()
{
    return { crypto_pwhash_OPSLIMIT_MIN, crypto_pwhash_MEMLIMIT_MIN };
}

void save(const std::string& path, const SecureBytes& passphrase,
    const Wallet& wallet, const KdfParams& kdf)
{
    if (sodium_init() < 0)
        throw std::runtime_error("sodium_init failed");
    if (passphrase.empty())
        throw std::runtime_error("empty passphrase");

    SecureBytes payload = pack_payload(wallet);

    uint8_t salt[kSalt], nonce[kNonce];
    randombytes_buf(salt, sizeof salt);
    randombytes_buf(nonce, sizeof nonce);
    SecureBytes key = derive_key(passphrase, salt, kdf.opslimit, kdf.memlimit);

    // Ciphertext is not secret; ordinary memory is fine.
    std::vector<uint8_t> ct(payload.size() + kMac);
    if (crypto_secretbox_xchacha20poly1305_easy(
            ct.data(), payload.data(), payload.size(), nonce, key.data())
        != 0)
        throw std::runtime_error("encrypt failed");

    std::string blob;
    blob.reserve(4 + 2 + 16 + kSalt + kNonce + 4 + ct.size());
    blob.append(kMagic, 4);
    blob.push_back(char(kVer));
    blob.push_back(char(kAlg));
    put_u64(blob, kdf.opslimit);
    put_u64(blob, kdf.memlimit);
    blob.append(reinterpret_cast<char*>(salt), kSalt);
    blob.append(reinterpret_cast<char*>(nonce), kNonce);
    const uint32_t n = uint32_t(ct.size());
    for (int i = 0; i < 4; ++i)
        blob.push_back(char((n >> (8 * i)) & 0xff));
    blob.append(reinterpret_cast<char*>(ct.data()), ct.size());

    // Atomic write: tmp + flush + rename; the previous file rotates to
    // .bak, never overwrite in place.
    namespace fs = std::filesystem;
    const fs::path dst(path), tmp(path + ".tmp"), bak(path + ".bak");
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f)
            throw std::runtime_error("cannot write " + tmp.string());
        f.write(blob.data(), std::streamsize(blob.size()));
        f.flush();
        if (!f)
            throw std::runtime_error("write failed " + tmp.string());
    }
    std::error_code ec;
    if (fs::exists(dst)) {
        fs::remove(bak, ec);
        fs::rename(dst, bak, ec); // losing the rotation is not fatal
    }
    fs::rename(tmp, dst);         // throws filesystem_error on failure
}

Wallet open(const std::string& path, const SecureBytes& passphrase)
{
    if (sodium_init() < 0)
        throw std::runtime_error("sodium_init failed");
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot open " + path);
    std::vector<uint8_t> blob(
        (std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const std::size_t head = 4 + 2 + 16 + kSalt + kNonce + 4;
    if (blob.size() < head || std::memcmp(blob.data(), kMagic, 4) != 0)
        throw std::runtime_error("not a vault file");
    const uint8_t* p = blob.data() + 4;
    if (*p++ != kVer)
        throw std::runtime_error("unsupported vault version");
    if (*p++ != kAlg)
        throw std::runtime_error("unsupported vault alg");
    const uint64_t ops = get_u64(p);
    p += 8;
    const uint64_t mem = get_u64(p);
    p += 8;
    const uint8_t* salt = p;
    p += kSalt;
    const uint8_t* nonce = p;
    p += kNonce;
    uint32_t n = 0;
    for (int i = 0; i < 4; ++i)
        n |= uint32_t(*p++) << (8 * i);
    if (blob.size() != head + n || n < kMac)
        throw std::runtime_error("vault file truncated");

    SecureBytes key = derive_key(passphrase, salt, ops, mem);
    SecureBytes payload(n - kMac);
    if (crypto_secretbox_xchacha20poly1305_open_easy(
            payload.data(), p, n, nonce, key.data())
        != 0)
        throw std::runtime_error("wrong passphrase or corrupted vault");
    return unpack_payload(payload);
}

void shred(const std::string& path)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec)
        return; // nothing there
    {
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!f)
            throw std::runtime_error("shred: cannot open " + path);
        uint8_t noise[4096];
        for (std::uintmax_t left = size; left > 0;) {
            const std::size_t n
                = left < sizeof noise ? std::size_t(left) : sizeof noise;
            randombytes_buf(noise, n);
            f.write(reinterpret_cast<const char*>(noise), std::streamsize(n));
            left -= n;
        }
        f.flush();
        if (!f)
            throw std::runtime_error("shred: overwrite failed " + path);
    }
    if (!fs::remove(path))
        throw std::runtime_error("shred: cannot delete " + path);
}

void change_password(const std::string& path, const SecureBytes& old_pass,
    const SecureBytes& new_pass, const KdfParams& kdf)
{
    const Wallet wallet = open(path, old_pass);
    save(path, new_pass, wallet, kdf);
    shred(path + ".bak");
}

}
