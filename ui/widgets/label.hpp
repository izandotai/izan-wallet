#pragma once

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

}
