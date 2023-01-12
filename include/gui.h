#pragma once

#include <lvgl.h>

extern lv_style_t style_icon;
extern lv_style_t style_icon_off;

// Call this from setup() to init GFX
void initLVGL();

// Call this in the loop to update GFX
void loopHandlLVGL();