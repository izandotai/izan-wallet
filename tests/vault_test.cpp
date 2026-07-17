#include <doctest/doctest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string_view>

#include <sodium.h>

#include "core/secure/vault.hpp"

namespace {

using izan::secure::SecureBytes;

SecureBytes sb_from(std::string_view text)
{
    SecureBytes out(text.size());
    std::memcpy(out.data(), text.data(), text.size());
    return out;
}

std::string temp_vault_path()
{
    return (std::filesystem::temp_directory_path() / "izan_vault_test.qvlt")
        .string();
}

}

TEST_CASE("vault roundtrip, wrong passphrase, tamper detection")
{
    const std::string path = temp_vault_path();
    std::filesystem::remove(path);
    std::filesystem::remove(path + ".bak");

    izan::vault::Wallet wallet;
    wallet.entropy = SecureBytes(32);
    randombytes_buf(wallet.entropy.data(), wallet.entropy.size());

    izan::vault::Imported im;
    im.label = "legacy-hot";
    im.key = SecureBytes(32);
    for (int i = 0; i < 32; ++i)
        im.key.data()[i] = uint8_t(i + 1);
    wallet.imported.push_back(std::move(im));

    SecureBytes pass = sb_from("correct horse battery");
    izan::vault::save(path, pass, wallet, izan::vault::kdf_min());

    izan::vault::Wallet loaded = izan::vault::open(path, pass);
    REQUIRE(loaded.entropy.size() == 32);
    CHECK(std::memcmp(loaded.entropy.data(), wallet.entropy.data(), 32) == 0);
    REQUIRE(loaded.imported.size() == 1);
    CHECK(loaded.imported[0].label == "legacy-hot");
    CHECK(std::memcmp(
              loaded.imported[0].key.data(), wallet.imported[0].key.data(), 32)
        == 0);

    SecureBytes bad = sb_from("wrong pass");
    CHECK_THROWS(izan::vault::open(path, bad));

    // Flip one ciphertext byte: the MAC must reject the file.
    {
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        f.seekg(-1, std::ios::end);
        char c;
        f.get(c);
        f.seekp(-1, std::ios::end);
        f.put(char(c ^ 0x01));
    }
    CHECK_THROWS(izan::vault::open(path, pass));

    std::filesystem::remove(path);
    std::filesystem::remove(path + ".bak");
}

TEST_CASE("vault overwrite rotates the previous file to .bak")
{
    const std::string path = temp_vault_path();
    std::filesystem::remove(path);
    std::filesystem::remove(path + ".bak");

    izan::vault::Wallet wallet;
    wallet.entropy = SecureBytes(16);
    randombytes_buf(wallet.entropy.data(), wallet.entropy.size());

    SecureBytes pass = sb_from("rotate me");
    izan::vault::save(path, pass, wallet, izan::vault::kdf_min());
    CHECK(!std::filesystem::exists(path + ".bak"));

    izan::vault::save(path, pass, wallet, izan::vault::kdf_min());
    CHECK(std::filesystem::exists(path + ".bak"));

    // The rotated copy is the previous generation and still opens.
    izan::vault::Wallet fromBak = izan::vault::open(path + ".bak", pass);
    CHECK(fromBak.entropy.size() == 16);

    std::filesystem::remove(path);
    std::filesystem::remove(path + ".bak");
}

TEST_CASE("vault rejects garbage and truncated files")
{
    const std::string path = temp_vault_path();
    SecureBytes pass = sb_from("whatever");

    CHECK_THROWS(izan::vault::open(path + ".does-not-exist", pass));

    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        f << "not a vault at all";
    }
    CHECK_THROWS(izan::vault::open(path, pass));
    std::filesystem::remove(path);
}

TEST_CASE("vault refuses an empty passphrase and bad shapes")
{
    const std::string path = temp_vault_path();
    izan::vault::Wallet wallet;
    wallet.entropy = SecureBytes(16);
    randombytes_buf(wallet.entropy.data(), wallet.entropy.size());

    SecureBytes empty;
    CHECK_THROWS(
        izan::vault::save(path, empty, wallet, izan::vault::kdf_min()));

    izan::vault::Wallet badEntropy;
    badEntropy.entropy = SecureBytes(15);
    SecureBytes pass = sb_from("ok pass");
    CHECK_THROWS(
        izan::vault::save(path, pass, badEntropy, izan::vault::kdf_min()));

    izan::vault::Wallet badKey;
    izan::vault::Imported im;
    im.label = "short";
    im.key = SecureBytes(31);
    badKey.imported.push_back(std::move(im));
    CHECK_THROWS(izan::vault::save(path, pass, badKey, izan::vault::kdf_min()));
}
