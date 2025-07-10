#pragma once

#include <lvgl.h>

static const uint32_t screenWidth = 800;
static const uint32_t screenHeight = 480;

// Call this from setup() to init GFX
void initLVGL();

// Call this in the loop to update GFX
void loopHandlLVGL();