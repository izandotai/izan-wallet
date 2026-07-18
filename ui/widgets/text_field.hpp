#pragma once

#include <array>
#include <cstddef>

#include "core/secure/secure_bytes.hpp"

namespace izan::ui {

// Text entry. Every field shows its purpose as a hint inside the empty
// box and submits on Enter.

// Ordinary text; returns true on Enter.
bool kit_text_field(
    const char* id, const char* hint, char* buf, std::size_t size);

// A password field that reports its focus into the page's secret-focus
// aggregate — the flag the IME detach rides on. Field-level, not
// form-level: ordinary text next to it keeps CJK input working. The
// hint shows inside the empty field; returns true when Enter submits.
bool secret_field(const char* label, std::array<char, 256>& buf,
    bool& secret_focus, const char* hint = nullptr);

// The paste box for secrets-in-transit (mnemonics, keys): multiline,
// IME-detached while focused. Returns true when the text changed.
bool kit_paste_box(const char* id, char* buf, std::size_t size, float rows,
    bool& secret_focus);

// Moves the buffer contents into guarded memory and wipes the buffer.
secure::SecureBytes take_secret(std::array<char, 256>& buf);

}
