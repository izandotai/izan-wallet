#pragma once

#include <imgui.h>

namespace izan::ui {

// Identity avatar: rounded square, color minted from the name, the
// name's first character as the glyph.
void kit_avatar(const char* name, float size);
// Same, drawn at an absolute position without touching the cursor —
// for composing custom rows.
void kit_avatar_at(ImVec2 pos, const char* name, float size);

}
