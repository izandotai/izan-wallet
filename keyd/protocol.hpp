#pragma once

#include <cstdint>

namespace izan::keyd {

// Password-channel protocol v1. One opcode byte, then the body. The
// deliberate poverty of this protocol is the point: the trust plane's
// entire remote surface fits on one screen.
enum class Op : uint8_t {
    // requests (UI → keyd)
    Unlock = 0x01, // body: passphrase bytes
    Lock = 0x02,
    Status = 0x03,
    Shutdown = 0x04,

    // replies (keyd → UI)
    Hello = 0x40, // body: version, hardening bitmask (sent once at start)
    Ok = 0x41,
    Err = 0x42,   // body: utf-8 reason
    State = 0x43, // body: 0 = locked, 1 = unlocked
};

inline constexpr uint8_t kProtocolVersion = 1;

// Hello hardening bitmask: which §3.1 process protections engaged.
inline constexpr uint8_t kHardenedDumps = 1 << 0;   // WER excluded
inline constexpr uint8_t kHardenedDynCode = 1 << 1; // no dynamic code
inline constexpr uint8_t kHardenedDllSig = 1 << 2;  // MS-signed DLLs only
inline constexpr uint8_t kHardenedDllDirs = 1 << 3; // system32-only search

}
