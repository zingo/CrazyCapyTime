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
lv_style_t styleIcon;
lv_style_t styleIconOff;
static lv_style_t styleBullet;
static lv_obj_t *labelRaceTime;

static const lv_font_t * font_normal = LV_FONT_DEFAULT;
static const lv_font_t * font_large = LV_FONT_DEFAULT;
static const lv_font_t * font_largest = LV_FONT_DEFAULT;

void createGUIRunnerTag(lv_obj_t * parent, uint32_t index)
{
  // index is index into iTag database
    lv_obj_t * panel1 = lv_obj_create(parent);
    lv_obj_set_size(panel1, LV_PCT(100),LV_SIZE_CONTENT);

    static lv_coord_t grid_1_col_dsc[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, 20, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_1_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_set_grid_cell(panel1, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_dsc_array(panel1, grid_1_col_dsc, grid_1_row_dsc);

    lv_obj_t * ledColor  = lv_led_create(panel1);
    lv_led_set_brightness(ledColor, 150);
    lv_led_set_color(ledColor, lv_palette_main(LV_PALETTE_GREY));
    lv_obj_set_grid_cell(ledColor, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * labelName = lv_label_create(panel1);
    lv_label_set_text(labelName, "");
    lv_obj_add_style(labelName, &styleTagText, 0);
    lv_label_set_long_mode(labelName, LV_LABEL_LONG_CLIP);
    lv_obj_set_grid_cell(labelName, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * lapsLabel = lv_label_create(panel1);
    lv_obj_add_style(lapsLabel, &styleTagText, 0);
    lv_label_set_text(lapsLabel, "Laps:");
    lv_obj_set_grid_cell(lapsLabel, LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * labelLaps = lv_label_create(panel1);
    lv_label_set_text(labelLaps, "");
    lv_obj_add_style(labelLaps, &styleTagText, 0);
    lv_obj_set_grid_cell(labelLaps, LV_GRID_ALIGN_START, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * labelConnectionStatus = lv_label_create(panel1);
    lv_obj_add_style(labelConnectionStatus, &styleIcon, 0);
    lv_label_set_text(labelConnectionStatus, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_grid_cell(labelConnectionStatus, LV_GRID_ALIGN_END, 4, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * labelBatterySymbol = lv_label_create(panel1);
    lv_obj_add_style(labelBatterySymbol, &styleIcon, 0);
    lv_label_set_text(labelBatterySymbol, LV_SYMBOL_BATTERY_EMPTY);
    lv_obj_set_grid_cell(labelBatterySymbol, LV_GRID_ALIGN_END, 5, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * labelBattery = lv_label_create(panel1);
    lv_label_set_text(labelBattery, "");
    lv_obj_set_grid_cell(labelBattery, LV_GRID_ALIGN_END, 6, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * log_out_btn = lv_btn_create(panel1);
    lv_obj_set_height(log_out_btn, LV_SIZE_CONTENT);
    lv_obj_t * label = lv_label_create(log_out_btn);
    lv_label_set_text(label, "Info");
    lv_obj_center(label);
    lv_obj_set_grid_cell(log_out_btn, LV_GRID_ALIGN_END, 7, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    iTags[index].saveGUIObjects(ledColor, labelName, labelLaps, labelConnectionStatus, labelBatterySymbol, labelBattery);
    iTags[index].updateGUI();
}

static void createGUITabRace(lv_obj_t * parent)
{
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);

  for(int j=0; j<ITAG_COUNT; j++)
  {
    createGUIRunnerTag(parent, j);
  }
}

void createGUI(void)
{
  lv_coord_t tab_h = 70;
  static lv_obj_t * tv;

#if LV_FONT_MONTSERRAT_36
  font_largest     = &lv_font_montserrat_36;
#else
  LV_LOG_WARN("LV_FONT_MONTSERRAT_36 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
#endif

#if LV_FONT_MONTSERRAT_24
  font_large     = &lv_font_montserrat_24;
#else
  LV_LOG_WARN("LV_FONT_MONTSERRAT_24 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
#endif
#if LV_FONT_MONTSERRAT_16
  font_normal    = &lv_font_montserrat_16;
#else
  LV_LOG_WARN("LV_FONT_MONTSERRAT_16 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
#endif

#if LV_USE_THEME_DEFAULT
  lv_theme_default_init(NULL, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), LV_THEME_DEFAULT_DARK,
                        font_normal);
#endif

  lv_style_init(&styleTextMuted);
  lv_style_set_text_opa(&styleTextMuted, LV_OPA_70);

  lv_style_init(&styleTitle);
  lv_style_set_text_font(&styleTitle, font_large);

  lv_style_init(&styleLargeText);
  lv_style_set_text_font(&styleLargeText, font_largest);

  lv_style_init(&styleTagText);
  lv_style_set_text_font(&styleTagText, font_largest);

  lv_style_init(&styleIcon);
  lv_style_set_text_color(&styleIcon, lv_theme_get_color_primary(NULL));
  lv_style_set_text_font(&styleIcon, font_largest);

  lv_style_init(&styleIconOff);
  lv_style_set_text_color(&styleIconOff, lv_theme_get_color_primary(NULL));
  lv_style_set_text_opa(&styleIconOff, LV_OPA_50);
  lv_style_set_text_font(&styleIconOff, font_largest);


  lv_style_init(&styleBullet);
  lv_style_set_border_width(&styleBullet, 0);
  lv_style_set_radius(&styleBullet, LV_RADIUS_CIRCLE);

  tv = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, tab_h);

  lv_obj_set_style_text_font(lv_scr_act(), font_normal, 0);


  lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tv);
  lv_obj_set_style_pad_left(tab_btns, LV_HOR_RES / 2, 0);

  lv_obj_t * logo = lv_img_create(tab_btns);
//  LV_IMG_DECLARE(img_lvgl_logo);
//  lv_img_set_src(logo, &img_lvgl_logo);
  lv_obj_align(logo, LV_ALIGN_LEFT_MID, -LV_HOR_RES / 2 + 25, 0);

  lv_obj_t * labelRaceName = lv_label_create(tab_btns);
  lv_label_set_text(labelRaceName, "Revolution Marathon");
  lv_obj_add_style(labelRaceName, &styleTitle, 0);
  lv_obj_align_to(labelRaceName, logo, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);

  lv_obj_t * label = lv_label_create(tab_btns);
  lv_label_set_text(label, "Crazy Capy Time");
  lv_obj_add_style(label, &styleTextMuted, 0);
  lv_obj_align_to(label, logo, LV_ALIGN_OUT_RIGHT_BOTTOM, 10, 0);

  labelRaceTime = lv_label_create(tab_btns);
  lv_label_set_text(labelRaceTime, "00:00:00");
  lv_obj_add_style(labelRaceTime, &styleLargeText, 0);
  lv_obj_align_to(labelRaceTime, labelRaceName, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);



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
  lv_label_set_text(labelRaceTime, rtc.getTime("%H:%M:%S").c_str());
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
  updateGUITime();
  lv_timer_handler();
}


