#pragma once

#include <cstdint>

namespace izan::keyd {

// Applies the §3.1 process hardening set to the calling process and
// reports what actually engaged as a protocol bitmask (kHardened*).
// Call before any key material exists in the process:
//  - crash dumps off (a minidump is key material on disk),
//  - dynamic code prohibited (a static wallet never JITs),
//  - DLL loads restricted to Microsoft-signed from system32 (a static
//    exe imports nothing else; this slams the classic injection door).
uint8_t apply_process_hardening();

}
