#pragma once

#include <cstddef>

namespace izan::ui {

// The amount, writ large: a centered, box-free numeric entry that
// grows with its digits. On a send screen the sum is the subject —
// everything else is supporting cast. Returns true on Enter.
bool kit_amount_field(const char* id, char* buf, std::size_t size);

}
