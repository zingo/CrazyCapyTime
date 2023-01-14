#pragma once

#include <lvgl.h>

extern lv_style_t styleIcon;
extern lv_style_t styleIconOff;

// Call this from setup() to init GFX
void initLVGL();

// Call this in the loop to update GFX
void loopHandlLVGL();