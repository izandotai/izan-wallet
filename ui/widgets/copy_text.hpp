#pragma once

namespace izan::ui {

// Click-to-copy text with visible feedback: shows `shown`, copies
// `full` on click, then swaps to the confirmation for a moment so the
// copy is never silent. Hovering shows the full text plus the hint.
void kit_copy_text(const char* id, const char* shown, const char* full,
    const char* hint, const char* copied_label);

// Same, right-aligned to the current content region's edge — for the
// trailing column of a row.
void kit_copy_text_right(const char* id, const char* shown, const char* full,
    const char* hint, const char* copied_label);

}
