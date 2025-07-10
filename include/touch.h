#pragma once
/*******************************************************************************
 * Touch libraries:
 * FT6X36: https://github.com/strange-v/FT6X36.git
 * GT911: https://github.com/TAMCTec/gt911-arduino.git
 * XPT2046: https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
 ******************************************************************************/

/* uncomment for FT6X36 */
// #define TOUCH_FT6X36
// #define TOUCH_FT6X36_SCL 19
// #define TOUCH_FT6X36_SDA 18
// #define TOUCH_FT6X36_INT 39
// #define TOUCH_SWAP_XY
// #define TOUCH_MAP_X1 480
// #define TOUCH_MAP_X2 0
// #define TOUCH_MAP_Y1 0
// #define TOUCH_MAP_Y2 320




/* uncomment for GT911 */
#define TOUCH_GT911


 #define TOUCH_GT911_INT -1
 #define TOUCH_GT911_RST 38
 #define TOUCH_GT911_ROTATION ROTATION_NORMAL
 #define TOUCH_MAP_X2 0
 #define TOUCH_MAP_Y2 0

/* uncomment for XPT2046 */
// #define TOUCH_XPT2046
// #define TOUCH_XPT2046_SCK 12
// #define TOUCH_XPT2046_MISO 13
// #define TOUCH_XPT2046_MOSI 11
// #define TOUCH_XPT2046_CS 38
// #define TOUCH_XPT2046_INT 18
// #define TOUCH_XPT2046_ROTATION 0
// #define TOUCH_MAP_X1 4000
// #define TOUCH_MAP_X2 100
// #define TOUCH_MAP_Y1 100
// #define TOUCH_MAP_Y2 4000

int touch_last_x = 0, touch_last_y = 0;

#if defined(TOUCH_FT6X36)
#include <Wire.h>
#include <FT6X36.h>
FT6X36 ts(&Wire, TOUCH_FT6X36_INT);
bool touch_touched_flag = true, touch_released_flag = true;

#elif defined(TOUCH_GT911)
#include <Wire.h>
#include <TAMC_GT911.h>

extern TAMC_GT911 *ts;
long ts_map_x1 = 800; // MAKERFAB_800x480
long ts_map_y1 = 480; // MAKERFAB_800x480

#elif defined(TOUCH_XPT2046)
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
XPT2046_Touchscreen ts(TOUCH_XPT2046_CS, TOUCH_XPT2046_INT);

#endif

#if defined(TOUCH_FT6X36)
void touch(TPoint p, TEvent e)
{
  if (e != TEvent::Tap && e != TEvent::DragStart && e != TEvent::DragMove && e != TEvent::DragEnd)
  {
    return;
  }
  // translation logic depends on screen rotation
#if defined(TOUCH_SWAP_XY)
  touch_last_x = map(p.y, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, gfx->width());
  touch_last_y = map(p.x, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, gfx->height());
#else
  touch_last_x = map(p.x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, gfx->width());
  touch_last_y = map(p.y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, gfx->height());
#endif
  switch (e)
  {
  case TEvent::Tap:
    Serial.println("Tap");
    touch_touched_flag = true;
    touch_released_flag = true;
    break;
  case TEvent::DragStart:
    Serial.println("DragStart");
    touch_touched_flag = true;
    break;
  case TEvent::DragMove:
    Serial.println("DragMove");
    touch_touched_flag = true;
    break;
  case TEvent::DragEnd:
    Serial.println("DragEnd");
    touch_released_flag = true;
    break;
  default:
    Serial.println("UNKNOWN");
    break;
  }
}
#endif

void touch_init()
{
#if defined(TOUCH_FT6X36)
  Wire.begin(TOUCH_FT6X36_SDA, TOUCH_FT6X36_SCL);
  ts.begin();
  ts.registerTouchHandler(touch);

#elif defined(TOUCH_GT911)
  uint8_t ts_scl = 18; // MAKERFAB_800x480
  uint8_t ts_sda = 17; // MAKERFAB_800x480


  if (HW_Platform == HWPlatform::Sunton_800x480 ) {
    // SUNTON_800x480
    ts_scl = 20;
    ts_sda = 19;
    ts_map_x1 = 480;
    ts_map_y1 = 272;
  }

  ts = new TAMC_GT911(ts_sda, ts_scl, TOUCH_GT911_INT, TOUCH_GT911_RST, max(ts_map_x1, static_cast<long>(TOUCH_MAP_X2)), max(ts_map_y1, static_cast<long>(TOUCH_MAP_Y2)));

  Wire.begin(ts_sda, ts_scl);
  ts->begin();
  ts->setRotation(TOUCH_GT911_ROTATION);

#elif defined(TOUCH_XPT2046)
  SPI.begin(TOUCH_XPT2046_SCK, TOUCH_XPT2046_MISO, TOUCH_XPT2046_MOSI, TOUCH_XPT2046_CS);
  ts.begin();
  ts.setRotation(TOUCH_XPT2046_ROTATION);

#endif
}

bool touch_has_signal()
{
#if defined(TOUCH_FT6X36)
  ts.loop();
  return touch_touched_flag || touch_released_flag;

#elif defined(TOUCH_GT911)
  return true;

#elif defined(TOUCH_XPT2046)
  return ts.tirqTouched();

#else
  return false;
#endif
}

bool touch_touched()
{
#if defined(TOUCH_FT6X36)
  if (touch_touched_flag)
  {
    touch_touched_flag = false;
    return true;
  }
  else
  {
    return false;
  }

#elif defined(TOUCH_GT911)
  ts->read();
  if (ts->isTouched)
  {
#if defined(TOUCH_SWAP_XY)
    touch_last_x = map(ts->points[0].y, ts_map_x1, TOUCH_MAP_X2, 0, screenWidth - 1);
    touch_last_y = map(ts->points[0].x, ts_map_y1, TOUCH_MAP_Y2, 0, screenHeight - 1);
#else
    touch_last_x = map(ts->points[0].x, ts_map_x1, TOUCH_MAP_X2, 0, screenWidth - 1);
    touch_last_y = map(ts->points[0].y, ts_map_y1, TOUCH_MAP_Y2, 0, screenHeight - 1);
#endif
    return true;
  }
  else
  {
    return false;
  }

#elif defined(TOUCH_XPT2046)
  if (ts.touched())
  {
    TS_Point p = ts.getPoint();
#if defined(TOUCH_SWAP_XY)
    touch_last_x = map(p.y, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, screenWidth - 1);
    touch_last_y = map(p.x, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, screenHeight - 1);
#else
    touch_last_x = map(p.x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, screenWidth - 1);
    touch_last_y = map(p.y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, screenHeight - 1);
#endif
    return true;
  }
  else
  {
    return false;
  }

#else
  return false;
#endif
}

bool touch_released()
{
#if defined(TOUCH_FT6X36)
  if (touch_released_flag)
  {
    touch_released_flag = false;
    return true;
  }
  else
  {
    return false;
  }

#elif defined(TOUCH_GT911)
  return true;

#elif defined(TOUCH_XPT2046)
  return true;

#else
  return false;
#endif
}