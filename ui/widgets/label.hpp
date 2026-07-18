#pragma once

#include <string>

namespace izan::ui {

// Text roles. The language's type scale in wearable form: a screen has
// one title, dialogs get headings, secondary lines are captions.

float kit_title_size();
float kit_heading_size();
float kit_caption_size();

void kit_title(const char* text);
void kit_heading(const char* text);
void kit_caption(const char* text); // muted color
void kit_vspace(float em = 0.5f);   // vertical breath between blocks

// Middle elision to a pixel budget: keep both ends — the parts a
// person actually compares — and give up the middle. ASCII-safe
// slicing; measure with the font that will draw the result.
std::string kit_elide_middle(const char* text, float budget);

}
