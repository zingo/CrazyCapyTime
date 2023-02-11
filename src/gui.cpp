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
#include <string>
#include "common.h"
#include <lvgl.h>
/*******************************************************************************
 ******************************************************************************/
#include <Arduino_GFX_Library.h>

#include "gui.h"
#include "messages.h"
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
static lv_style_t styleIcon;
static lv_style_t styleIconOff;
static lv_style_t styleBullet;
static lv_style_t style_iTag0;  //iTag circle
static lv_style_t style_iTag1;  //iTag rubber circle

static lv_obj_t *labelRaceTime;

static lv_obj_t *tabRace;
static lv_obj_t *tabParticipants;

static const lv_font_t * fontNormal = LV_FONT_DEFAULT;
static const lv_font_t * fontTag = LV_FONT_DEFAULT;
static const lv_font_t * fontLarge = LV_FONT_DEFAULT;
static const lv_font_t * fontLargest = LV_FONT_DEFAULT;
static const lv_font_t * fontTime = LV_FONT_DEFAULT;

class guiParticipant {
  public:
    uint32_t handleDB; // save handle to use in the RaceDB messages (supplied ti RaceDB)

    // ParticipantTab
    lv_obj_t * ledColor0;
    lv_obj_t * ledColor1;
    lv_obj_t * labelName;
    lv_obj_t * labelDist;
    lv_obj_t * labelLaps;
    lv_obj_t * labelTime;
    lv_obj_t * labelConnectionStatus;
    //lv_obj_t * labelBatterySymbol;
    lv_obj_t * labelBattery;

    // Graph
    lv_obj_t * lapsChart;
    lv_chart_series_t * lapsSeries;

    bool inRace;
    // RaceTab (only valid if inRace is true)
    lv_obj_t * ledRaceColor0;
    lv_obj_t * ledRaceColor1;
    lv_obj_t * labelRaceName;
    lv_obj_t * labelRaceDist;
    lv_obj_t * labelRaceLaps;
    lv_obj_t * labelRaceTime;
    lv_obj_t * labelRaceConnectionStatus;
};

static guiParticipant guiParticipants[ITAG_COUNT]; // TODO Could be dynamic
static uint32_t handleGFX = 0;  // We use the index into guiParticipants as a handle we will give to others like RaceDB


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
      msg_RaceDB msg;
      msg.SaveRace.header.msgType = MSG_ITAG_SAVE_RACE;
      //ESP_LOGI(TAG,"Send: MSG_ITAG_SAVE_RACE MSG:0x%x handleDB:0x%08x", msg.SaveRace.header.msgType);
      BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  
      // TODO log error xReturned show in GUI;
    }
}

static void btnLoad_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
      msg_RaceDB msg;
      msg.LoadRace.header.msgType = MSG_ITAG_LOAD_RACE;
      //ESP_LOGI(TAG,"Send: MSG_ITAG_LOAD_RACE MSG:0x%x handleDB:0x%08x", msg.LoadRace.header.msgType);
      BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  
      // TODO log error xReturned show in GUI;
    }
}

static void btnTagAdd_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    //lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
      uint32_t handleGFX = reinterpret_cast<uint32_t>(lv_event_get_user_data(e));
      //tag->participant.nextLap();
    }
}

static void btnTagSub_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    //lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
      uint32_t handleGFX = reinterpret_cast<uint32_t>(lv_event_get_user_data(e));
      //tag->participant.prevLap();
    }
}


static void btnTagAddToRace_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    //lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
      uint32_t handleGFX = reinterpret_cast<uint32_t>(lv_event_get_user_data(e));
      msg_RaceDB msg;
      msg.UpdateParticipantRaceStatus.header.msgType = MSG_ITAG_UPDATE_USER_RACE_STATUS;
      msg.UpdateParticipantRaceStatus.handleDB = guiParticipants[handleGFX].handleDB;
      msg.UpdateParticipantRaceStatus.handleGFX = handleGFX;
      msg.UpdateParticipantRaceStatus.inRace = !guiParticipants[handleGFX].inRace; //TOGGLE
      //ESP_LOGI(TAG,"Send: MSG_ITAG_UPDATE_USER_RACE_STATUS MSG:0x%x handleDB:0x%08x handleGFX:0x%08x inRace:%d -------------------------------------------", 
      //              msg.UpdateParticipantRaceStatus.header.msgType, msg.UpdateParticipantRaceStatus.handleDB, msg.UpdateParticipantRaceStatus.handleGFX, msg.UpdateParticipantRaceStatus.inRace);
      BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 200 ));  
      // it it fails let the user click again
      // TODO log error? xReturned;

    }
}



bool gfxAddUserToRace(lv_obj_t * parent, uint32_t handleGFX)
{

  lv_color_t color0 = lv_obj_get_style_bg_color(guiParticipants[handleGFX].ledColor0, LV_PART_MAIN);
  lv_color_t color1 = lv_obj_get_style_bg_color(guiParticipants[handleGFX].ledColor1, LV_PART_MAIN);
  std::string nameParticipant = std::string(lv_label_get_text(guiParticipants[handleGFX].labelName));

  lv_obj_t * panel1 = lv_obj_create(parent);
  lv_obj_set_size(panel1, LV_PCT(100),LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(panel1, 5,0); 

  static lv_coord_t grid_1_col_dsc[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,LV_GRID_CONTENT, LV_GRID_CONTENT, 30, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_1_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

  int x_pos = 0;

  lv_obj_set_grid_cell(panel1, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_dsc_array(panel1, grid_1_col_dsc, grid_1_row_dsc);

  lv_obj_t * ledColor1 = lv_obj_create(panel1);
  lv_obj_add_style(ledColor1, &style_iTag1, 0);
  lv_obj_remove_style(ledColor1, NULL, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(ledColor1, color1,0);
  lv_obj_set_grid_cell(ledColor1, LV_GRID_ALIGN_CENTER, x_pos, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * ledColor0 = lv_obj_create(panel1);
  lv_obj_add_style(ledColor0, &style_iTag0, 0);
  lv_obj_remove_style(ledColor0, NULL, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(ledColor0, color0,0);
  lv_obj_set_grid_cell(ledColor0, LV_GRID_ALIGN_CENTER, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelName = lv_label_create(panel1);
  lv_label_set_text(labelName, nameParticipant.c_str());
  lv_obj_add_style(labelName, &styleTagText, 0);
  lv_label_set_long_mode(labelName, LV_LABEL_LONG_CLIP);
  lv_obj_set_grid_cell(labelName, LV_GRID_ALIGN_START, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelDist = lv_label_create(panel1);
  lv_obj_add_style(labelDist, &styleTagText, 0);
//  lv_label_set_text(labelDist, "00.000");
  lv_label_set_text_fmt(labelDist, "   -.--- km");
  lv_obj_set_grid_cell(labelDist, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelLaps = lv_label_create(panel1);
  //lv_label_set_text(labelLaps, "Laps");
  lv_label_set_text_fmt(labelLaps, "(%2d/%2d)",0,RACE_LAPS);
  lv_obj_add_style(labelLaps, &styleTagText, 0);
  lv_obj_set_grid_cell(labelLaps, LV_GRID_ALIGN_START, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelTime = lv_label_create(panel1);
  lv_obj_add_style(labelTime, &styleTagText, 0);
//  lv_label_set_text(labelTime, "00:00:00");
  struct tm timeinfo;
  time_t tt = 0;//msgParticipant.lastlaptime;
  localtime_r(&tt, &timeinfo);
  lv_label_set_text_fmt(labelTime, "%3d:%02d:%02d", (timeinfo.tm_mday-1)*24+timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
  lv_obj_set_grid_cell(labelTime, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelConnectionStatus = lv_label_create(panel1);
  lv_obj_add_style(labelConnectionStatus, &styleIcon, 0);
  lv_label_set_text(labelConnectionStatus, LV_SYMBOL_BLUETOOTH);
  lv_obj_set_grid_cell(labelConnectionStatus, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // All well so far, lets update the internal struct with the info.
  guiParticipants[handleGFX].ledRaceColor0 = ledColor0;
  guiParticipants[handleGFX].ledRaceColor1 = ledColor1;
  guiParticipants[handleGFX].labelRaceName = labelName;
  guiParticipants[handleGFX].labelRaceDist = labelDist;
  guiParticipants[handleGFX].labelRaceLaps = labelLaps;
  guiParticipants[handleGFX].labelRaceTime = labelTime;
  guiParticipants[handleGFX].labelRaceConnectionStatus = labelConnectionStatus;
  guiParticipants[handleGFX].inRace = true;

  return true;  //TODO check errors
}

bool gfxAddUserToParticipants(lv_obj_t * parent, msg_AddParticipant &msgParticipant, uint32_t handleGFX)
{

  lv_obj_t * btn;
  lv_obj_t *label;
  lv_obj_t * panel1 = lv_obj_create(parent);
  lv_obj_set_size(panel1, LV_PCT(100),LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(panel1, 6,0);

  static lv_coord_t grid_1_col_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,LV_GRID_CONTENT, LV_GRID_CONTENT, 30, 40, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_1_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

  int x_pos = 0;

  lv_obj_set_grid_cell(panel1, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_dsc_array(panel1, grid_1_col_dsc, grid_1_row_dsc);

  {
    btn = lv_btn_create(panel1); 
    lv_obj_align_to(btn, parent, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
    lv_obj_add_event_cb(btn, btnTagAddToRace_event_cb, LV_EVENT_ALL, reinterpret_cast<void *>(handleGFX));

    label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_UPLOAD );
    lv_obj_center(label);
    lv_obj_add_style(label, &styleTagSmallText, 0);

    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  }

  lv_obj_t * ledColor1 = lv_obj_create(panel1);
  lv_obj_add_style(ledColor1, &style_iTag1, 0);
  lv_obj_remove_style(ledColor1, NULL, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(ledColor1, lv_color_hex(msgParticipant.color1),0);
  lv_obj_set_grid_cell(ledColor1, LV_GRID_ALIGN_CENTER, x_pos, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * ledColor0 = lv_obj_create(panel1);
  lv_obj_add_style(ledColor0, &style_iTag0, 0);
  lv_obj_remove_style(ledColor0, NULL, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(ledColor0, lv_color_hex(msgParticipant.color0),0);
  lv_obj_set_grid_cell(ledColor0, LV_GRID_ALIGN_CENTER, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelName = lv_label_create(panel1);
  //std::string nameParticipant = std::string(participant.name);
  lv_label_set_text(labelName, msgParticipant.name);
  lv_obj_add_style(labelName, &styleTagText, 0);
  lv_label_set_long_mode(labelName, LV_LABEL_LONG_CLIP);
  lv_obj_set_grid_cell(labelName, LV_GRID_ALIGN_START, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelDist = lv_label_create(panel1);
  lv_obj_add_style(labelDist, &styleTagText, 0);
  lv_label_set_text_fmt(labelDist, "   -.--- km");
  lv_obj_set_grid_cell(labelDist, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelLaps = lv_label_create(panel1);
  lv_label_set_text_fmt(labelLaps, "(%2d/%2d)",0,RACE_LAPS);
  lv_obj_add_style(labelLaps, &styleTagText, 0);
  lv_obj_set_grid_cell(labelLaps, LV_GRID_ALIGN_START, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_t * labelTime = lv_label_create(panel1);
  lv_obj_add_style(labelTime, &styleTagText, 0);
  struct tm timeinfo;
  time_t tt = 0;//msgParticipant.lastlaptime;
  localtime_r(&tt, &timeinfo);
  lv_label_set_text_fmt(labelTime, "%3d:%02d:%02d", (timeinfo.tm_mday-1)*24+timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
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


  btn = lv_btn_create(panel1); 
  lv_obj_align_to(btn, parent, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
  lv_obj_add_event_cb(btn, btnTagSub_event_cb, LV_EVENT_ALL, reinterpret_cast<void *>(handleGFX));

  label = lv_label_create(btn);
  lv_label_set_text(label, "-");
  lv_obj_center(label);
  lv_obj_add_style(label, &styleTagSmallText, 0);

  lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  btn = lv_btn_create(panel1); 
  lv_obj_align_to(btn, parent, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
  lv_obj_add_event_cb(btn, btnTagAdd_event_cb, LV_EVENT_ALL, reinterpret_cast<void *>(handleGFX));

  label = lv_label_create(btn);
  lv_label_set_text(label, "+");
  lv_obj_center(label);
  lv_obj_add_style(label, &styleTagSmallText, 0);

  lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // All well so far, lets update the internal struct with the info.
  guiParticipants[handleGFX].handleDB = msgParticipant.handleDB;
  guiParticipants[handleGFX].ledColor0 = ledColor0;
  guiParticipants[handleGFX].ledColor1 = ledColor1;
  guiParticipants[handleGFX].labelName = labelName;
  guiParticipants[handleGFX].labelDist = labelDist;
  guiParticipants[handleGFX].labelLaps = labelLaps;
  guiParticipants[handleGFX].labelTime = labelTime;
  guiParticipants[handleGFX].labelConnectionStatus = labelConnectionStatus;
  guiParticipants[handleGFX].labelBattery = labelBattery;

  if (msgParticipant.inRace) {
    gfxAddUserToRace(tabRace, handleGFX);
  }

  return true;
}




void gfxUpdateParticipant(msg_UpdateParticipant msg)
{
  uint32_t index = msg.handleGFX;
  lv_label_set_text_fmt(guiParticipants[index].labelDist, "%4.3fkm",msg.distance/1000.0);
  lv_label_set_text_fmt(guiParticipants[index].labelLaps, "(%2d/%2d)",msg.laps,RACE_LAPS);
  struct tm timeinfo;
  time_t tt = msg.lastlaptime;
  localtime_r(&tt, &timeinfo);
  lv_label_set_text_fmt(guiParticipants[index].labelTime, "%3d:%02d:%02d", (timeinfo.tm_mday-1)*24+timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);

  std::string conn;
  if (msg.connectionStatus == 1)
  {
    conn = std::string(LV_SYMBOL_EYE_CLOSE);
  }
  else if (msg.connectionStatus == 0)
  {
    conn = std::string("");

  } else {
    conn = std::string(LV_SYMBOL_EYE_OPEN);
  }
  
  lv_label_set_text(guiParticipants[index].labelConnectionStatus, conn.c_str());

  if (guiParticipants[index].inRace) {
    lv_label_set_text_fmt(guiParticipants[index].labelRaceDist, "%4.3fkm",msg.distance/1000.0);
    lv_label_set_text_fmt(guiParticipants[index].labelRaceLaps, "(%2d/%2d)",msg.laps,RACE_LAPS),  (msg.laps*RACE_DISTANCE_LAP)/1000.0;
    lv_label_set_text_fmt(guiParticipants[index].labelRaceTime, "%3d:%02d:%02d", (timeinfo.tm_mday-1)*24+timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    lv_label_set_text(guiParticipants[index].labelRaceConnectionStatus, conn.c_str());
  }
}

void gfxUpdateParticipantStatus(msg_UpdateParticipantStatus msg)
{
  uint32_t index = msg.handleGFX;

  if(msg.inRace)
  {
    //make sure participant is on race page
    if (!guiParticipants[index].inRace)
    {
      gfxAddUserToRace(tabRace, index);
    }
  }
  else {
    //make sure participant is NOT on race page
    if (!guiParticipants[index].inRace)
    {
      // TODO not supported yet
      //gfxRemoveUserFromRace(tabRace, handleGFX);
    }
  }

  std::string conn;
  if (msg.connectionStatus == 1)
  {
    conn = std::string(LV_SYMBOL_EYE_CLOSE);
  }
  else if (msg.connectionStatus == 0)
  {
    conn = std::string("");

  } else {
    conn = std::string(LV_SYMBOL_EYE_OPEN);
  }
  
  lv_label_set_text(guiParticipants[index].labelConnectionStatus, conn.c_str());
  if(msg.battery >= 0 && msg.battery <=100) {
    lv_label_set_text_fmt(guiParticipants[index].labelBattery, "%3d%%",msg.battery);
  }

  if (guiParticipants[index].inRace) {
    lv_label_set_text(guiParticipants[index].labelRaceConnectionStatus, conn.c_str());
  }
}

// return used handleGFX or negative value in case of error
uint32_t gfxAddUserToGUI(msg_AddParticipant &msgParticipant)
{
  uint32_t ret_handle = handleGFX;

  if(handleGFX >= ITAG_COUNT) {
    ESP_LOGE(TAG," ERROR to may user added handleGFX:%d >= ITAG_COUNT:%d for msg_AddParticipant MSG:0x%x handleDB:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d  ---> Do nothing",handleGFX, ITAG_COUNT,
               msgParticipant.header.msgType, msgParticipant.handleDB, msgParticipant.color0, msgParticipant.color1, msgParticipant.name, msgParticipant.inRace);
    return UINT32_MAX; // indicates error
  }
//  else {
//    ESP_LOGI(TAG," handleGFX:%d < ITAG_COUNT:%d for msg_AddParticipant MSG:0x%x handleDB:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d  ---> Add",handleGFX, ITAG_COUNT,
//               msgParticipant.header.msgType, msgParticipant.handleDB, msgParticipant.color0, msgParticipant.color1, msgParticipant.name, msgParticipant.inRace);
//  }
  gfxAddUserToParticipants(tabParticipants, msgParticipant, handleGFX);

  handleGFX++;
  if(handleGFX >= ITAG_COUNT) {
    ESP_LOGI(TAG,"Added all Users handleGFX:%d from MSG:0x%x handleDB:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d --------- COME ON LETS PARTY!!!!!!!!!!!!!!!!!!", handleGFX,
               msgParticipant.header.msgType, msgParticipant.handleDB, msgParticipant.color0, msgParticipant.color1, msgParticipant.name, msgParticipant.inRace);

  }

  // All Ok return used handleGFX
  return ret_handle;
}

static void createGUITabRace(lv_obj_t * parent)
{
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_column(parent,0,0);
  lv_obj_set_style_pad_row(parent,0,0);
  lv_obj_set_style_pad_all(parent, 5,0);
}

static void createGUITabParticipant(lv_obj_t * parent)
{
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_column(parent, 2,0);
  lv_obj_set_style_pad_row(parent, 2,0);
  lv_obj_set_style_pad_all(parent, 2,0);
}

static void createGUITabRaceExtra(lv_obj_t * parent)
{
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_column(parent,5,0);
  lv_obj_set_style_pad_row(parent,5,0);
  lv_obj_set_style_pad_all(parent, 5,0);

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


  //TODO for(int i=0; i<ITAG_COUNT; i++)
  //{
  //  lv_chart_series_t * series = lv_chart_add_series(chart, lv_color_hex(iTags[i].color0), LV_CHART_AXIS_PRIMARY_Y);
  //  iTags[i].participant.saveGUIObjects(chart, series);
  //}

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

  lv_style_init(&style_iTag0);
  lv_style_set_radius(&style_iTag0, LV_RADIUS_CIRCLE);
  lv_style_set_size(&style_iTag0,20);
  lv_style_set_bg_color(&style_iTag0,lv_palette_main(LV_PALETTE_PINK));
  lv_style_set_border_width(&style_iTag0, 1);
  lv_style_set_border_color(&style_iTag0, lv_color_hex(0xd0d0d0));
  //lv_style_remove_prop(&style_iTag0, LV_PART_SCROLLBAR); // TODO Figure out if this could be done from style

  lv_style_init(&style_iTag1);
  lv_style_set_radius(&style_iTag1, LV_RADIUS_CIRCLE);
  lv_style_set_size(&style_iTag1,30);
  lv_style_set_bg_color(&style_iTag1,lv_palette_main(LV_PALETTE_PINK));
  lv_style_set_border_width(&style_iTag1, 1);
  lv_style_set_border_color(&style_iTag1, lv_color_hex(0xb0b0b0));
  //lv_style_remove_prop(&style_iTag1, LV_PART_SCROLLBAR); // TODO Figure out if this could be done from style

#define TAB_POS (((LV_HOR_RES / 3)*2)-50)
#define TAB_HIGHT 70
#define TAB_TIME_WIDTH 200

  tv = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, TAB_HIGHT); // height of tab area

  lv_obj_set_style_text_font(lv_scr_act(), fontNormal, 0);

  lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tv);
  lv_obj_set_style_pad_left(tab_btns, TAB_POS, 0);

//  lv_obj_t * logo = lv_img_create(tab_btns);
//  LV_IMG_DECLARE(img_lvgl_logo);
//  lv_img_set_src(logo, &img_lvgl_logo);
//  lv_obj_align(logo, LV_ALIGN_LEFT_MID, -TAB_POS + 25, 0);

  lv_obj_t * labelRaceName = lv_label_create(tab_btns);
  lv_label_set_text(labelRaceName, "Revolution Marathon");
  lv_obj_add_style(labelRaceName, &styleTitle, 0);
//  lv_obj_align_to(labelRaceName, logo, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);
  lv_obj_align(labelRaceName, LV_ALIGN_OUT_LEFT_TOP, -TAB_POS + 10, 10);

  lv_obj_t * label = lv_label_create(tab_btns);
  lv_label_set_text(label, "Crazy Capy Time");
  lv_obj_add_style(label, &styleTextMuted, 0);
//  lv_obj_align_to(label, logo, LV_ALIGN_OUT_RIGHT_BOTTOM, 10, 0);
  lv_obj_align(label, LV_ALIGN_OUT_LEFT_TOP, -TAB_POS + 10, 35);
//  lv_obj_align_to(label, labelRaceName, LV_ALIGN_OUT_LEFT_BOTTOM, 0, 20);

  lv_obj_t * btnTime = lv_btn_create(tab_btns); 
  lv_obj_align_to(btnTime, labelRaceName, LV_ALIGN_OUT_RIGHT_TOP, 10, -10);
  //lv_obj_align(btnTime, LV_ALIGN_CENTER, -30, -((TAB_HIGHT-10)/2));
  //lv_obj_set_pos(btnTime, 10, 10);                            /*Set its position*/
  //lv_obj_set_size(btnTime, 120, 50);                          /*Set its size*/
  lv_obj_set_width(btnTime,TAB_TIME_WIDTH);
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

  tabRace = lv_tabview_add_tab(tv, "Race"); 
  lv_obj_t * t3 = lv_tabview_add_tab(tv, LV_SYMBOL_IMAGE );
  tabParticipants = lv_tabview_add_tab(tv, LV_SYMBOL_LIST );

  createGUITabRace(tabRace);
  createGUITabRaceExtra(t3);
  createGUITabParticipant(tabParticipants);
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
//    disp_drv.rotated = LV_DISP_ROT_90;
//    disp_drv.sw_rotate = 1;
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

  msg_GFX msg;
  while ( xQueueReceive(queueGFX, &(msg), (TickType_t)0) == pdPASS)  // Don't block main loop just take a peek
  {
    switch(msg.header.msgType) {
      case MSG_GFX_UPDATE_USER:
      {
        //ESP_LOGI(TAG,"Recived MSG_GFX_UPDATE_USER: MSG:0x%x handleGFX:0x%08x distance:%d laps:%d lastlaptime:%d,connectionStatus:%d",
        //        msg.Update.header.msgType, msg.Update.handleGFX, msg.Update.distance, msg.Update.laps, msg.Update.lastlaptime, msg.Update.connectionStatus);
        gfxUpdateParticipant(msg.Update);
        // Done! No response on this msg
        break;
      }
      case MSG_GFX_UPDATE_STATUS_USER:
      {
        //ESP_LOGI(TAG,"Recived MSG_GFX_UPDATE_STATUS_USER: MSG:0x%x handleGFX:0x%08x connectionStatus:%d battery:%d inRace:%d",
        //              msg.UpdateStatus.header.msgType, msg.UpdateStatus.handleGFX, msg.UpdateStatus.connectionStatus, msg.UpdateStatus.battery, msg.UpdateStatus.inRace);
        gfxUpdateParticipantStatus(msg.UpdateStatus);
        break;
      }
      case MSG_GFX_ADD_USER_TO_RACE:
      {
        //  ESP_LOGI(TAG,"Received: MSG_GFX_ADD_USER_TO_RACE MSG:0x%x handleDB:0x%08x color:(0x%x,0x%x) Name:%s inRace:%d", 
        //               msg.Add.header.msgType, msg.Add.handleDB, msg.Add.color0, msg.Add.color1, msg.Add.name, msg.Add.inRace);
        uint32_t handle = gfxAddUserToGUI(msg.Add);

        msg_RaceDB msgResponse;
        msgResponse.AddedToGFX.header.msgType = MSG_ITAG_GFX_ADD_USER_RESPONSE;
        msgResponse.AddedToGFX.handleDB = msg.Add.handleDB;
        msgResponse.AddedToGFX.handleGFX = handle;
        if (handle != UINT32_MAX) {
          msgResponse.AddedToGFX.wasOK = true;
        }
        else {
          msgResponse.AddedToGFX.wasOK = false;
        }
        // ESP_LOGI(TAG,"Send: MSG_ITAG_GFX_ADD_USER_RESPONSE MSG:0x%x handleDB:0x%08x handleGFX:0x%08x wasOK:%d", 
        //              msgResponse.AddedToGFX.header.msgType, msgResponse.AddedToGFX.handleDB, msgResponse.AddedToGFX.handleGFX, msgResponse.AddedToGFX.wasOK);
        BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msgResponse, (TickType_t)pdMS_TO_TICKS( 2000 ));
        // TODO handle error? xReturned;

        break;
      }
      default:
        ESP_LOGE(TAG,"ERROR received bad msg: 0x%x",msg.header.msgType);
        //break;
    }
  }

  static unsigned long lastTimeUpdate = 356*24*60*60; //start on something we will never match like 1971
  unsigned long now = rtc.getEpoch();
  if (lastTimeUpdate != now) {
    lastTimeUpdate = now;
    updateGUITime();
  }
  lv_timer_handler();
}


