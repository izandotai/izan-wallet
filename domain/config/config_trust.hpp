#pragma once

#include <string>
#include <string_view>

namespace izan::config {

// §3.1 hardening item 6. chains.json and tokens.json cannot leak a
// key, but a rewritten copy can feed fake balances or point broadcasts
// at a hostile RPC — so the shipped defaults are digest-pinned in the
// binary and any drift is surfaced loudly in the UI. A user's own
// edits land in the same Modified bucket on purpose: the warning tells
// the truth ("this is not what we shipped") and the user who edited it
// knows why. Pinning digests, not signatures: an attacker who can
// rewrite the binary could rewrite an embedded public key just as
// easily, so a signature here would add ceremony, not protection.
enum class Trust {
    ShippedDefault,
    Modified,
};

// Classifies a config file's contents against the shipped digest for
// that filename. Unknown filenames are Modified by definition.
Trust classify(std::string_view filename, std::string_view contents);

std::string sha256_hex(std::string_view data);

}
