/*******************************************************************************
 * LVGL Widgets
 * This is a widgets demo for LVGL - Light and Versatile Graphics Library
 * import from: https://github.com/lvgl/lv_demos.git
 *
 * This was created from the project here 
 * https://www.makerfabs.com/sunton-esp32-s3-4-3-inch-ips-with-touch.html
 * 
 * Dependent libraries:
 * LVGL: https://github.com/lvgl/lvgl.git

 * Touch libraries:
 * FT6X36: https://github.com/strange-v/FT6X36.git
 * GT911: https://github.com/TAMCTec/gt911-arduino.git
 * XPT2046: https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
 *
 * LVGL Configuration file:
 * Copy your_arduino_path/libraries/lvgl/lv_conf_template.h
 * to your_arduino_path/libraries/lv_conf.h
 * Then find and set:
 * #define LV_COLOR_DEPTH     16
 * #define LV_TICK_CUSTOM     1
 *
 * For SPI display set color swap can be faster, parallel screen don't set!
 * #define LV_COLOR_16_SWAP   1
 *
 * Optional: Show CPU usage and FPS count
 * #define LV_USE_PERF_MONITOR 1
 ******************************************************************************/
#include "common.h"
#include <lvgl.h>
/*******************************************************************************
 ******************************************************************************/
#include <Arduino_GFX_Library.h>

#include "gui.h"
#include "iTag.h"

#define TAG "GFX"


#define TFT_BL 2
#define GFX_BL DF_GFX_BL // default backlight pin, you may replace DF_GFX_BL to actual backlight pin

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */
);

//  ST7262 IPS LCD 800x480
Arduino_RPi_DPI_RGBPanel *gfx = new Arduino_RPi_DPI_RGBPanel(
    bus,
    800 /* width */, 0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
    480 /* height */, 0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 14000000 /* prefer_speed */, true /* auto_flush */);

#include "touch.h"

// Setup screen resolution for LVGL
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

// GUI objects
static lv_style_t styleTextMuted;
static lv_style_t styleTitle;
static lv_style_t styleLargeText;
static lv_style_t styleTagText;
static lv_style_t styleTagSmallText;
static lv_style_t styleTime;
lv_style_t styleIcon;
lv_style_t styleIconOff;
static lv_style_t styleBullet;
static lv_obj_t *labelRaceTime;

static const lv_font_t * fontNormal = LV_FONT_DEFAULT;
static const lv_font_t * fontTag = LV_FONT_DEFAULT;
static const lv_font_t * fontLarge = LV_FONT_DEFAULT;
static const lv_font_t * fontLargest = LV_FONT_DEFAULT;
static const lv_font_t * fontTime = LV_FONT_DEFAULT;

static void btnTime_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
        if (!raceOngoing)
        {
          //lv_obj_t * label = lv_obj_get_child(btn, 0);
          //lv_label_set_text_fmt(label, "Race Starts soon");
          startRaceCountdown();
        }
    }

    // TODO add protection to not start race again if allready started maybe longpress
    // could be used to override race restart protection
    if(code == LV_EVENT_LONG_PRESSED) {
      if (raceOngoing) {
        raceStartIn = 0;
        raceOngoing = false;
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        lv_label_set_text_fmt(label, "Start!");
      }
      else {
        raceStartIn = 0;
        raceOngoing = true;
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        lv_label_set_text_fmt(label, "Race continued!");
      }
    }
}

static void btnSave_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
      saveRace();
    }
}

static void btnLoad_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
      loadRace();
    }
}

static void btnTagAdd_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    //lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
      iTag *tag = static_cast<iTag *>(lv_event_get_user_data(e));
      tag->participant.nextLap();
    }
}

static void btnTagSub_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    //lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
      iTag *tag = static_cast<iTag *>(lv_event_get_user_data(e));
      tag->participant.prevLap();
    }
}

void createGUIRunnerTag(lv_obj_t * parent, uint32_t index)
{
  // index is index into iTag database
  lv_obj_t * panel1 = lv_obj_create(parent);
  lv_obj_set_size(panel1, LV_PCT(100),LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(panel1, 13,0); 


  static lv_coord_t grid_1_col_dsc[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,LV_GRID_CONTENT, LV_GRID_CONTENT, 30, 40, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_1_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

  int x_pos = 0;

  lv_obj_set_grid_cell(panel1, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_dsc_array(panel1, grid_1_col_dsc, grid_1_row_dsc);

/*
static lv_style_t style_bullet;
    lv_style_init(&style_bullet);
    lv_style_set_border_width(&style_bullet, 0);
    lv_style_set_radius(&style_bullet, LV_RADIUS_CIRCLE);
    lv_obj_t * ledColor1 = lv_obj_create(panel1);
    lv_obj_set_size(ledColor1, 30, 30);
    lv_obj_remove_style(ledColor1, NULL, LV_PART_SCROLLBAR);
    lv_obj_add_style(ledColor1, &style_bullet, 0);
    lv_obj_set_style_bg_color(ledColor1, lv_palette_main(LV_PALETTE_RED), 0);
*/
  lv_obj_t * ledColor1 = lv_obj_create(panel1);
  lv_obj_set_size(ledColor1, 30, 30);
  lv_obj_remove_style(ledColor1, NULL, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(ledColor1, lv_palette_main(LV_PALETTE_PINK), 0);
  lv_obj_set_style_radius(ledColor1, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(ledColor1, 0,0);
  lv_obj_set_style_border_color(ledColor1, lv_color_hex(0x000000),0);
//  lv_obj_t * ledColor1  = lv_led_create(panel1);
//  lv_led_set_brightness(ledColor1, 255);
//  lv_led_set_color(ledColor1, lv_palette_main(LV_PALETTE_PINK));
//  lv_led_on(ledColor1);
  lv_obj_set_grid_cell(ledColor1, LV_GRID_ALIGN_CENTER, x_pos, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * ledColor0 = lv_obj_create(panel1);
  lv_obj_set_size(ledColor0, 20, 20);
  lv_obj_remove_style(ledColor0, NULL, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(ledColor0, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_style_radius(ledColor0, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(ledColor0, 1,0);
  lv_obj_set_style_border_color(ledColor0, lv_color_hex(0xd0d0d0),0);
//  lv_obj_t * ledColor0  = lv_led_create(panel1);
//  lv_led_set_brightness(ledColor0, 255);
//  lv_led_set_color(ledColor0, lv_palette_main(LV_PALETTE_GREY));
  lv_obj_set_grid_cell(ledColor0, LV_GRID_ALIGN_CENTER, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelName = lv_label_create(panel1);
  lv_label_set_text(labelName, "");
  lv_obj_add_style(labelName, &styleTagText, 0);
  lv_label_set_long_mode(labelName, LV_LABEL_LONG_CLIP);
  lv_obj_set_grid_cell(labelName, LV_GRID_ALIGN_START, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelDist = lv_label_create(panel1);
  lv_obj_add_style(labelDist, &styleTagText, 0);
  lv_label_set_text(labelDist, "00.000");
  lv_obj_set_grid_cell(labelDist, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelLaps = lv_label_create(panel1);
  lv_label_set_text(labelLaps, "Laps");
  lv_obj_add_style(labelLaps, &styleTagText, 0);
  lv_obj_set_grid_cell(labelLaps, LV_GRID_ALIGN_START, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelTime = lv_label_create(panel1);
  lv_obj_add_style(labelTime, &styleTagText, 0);
  lv_label_set_text(labelTime, "00:00:00");
  lv_obj_set_grid_cell(labelTime, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelConnectionStatus = lv_label_create(panel1);
  lv_obj_add_style(labelConnectionStatus, &styleIcon, 0);
  lv_label_set_text(labelConnectionStatus, LV_SYMBOL_BLUETOOTH);
  lv_obj_set_grid_cell(labelConnectionStatus, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);
/*
  lv_obj_t * labelBatterySymbol = lv_label_create(panel1);
  lv_obj_add_style(labelBatterySymbol, &styleIcon, 0);
  lv_label_set_text(labelBatterySymbol, LV_SYMBOL_BATTERY_EMPTY);
  lv_obj_set_grid_cell(labelBatterySymbol, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);
*/

  lv_obj_t * labelBattery = lv_label_create(panel1);
  lv_label_set_text(labelBattery, "");
  lv_obj_add_style(labelBattery, &styleTagSmallText, 0);
  lv_obj_set_grid_cell(labelBattery, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * btn;
  lv_obj_t *label;
  btn = lv_btn_create(panel1); 
  lv_obj_align_to(btn, parent, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
  lv_obj_add_event_cb(btn, btnTagSub_event_cb, LV_EVENT_ALL, &iTags[index]);

  label = lv_label_create(btn);          /*Add a label to the button*/
  lv_label_set_text(label, "-");                     /*Set the labels text*/
  lv_obj_center(label);
  lv_obj_add_style(label, &styleTagSmallText, 0);

  lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  btn = lv_btn_create(panel1); 
  lv_obj_align_to(btn, parent, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
  lv_obj_add_event_cb(btn, btnTagAdd_event_cb, LV_EVENT_ALL, &iTags[index]);

  label = lv_label_create(btn);          /*Add a label to the button*/
  lv_label_set_text(label, "+");                     /*Set the labels text*/
  lv_obj_center(label);
  lv_obj_add_style(label, &styleTagSmallText, 0);

  lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);


    //lv_obj_t * log_out_btn = lv_btn_create(panel1);
    //lv_obj_set_height(log_out_btn, LV_SIZE_CONTENT);
    //lv_obj_t * label = lv_label_create(log_out_btn);
    //lv_label_set_text(label, "Info");
    //lv_obj_center(label);
    //lv_obj_set_grid_cell(log_out_btn, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    iTags[index].saveGUIObjects(ledColor0, ledColor1, labelName, labelDist, labelLaps, labelTime, labelConnectionStatus, /* labelBatterySymbol,*/ labelBattery);
    iTags[index].updateGUI();
}

static void createGUITabRace(lv_obj_t * parent)
{
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_column(parent,5,0);
  lv_obj_set_style_pad_row(parent,5,0);
  lv_obj_set_style_pad_all(parent, 5,0); 


  for(int i=0; i<ITAG_COUNT; i++)
  {
    createGUIRunnerTag(parent, i);
  }

  lv_obj_t * chart;
  chart = lv_chart_create(parent);
  lv_obj_align_to(chart, parent, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_size(chart, LV_PCT(100), 400);
  lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER);
  
  const uint32_t ydiv_num = 8;
  const uint32_t xdiv_num = 9;
  lv_chart_set_div_line_count(chart, 9, 10);
//  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 20, 10, 10, 6, true, 25);
//  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 3, 8, 1, true, 20);

  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X, 0, 60*60*9);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 8);

  lv_chart_set_point_count(chart, (DRAW_MAX_LAPS_IN_CHART*2*ITAG_COUNT));


  for(int i=0; i<ITAG_COUNT; i++)
  {
    lv_chart_series_t * series = lv_chart_add_series(chart, lv_color_hex(iTags[i].color0), LV_CHART_AXIS_PRIMARY_Y);
    iTags[i].participant.saveGUIObjects(chart, series);
  }

  lv_obj_t * btnLoad = lv_btn_create(parent); 
  //lv_obj_align_to(btnLoad, parent, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(btnLoad, btnLoad_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *labelLoad = lv_label_create(btnLoad);          /*Add a label to the button*/
  lv_label_set_text(labelLoad, "Load");                     /*Set the labels text*/
  lv_obj_center(labelLoad);
  lv_obj_add_style(labelLoad, &styleTime, 0);

  lv_obj_t * btnSave = lv_btn_create(parent); 
  //lv_obj_align_to(btnSave, labelLoad, LV_ALIGN_TOP_RIGHT, 0, 0);
  //lv_obj_set_pos(btnSave, 10, 10);                            /*Set its position*/
  //lv_obj_set_size(btnSave, 120, 50);                          /*Set its size*/
  lv_obj_add_event_cb(btnSave, btnSave_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *labelSave = lv_label_create(btnSave);          /*Add a label to the button*/
  lv_label_set_text(labelSave, "Save");                     /*Set the labels text*/
  lv_obj_center(labelSave);
  lv_obj_add_style(labelSave, &styleTime, 0);

}

void createGUI(void)
{
  static lv_obj_t * tv;

  fontNormal  = &lv_font_montserrat_16;
  fontLarge   = &lv_font_montserrat_24;
  fontTag     = &lv_font_montserrat_28;
  fontLargest = &lv_font_montserrat_36;
  fontTime    = &lv_font_montserrat_42;

#if LV_USE_THEME_DEFAULT
  lv_theme_default_init(NULL, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), LV_THEME_DEFAULT_DARK, fontNormal);
#endif

  lv_style_init(&styleTextMuted);
  lv_style_set_text_opa(&styleTextMuted, LV_OPA_70);

  lv_style_init(&styleTitle);
  lv_style_set_text_font(&styleTitle, fontLarge);

  lv_style_init(&styleLargeText);
  lv_style_set_text_font(&styleLargeText, fontLargest);

  lv_style_init(&styleTagText);
  lv_style_set_text_font(&styleTagText, fontTag);

  lv_style_init(&styleTagSmallText);
  lv_style_set_text_font(&styleTagSmallText, fontNormal);

  lv_style_init(&styleTime);
  lv_style_set_text_font(&styleTime, fontTime);

  lv_style_init(&styleIcon);
  lv_style_set_text_color(&styleIcon, lv_theme_get_color_primary(NULL));
  lv_style_set_text_font(&styleIcon, fontLarge);

  lv_style_init(&styleIconOff);
  lv_style_set_text_color(&styleIconOff, lv_theme_get_color_primary(NULL));
  lv_style_set_text_opa(&styleIconOff, LV_OPA_50);
  lv_style_set_text_font(&styleIconOff, fontLarge);

  lv_style_init(&styleBullet);
  lv_style_set_border_width(&styleBullet, 0);
  lv_style_set_radius(&styleBullet, LV_RADIUS_CIRCLE);

  tv = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 70); //70 - hight of tab area

  lv_obj_set_style_text_font(lv_scr_act(), fontNormal, 0);

#define TAB_POS ((LV_HOR_RES / 4)*3)
  lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tv);
  lv_obj_set_style_pad_left(tab_btns, TAB_POS, 0);

  lv_obj_t * logo = lv_img_create(tab_btns);
//  LV_IMG_DECLARE(img_lvgl_logo);
//  lv_img_set_src(logo, &img_lvgl_logo);
  lv_obj_align(logo, LV_ALIGN_LEFT_MID, -TAB_POS + 25, 0);

  lv_obj_t * labelRaceName = lv_label_create(tab_btns);
  lv_label_set_text(labelRaceName, "Revolution Marathon");
  lv_obj_add_style(labelRaceName, &styleTitle, 0);
  lv_obj_align_to(labelRaceName, logo, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);

  lv_obj_t * label = lv_label_create(tab_btns);
  lv_label_set_text(label, "Crazy Capy Time");
  lv_obj_add_style(label, &styleTextMuted, 0);
  lv_obj_align_to(label, logo, LV_ALIGN_OUT_RIGHT_BOTTOM, 10, 0);

  lv_obj_t * btnTime = lv_btn_create(tab_btns); 
  lv_obj_align_to(btnTime, labelRaceName, LV_ALIGN_OUT_RIGHT_TOP, 30, -20);
  //lv_obj_set_pos(btnTime, 10, 10);                            /*Set its position*/
  //lv_obj_set_size(btnTime, 120, 50);                          /*Set its size*/
  lv_obj_add_event_cb(btnTime, btnTime_event_cb, LV_EVENT_ALL, NULL);

  labelRaceTime = lv_label_create(btnTime);          /*Add a label to the button*/
  lv_label_set_text(labelRaceTime, "Start!");                     /*Set the labels text*/
  lv_obj_center(labelRaceTime);
  lv_obj_add_style(labelRaceTime, &styleTime, 0);


/*
  labelRaceTime = lv_label_create(tab_btns);
  lv_label_set_text(labelRaceTime, "00:00:00");
  lv_obj_add_style(labelRaceTime, &styleTime, 0);
  lv_obj_align_to(labelRaceTime, labelRaceName, LV_ALIGN_OUT_RIGHT_TOP, 30, -20);
*/

  lv_obj_t * t1 = lv_tabview_add_tab(tv, "Race");
 
  createGUITabRace(t1);
}

// Display callback to flush the buffer to screen
void lvgl_displayFlushCallBack(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

// Touchpad callback to read the touchpad
void lvgl_touchPadReadCallback(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_REL;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}

void updateGUITime()
{
  if (raceOngoing) {
    lv_label_set_text(labelRaceTime, rtc.getTime("%H:%M:%S").c_str());
  }
  else if(raceStartIn) {
    lv_label_set_text_fmt(labelRaceTime, ">>  %d  <<", raceStartIn);
  }
}

void initLVGL()
{
  ESP_LOGI(TAG, "Setup GFX");
  gfx->begin();
  gfx->fillScreen(BLACK);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  lv_init();
  touch_init();
  screenWidth = gfx->width();
  screenHeight = gfx->height();
//#ifdef ESP32
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 4, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
//#else
//  disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 4);
//#endif
  if (!disp_draw_buf)
  {
    ESP_LOGE(TAG, "LVGL disp_draw_buf allocate failed!");
    return;
  }
  else
  {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 4);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = lvgl_displayFlushCallBack;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Initialize the (dummy) input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touchPadReadCallback;
    lv_indev_drv_register(&indev_drv);

    createGUI();

    ESP_LOGI(TAG, "Setup GFX done");
  }
}

void loopHandlLVGL()
{
  static unsigned long lastTimeUpdate = 356*24*60*60; //start on something we will never match like 1971
  unsigned long now = rtc.getEpoch();
  if (lastTimeUpdate != now) {
    lastTimeUpdate = now;
    updateGUITime();
  }
  lv_timer_handler();
}


