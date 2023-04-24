/*******************************************************************************
 * LVGL Widgets
 *
 * This was created from the project here 
 * https://www.makerfabs.com/sunton-esp32-s3-4-3-inch-ips-with-touch.html
 * and some HW setup values from here
 * https://github.com/Makerfabs/ESP32-S3-Parallel-TFT-with-Touch-4.3inch
 * 
 * Dependent libraries:
 * LVGL: https://github.com/lvgl/lvgl.git

 * Touch libraries:
 * FT6X36: https://github.com/strange-v/FT6X36.git
 * GT911: https://github.com/TAMCTec/gt911-arduino.git
 * XPT2046: https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
 *
 * LVGL Configuration file:
 * include/lv_conf.h
 *
 * Optional: Show CPU usage and FPS count
 * #define LV_USE_PERF_MONITOR 1
 ******************************************************************************/
#include <string>
#include <cstdlib>

#include "common.h"
#include <lvgl.h>

#include <Arduino_GFX_Library.h>

#include "gui.h"
#include "messages.h"
#include "iTag.h"

#define TAG "GFX"

#define TFT_BL 2
#define GFX_BL DF_GFX_BL // default backlight pin, you may replace DF_GFX_BL to actual backlight pin

static Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */
);

//  ST7262 IPS LCD 800x480
static Arduino_RPi_DPI_RGBPanel *gfx;

// Include touch.h after gfx is declered, it is used inside that file
#include "touch.h"
#if defined(TOUCH_GT911)
TAMC_GT911 *ts; //= TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));
#endif

// Setup screen resolution for LVGL
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf = nullptr;
static lv_disp_drv_t disp_drv;

static const lv_font_t *fontNormal = &lv_font_montserrat_16;
static const lv_font_t *fontTag = &lv_font_montserrat_28;
static const lv_font_t *fontLarge = &lv_font_montserrat_24;
static const lv_font_t *fontLargest = &lv_font_montserrat_36;
static const lv_font_t *fontTime = &lv_font_montserrat_42;

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

static lv_obj_t *tabView = nullptr;       //Screen
static lv_obj_t *globalLabelRaceName = nullptr;
static lv_obj_t *globalLabelRaceTag = nullptr;
static lv_obj_t *globalLabelRaceTime = nullptr;

static lv_obj_t *tabRace = nullptr;
static lv_obj_t *tabParticipants = nullptr;
static lv_obj_t *chartLaps = nullptr;
//static lv_obj_t *chartRSSI = nullptr;

static lv_obj_t* keyboard = nullptr;

class guiParticipant {
  public:
    uint32_t handleDB; // save handle to use in the RaceDB messages (supplied ti RaceDB)
    uint32_t laps;     // sevaed so we can detect new laps and draw them in the graph
    uint32_t thisLapStart;
    // ParticipantTab
    lv_obj_t * labelToRace;
    lv_obj_t * ledColor0;
    lv_obj_t * ledColor1;
    lv_obj_t * textAreaName;
    lv_obj_t * labelDist;
    lv_obj_t * labelLaps;
    lv_obj_t * labelTime;
    lv_obj_t * labelConnectionStatus;
    lv_obj_t * labelBattery;

    // Graph
    lv_chart_series_t * seriesLaps = nullptr;;
    //lv_chart_series_t * seriesRSSI = nullptr;;

    bool inRace;
    // RaceTab (only valid if inRace is true)
    lv_obj_t * objRace;        // Delete this to remove from GUI (set the rest *Race* to nullptr below)
    lv_obj_t * ledRaceColor0;
    lv_obj_t * ledRaceColor1;
    lv_obj_t * labelRaceName;
    lv_obj_t * labelRaceDist;
    lv_obj_t * labelRaceLaps;
    lv_obj_t * labelRaceTime;
    lv_obj_t * labelRaceConnectionStatus;
};

class guiRace {
  public:
    guiRace() : dataValid(false) , raceStart(0), distance(0), laps(1),
                textAreaConfigRaceFileName(nullptr),
                textAreaConfigRaceName(nullptr),
                textAreaConfigRaceTimebased(nullptr),
                textAreaConfigRaceMaxTime(nullptr),
                textAreaConfigRaceDistance(nullptr),
                textAreaConfigRaceLaps(nullptr),
                textAreaConfigRaceLapsDistances(nullptr),
                textAreaConfigRaceBlockNewLapTime(nullptr),
                textAreaConfigRaceUpdateCloserTime(nullptr),
                textAreaConfigRaceRaceStartIn(nullptr),
                tabGraph(nullptr)
                {}

    void receiveConfigRace(msg_RaceConfig *raceConfig);
    void sendConfigRace();
    void createGUITabConfig(lv_obj_t * parent);
    void updateGUITabRaceGraph();
    time_t getRaceStart() { return raceStart;}
    void setRaceStart(time_t inRaceStart) { raceStart = inRaceStart;}
    uint32_t getDistance() { return distance;}
    void setDistance(uint32_t inDistance) { distance = inDistance;}
    uint32_t getLaps() { return laps;}
    void setLaps(uint32_t inLaps) { laps = inLaps;}
    void setBlockNewLapTime(time_t newTime) {lv_textarea_set_text(textAreaConfigRaceBlockNewLapTime,std::to_string(newTime).c_str());}
    bool isTextAreaDistance(lv_obj_t *ta) {return ta==textAreaConfigRaceDistance;}
    bool isTextAreaLaps(lv_obj_t *ta) {return ta==textAreaConfigRaceLaps;}
    bool isCheckBoxTimeBased(lv_obj_t *cb) {return cb==textAreaConfigRaceTimebased;}
    bool isTimeBasedRace() {return lv_obj_get_state(textAreaConfigRaceTimebased) & LV_STATE_CHECKED;}
    time_t getMaxTime();
    bool isDataValid() {return dataValid;}
    void setTabGraph(lv_obj_t *inTabGraph) {tabGraph=inTabGraph; createGUITabRaceGraph();}
  private:
    bool dataValid;
    time_t raceStart;
    uint32_t distance;
    uint32_t laps;
    lv_obj_t * textAreaConfigRaceFileName = nullptr;
    lv_obj_t * textAreaConfigRaceName = nullptr;
    lv_obj_t * textAreaConfigRaceTimebased = nullptr;
    lv_obj_t * textAreaConfigRaceMaxTime = nullptr;
    lv_obj_t * textAreaConfigRaceDistance = nullptr;
    lv_obj_t * textAreaConfigRaceLaps = nullptr;
    lv_obj_t * textAreaConfigRaceLapsDistances = nullptr;
    lv_obj_t * textAreaConfigRaceBlockNewLapTime = nullptr;
    lv_obj_t * textAreaConfigRaceUpdateCloserTime = nullptr;
    lv_obj_t * textAreaConfigRaceRaceStartIn = nullptr;
    lv_obj_t * tabGraph = nullptr;

    void createGUITabRaceGraph();
    //void createGUITabRSSI(lv_obj_t * parent);
};

static guiRace guiRace;
static guiParticipant guiParticipants[ITAG_COUNT]; // TODO Could be dynamic
static uint32_t globalHandleGFX = 0;  // We use the index into guiParticipants as a handle we will give to others like RaceDB

static void gfxClearAllParticipantData();

static void btnTime_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
        if (!raceOngoing)
        {
          //lv_obj_t * label = lv_obj_get_child(btn, 0);
          //lv_label_set_text_fmt(label, "Race Starts soon");
          startRaceCountdown();  //TODO should be signal
          lv_obj_scroll_to_view_recursive(tabRace, LV_ANIM_ON);
          gfxClearAllParticipantData(); // TODO should probably be triggreded from RaceDB when it is cleared
        }
    }

    // TODO add protection to not start race again if already started maybe longpress
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
        lv_obj_scroll_to_view_recursive(tabRace, LV_ANIM_ON);
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
      msg_RaceDB msg;
      msg.LoadRace.header.msgType = MSG_ITAG_LOAD_RACE;
      //ESP_LOGI(TAG,"Send: MSG_ITAG_LOAD_RACE MSG:0x%x handleDB:0x%08x", msg.LoadRace.header.msgType);
      BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  
      // TODO log error xReturned show in GUI;
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
      //ESP_LOGI(TAG,"Send: MSG_ITAG_UPDATE_USER_RACE_STATUS MSG:0x%x handleDB:0x%08x handleGFX:0x%08x inRace:%d", 
      //              msg.UpdateParticipantRaceStatus.header.msgType, msg.UpdateParticipantRaceStatus.handleDB, msg.UpdateParticipantRaceStatus.handleGFX, msg.UpdateParticipantRaceStatus.inRace);
      BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 1000 ));  
      // it it fails let the user click again
      // TODO log error? xReturned;
    }
}

static void taEdit_event_cb(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  uint32_t handleGFX = reinterpret_cast<uint32_t>(lv_event_get_user_data(e));
 // lv_obj_t *kb = reinterpret_cast<lv_obj_t *>(lv_event_get_user_data(e));
  if(code == LV_EVENT_FOCUSED) {
    if(lv_indev_get_type(lv_indev_get_act()) != LV_INDEV_TYPE_KEYPAD) {
      lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
      lv_keyboard_set_textarea(keyboard, ta);
      lv_obj_set_style_max_height(keyboard, LV_HOR_RES * 2 / 3, 0);
      lv_obj_update_layout(tabView);   /*Be sure the sizes are recalculated*/
      lv_obj_set_height(tabView, LV_VER_RES - lv_obj_get_height(keyboard));
      lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
      lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON);
    }
  }
  else if(code == LV_EVENT_DEFOCUSED) {
    lv_keyboard_set_textarea(keyboard, NULL);
    lv_obj_set_height(tabView, LV_VER_RES);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_indev_reset(NULL, ta);
  }
  else if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_set_height(tabView, LV_VER_RES);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(ta, LV_STATE_FOCUSED);
    lv_indev_reset(NULL, ta);   /*To forget the last clicked object to make it focusable again*/
  }

  // Name updated -> send update signal to RaceDB, and update signal will be send back
  // that will resync name in all tabs.
  if(code == LV_EVENT_READY || code == LV_EVENT_DEFOCUSED || code == LV_EVENT_CANCEL)
  {
    uint32_t color0 = lv_color_to32(lv_obj_get_style_bg_color(guiParticipants[handleGFX].ledColor0, LV_PART_MAIN));
    uint32_t color1 = lv_color_to32(lv_obj_get_style_bg_color(guiParticipants[handleGFX].ledColor1, LV_PART_MAIN));
    std::string nameParticipant = std::string(lv_textarea_get_text(guiParticipants[handleGFX].textAreaName));
    bool inRace = guiParticipants[handleGFX].inRace;

    msg_RaceDB msg;
    msg.UpdateParticipant.header.msgType = MSG_ITAG_UPDATE_USER;
    msg.UpdateParticipant.handleDB = guiParticipants[handleGFX].handleDB;
    msg.UpdateParticipant.handleGFX = handleGFX;
    msg.UpdateParticipant.color0 = color0;
    msg.UpdateParticipant.color1 = color1;
    size_t len = nameParticipant.copy(msg.UpdateParticipant.name, PARTICIPANT_NAME_LENGTH);
    msg.UpdateParticipant.name[len] = '\0';
    msg.UpdateParticipant.inRace = inRace;

    //ESP_LOGI(TAG,"Send: MSG_ITAG_UPDATE_USER MSG:0x%x handleDB:0x%08x handleGFX:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d",
    //             msg.UpdateParticipant.header.msgType, msg.UpdateParticipant.handleDB, msg.UpdateParticipant.handleGFX, msg.UpdateParticipant.color0, msg.UpdateParticipant.color1, msg.UpdateParticipant.name, msg.UpdateParticipant.inRace);

    BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 )); // TODO add resend ?
    if (!xReturned) {
      // it it fails let the user click again
      ESP_LOGW(TAG,"WARNING: Send: MSG_ITAG_UPDATE_USER MSG:0x%x handleDB:0x%08x handleGFX:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d could not be sent in 2000ms. USER need to retry",
                  msg.UpdateParticipant.header.msgType, msg.UpdateParticipant.handleDB, msg.UpdateParticipant.handleGFX, msg.UpdateParticipant.color0, msg.UpdateParticipant.color1, msg.UpdateParticipant.name, msg.UpdateParticipant.inRace);
    }
  }
}

static void btnTagAdd_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    //lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
      uint32_t handleGFX = reinterpret_cast<uint32_t>(lv_event_get_user_data(e));
      msg_RaceDB msg;
      msg.UpdateParticipantLapCount.header.msgType = MSG_ITAG_UPDATE_USER_LAP_COUNT;
      msg.UpdateParticipantLapCount.handleDB = guiParticipants[handleGFX].handleDB;
      msg.UpdateParticipantLapCount.handleGFX = handleGFX;
      msg.UpdateParticipantLapCount.lapDiff = 1;
      //ESP_LOGI(TAG,"Send: MSG_ITAG_UPDATE_USER_RACE_STATUS MSG:0x%x handleDB:0x%08x handleGFX:0x%08x lapDiff:%d", 
      //              msg.UpdateParticipantLapCount.header.msgType, msg.UpdateParticipantLapCount.handleDB, msg.UpdateParticipantLapCount.handleGFX, msg.UpdateParticipantLapCount.lapDiff);
      BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 1000 ));  
      if (!xReturned) {
        // it it fails let the user click again
        ESP_LOGW(TAG,"WARNING: Send: MSG_ITAG_UPDATE_USER_RACE_STATUS MSG:0x%x handleDB:0x%08x handleGFX:0x%08x lapDiff:%d could not be sent in 1000ms. USER need to retry", 
                      msg.UpdateParticipantLapCount.header.msgType, msg.UpdateParticipantLapCount.handleDB, msg.UpdateParticipantLapCount.handleGFX, msg.UpdateParticipantLapCount.lapDiff);
      }
    }
}

static void btnTagSub_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    //lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_SHORT_CLICKED) {
      uint32_t handleGFX = reinterpret_cast<uint32_t>(lv_event_get_user_data(e));
      msg_RaceDB msg;
      msg.UpdateParticipantLapCount.header.msgType = MSG_ITAG_UPDATE_USER_LAP_COUNT;
      msg.UpdateParticipantLapCount.handleDB = guiParticipants[handleGFX].handleDB;
      msg.UpdateParticipantLapCount.handleGFX = handleGFX;
      msg.UpdateParticipantLapCount.lapDiff = -1;
      //ESP_LOGI(TAG,"Send: MSG_ITAG_UPDATE_USER_RACE_STATUS MSG:0x%x handleDB:0x%08x handleGFX:0x%08x lapDiff:%d", 
      //              msg.UpdateParticipantLapCount.header.msgType, msg.UpdateParticipantLapCount.handleDB, msg.UpdateParticipantLapCount.handleGFX, msg.UpdateParticipantLapCount.lapDiff);
      BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 1000 ));  
      if (!xReturned) {
        // it it fails let the user click again
        ESP_LOGW(TAG,"WARNING: Send: MSG_ITAG_UPDATE_USER_RACE_STATUS MSG:0x%x handleDB:0x%08x handleGFX:0x%08x lapDiff:%d could not be sent in 1000ms. USER need to retry", 
                      msg.UpdateParticipantLapCount.header.msgType, msg.UpdateParticipantLapCount.handleDB, msg.UpdateParticipantLapCount.handleGFX, msg.UpdateParticipantLapCount.lapDiff);
      }
    }
}

#ifdef TESTCODE

void startTestEndToEnd(std::string testname); //TODO move to header
void stopTestEndToEnd(); //TODO move to header

static const char * btnTest_map[] = {"Test24HFast", "Test24HLive", "\n",
                                     "StopTest", "StopTest", "StopTest", ""
                                };

static void btnTest_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        uint32_t id = lv_btnmatrix_get_selected_btn(obj);
        const char * txt = lv_btnmatrix_get_btn_text(obj, id);

        ESP_LOGI(TAG,"TEST: %s was pressed\n", txt);
        std::string stopTesting("StopTesting");
        std::string selected = txt;

        if (selected != stopTesting) {
          startTestEndToEnd(selected);
        }
        else {
          stopTestEndToEnd();
        }


    }
}
#endif



static void gfxRemoveUserFromRace(uint32_t handleGFX)
{
  if (!guiParticipants[handleGFX].inRace) {
    // Should already be remove but lets check for errors
    if (guiParticipants[handleGFX].objRace ||
        guiParticipants[handleGFX].ledRaceColor0 ||
        guiParticipants[handleGFX].ledRaceColor1 ||
        guiParticipants[handleGFX].labelRaceName ||
        guiParticipants[handleGFX].labelRaceDist ||
        guiParticipants[handleGFX].labelRaceLaps ||
        guiParticipants[handleGFX].labelRaceTime ||
        guiParticipants[handleGFX].labelRaceConnectionStatus) {
        ESP_LOGE(TAG,"ERROR calling gfxRemoveUserFromRace() on a participant that should not be in the list AND all fields are not cleared as they should DO NOTHING");
        }
  }
  // Remove it all
  guiParticipants[handleGFX].inRace = false;
  lv_obj_t * panel1 = guiParticipants[handleGFX].objRace;
  guiParticipants[handleGFX].objRace = nullptr;
  guiParticipants[handleGFX].ledRaceColor0 = nullptr;
  guiParticipants[handleGFX].ledRaceColor1 = nullptr;
  guiParticipants[handleGFX].labelRaceName = nullptr;
  guiParticipants[handleGFX].labelRaceDist = nullptr;
  guiParticipants[handleGFX].labelRaceLaps = nullptr;
  guiParticipants[handleGFX].labelRaceTime = nullptr;
  guiParticipants[handleGFX].labelRaceConnectionStatus = nullptr;
  if (panel1) {
    lv_obj_del(panel1);
  }
  lv_label_set_text(guiParticipants[handleGFX].labelToRace, LV_SYMBOL_UPLOAD );
}

static void gfxAddUserToRace(lv_obj_t * parent, uint32_t handleGFX)
{
  if (!guiParticipants[handleGFX].inRace) {
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
    //lv_obj_set_style_bg_color(ledColor1, color1,0);
    lv_obj_set_grid_cell(ledColor1, LV_GRID_ALIGN_CENTER, x_pos, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * ledColor0 = lv_obj_create(panel1);
    lv_obj_add_style(ledColor0, &style_iTag0, 0);
    lv_obj_remove_style(ledColor0, NULL, LV_PART_SCROLLBAR);
    //lv_obj_set_style_bg_color(ledColor0, color0,0);
    lv_obj_set_grid_cell(ledColor0, LV_GRID_ALIGN_CENTER, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * labelName = lv_label_create(panel1);
    //lv_label_set_text(labelName, nameParticipant.c_str());
    lv_obj_add_style(labelName, &styleTagText, 0);
    lv_label_set_long_mode(labelName, LV_LABEL_LONG_CLIP);
    lv_obj_set_grid_cell(labelName, LV_GRID_ALIGN_START, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * labelDist = lv_label_create(panel1);
    lv_obj_add_style(labelDist, &styleTagText, 0);
    //lv_label_set_text_fmt(labelDist, "   -.--- km");
    lv_obj_set_grid_cell(labelDist, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * labelLaps = lv_label_create(panel1);
    //lv_label_set_text_fmt(labelLaps, "(%2d/%2d)",0,guiRace.getLaps());
    lv_obj_add_style(labelLaps, &styleTagText, 0);
    lv_obj_set_grid_cell(labelLaps, LV_GRID_ALIGN_START, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * labelTime = lv_label_create(panel1);
    lv_obj_add_style(labelTime, &styleTagText, 0);
    //struct tm timeinfo;
    //time_t tt = 0;//msgParticipant.lastlaptime;
    //localtime_r(&tt, &timeinfo);
    //lv_label_set_text_fmt(labelTime, "%3d:%02d:%02d", (timeinfo.tm_mday-1)*24+timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    lv_obj_set_grid_cell(labelTime, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * labelConnectionStatus = lv_label_create(panel1);
    lv_obj_add_style(labelConnectionStatus, &styleIcon, 0);
    lv_label_set_text(labelConnectionStatus, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_grid_cell(labelConnectionStatus, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    // All well so far, lets update the internal struct with the info.
    guiParticipants[handleGFX].objRace = panel1;
    guiParticipants[handleGFX].ledRaceColor0 = ledColor0;
    guiParticipants[handleGFX].ledRaceColor1 = ledColor1;
    guiParticipants[handleGFX].labelRaceName = labelName;
    guiParticipants[handleGFX].labelRaceDist = labelDist;
    guiParticipants[handleGFX].labelRaceLaps = labelLaps;
    guiParticipants[handleGFX].labelRaceTime = labelTime;
    guiParticipants[handleGFX].labelRaceConnectionStatus = labelConnectionStatus;
    guiParticipants[handleGFX].inRace = true;
  }
  else {
    // Already added make sure all fiealds are valid
    if (!(guiParticipants[handleGFX].objRace &&
        guiParticipants[handleGFX].ledRaceColor0 &&
        guiParticipants[handleGFX].ledRaceColor1 &&
        guiParticipants[handleGFX].labelRaceName &&
        guiParticipants[handleGFX].labelRaceDist &&
        guiParticipants[handleGFX].labelRaceLaps &&
        guiParticipants[handleGFX].labelRaceTime &&
        guiParticipants[handleGFX].labelRaceConnectionStatus)) {
        ESP_LOGE(TAG,"ERROR calling gfxAddUserToRace() again on a already added object AND all fields are not set as they should, will try to remove and re-add");
        gfxRemoveUserFromRace(handleGFX);
        gfxAddUserToRace(parent, handleGFX);
        //the recursive call to gfxAddUserToRace() above have handled all the rest of the stuff so return here
        return;
      }
  }

  lv_label_set_text(guiParticipants[handleGFX].labelToRace, LV_SYMBOL_OK );

  // Copy content from Particapand GUI
  lv_color_t color0 = lv_obj_get_style_bg_color(guiParticipants[handleGFX].ledColor0, LV_PART_MAIN);
  lv_color_t color1 = lv_obj_get_style_bg_color(guiParticipants[handleGFX].ledColor1, LV_PART_MAIN);
  std::string nameParticipant = std::string(lv_textarea_get_text(guiParticipants[handleGFX].textAreaName));
  std::string RaceDist = std::string(lv_label_get_text(guiParticipants[handleGFX].labelDist));
  std::string RaceLaps = std::string(lv_label_get_text(guiParticipants[handleGFX].labelLaps));
  std::string RaceTime = std::string(lv_label_get_text(guiParticipants[handleGFX].labelTime));
  std::string RaceConnectionStatus = std::string(lv_label_get_text(guiParticipants[handleGFX].labelConnectionStatus));

  // Update fields
  lv_obj_set_style_bg_color(guiParticipants[handleGFX].ledRaceColor1, color1,0);
  lv_obj_set_style_bg_color(guiParticipants[handleGFX].ledRaceColor0, color0,0);
  lv_label_set_text(guiParticipants[handleGFX].labelRaceName, nameParticipant.c_str());
  lv_label_set_text(guiParticipants[handleGFX].labelRaceDist, RaceDist.c_str());
  lv_label_set_text(guiParticipants[handleGFX].labelRaceLaps, RaceLaps.c_str());
  lv_label_set_text(guiParticipants[handleGFX].labelRaceTime, RaceTime.c_str());
  lv_label_set_text(guiParticipants[handleGFX].labelRaceConnectionStatus, RaceConnectionStatus.c_str());
}

static void gfxUpdateInRace(bool newInRace, uint32_t handleGFX)
{
  if(newInRace)
  {
    if (!guiParticipants[handleGFX].inRace) {
      //make sure participant is on race page
      gfxAddUserToRace(tabRace, handleGFX);  // and updated fields
    }
  }
  else {
    if (guiParticipants[handleGFX].inRace) {
      //make sure participant is NOT on race page
      gfxRemoveUserFromRace(handleGFX);
    }
  }
}

static bool gfxAddUserToParticipants(lv_obj_t * parent, msg_AddParticipant &msgParticipant, uint32_t handleGFX)
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

  // ------ Add/Remove from Race Page
  btn = lv_btn_create(panel1); 
  lv_obj_align_to(btn, parent, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
  lv_obj_add_event_cb(btn, btnTagAddToRace_event_cb, LV_EVENT_ALL, reinterpret_cast<void *>(handleGFX));

  lv_obj_t * labelToRace = lv_label_create(btn);
  lv_label_set_text(labelToRace, LV_SYMBOL_UPLOAD );
  lv_obj_center(labelToRace);
  lv_obj_add_style(labelToRace, &styleTagSmallText, 0);

  lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // ------ Tag
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

  // ------ Name
  lv_obj_t * textAreaName = lv_textarea_create(panel1);
  lv_obj_add_style(textAreaName, &styleTagSmallText, 0);
  lv_textarea_set_text(textAreaName, msgParticipant.name);
  lv_textarea_set_one_line(textAreaName, true);
  lv_textarea_set_password_mode(textAreaName, false);
  lv_obj_add_event_cb(textAreaName, taEdit_event_cb, LV_EVENT_ALL, reinterpret_cast<void *>(handleGFX));
  lv_obj_set_grid_cell(textAreaName, LV_GRID_ALIGN_START, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // ------ Dist 
  lv_obj_t * labelDist = lv_label_create(panel1);
  lv_obj_add_style(labelDist, &styleTagText, 0);
  lv_label_set_text_fmt(labelDist, "   -.--- km");
  lv_obj_set_grid_cell(labelDist, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // ------ Laps
  lv_obj_t * labelLaps = lv_label_create(panel1);
  lv_label_set_text_fmt(labelLaps, "(%2d/%2d)",0,guiRace.getLaps());
  lv_obj_add_style(labelLaps, &styleTagText, 0);
  lv_obj_set_grid_cell(labelLaps, LV_GRID_ALIGN_START, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // ------ Last time
  lv_obj_t * labelTime = lv_label_create(panel1);
  lv_obj_add_style(labelTime, &styleTagText, 0);
  struct tm timeinfo;
  time_t tt = 0;//msgParticipant.lastlaptime;
  localtime_r(&tt, &timeinfo);
  lv_label_set_text_fmt(labelTime, "%3d:%02d:%02d", (timeinfo.tm_mday-1)*24+timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
  lv_obj_set_grid_cell(labelTime, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // ------ BT Connection
  lv_obj_t * labelConnectionStatus = lv_label_create(panel1);
  lv_obj_add_style(labelConnectionStatus, &styleIcon, 0);
  lv_label_set_text(labelConnectionStatus, LV_SYMBOL_BLUETOOTH);
  lv_obj_set_grid_cell(labelConnectionStatus, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // ------ BT Battery
  lv_obj_t * labelBattery = lv_label_create(panel1);
  lv_label_set_text(labelBattery, "");
  lv_obj_add_style(labelBattery, &styleTagSmallText, 0);
  lv_obj_set_grid_cell(labelBattery, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // ------ Add/Subb lap (In case of error)
  btn = lv_btn_create(panel1); 
  lv_obj_align_to(btn, parent, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
  lv_obj_add_event_cb(btn, btnTagSub_event_cb, LV_EVENT_ALL, reinterpret_cast<void *>(handleGFX));

  label = lv_label_create(btn);
  lv_label_set_text(label, LV_SYMBOL_MINUS );
  lv_obj_center(label);
  lv_obj_add_style(label, &styleTagSmallText, 0);

  lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  // ------
  btn = lv_btn_create(panel1); 
  lv_obj_align_to(btn, parent, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
  lv_obj_add_event_cb(btn, btnTagAdd_event_cb, LV_EVENT_ALL, reinterpret_cast<void *>(handleGFX));

  label = lv_label_create(btn);
  lv_label_set_text(label, LV_SYMBOL_PLUS);
  lv_obj_center(label);
  lv_obj_add_style(label, &styleTagSmallText, 0);

  lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_END, x_pos++, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_chart_series_t * seriesLaps = lv_chart_add_series(chartLaps, lv_color_hex(msgParticipant.color0), LV_CHART_AXIS_PRIMARY_Y);
  //lv_chart_series_t * seriesRSSI = lv_chart_add_series(chartRSSI, lv_color_hex(msgParticipant.color0), LV_CHART_AXIS_PRIMARY_Y);

  // All well so far, lets update the internal struct with the info.
  guiParticipants[handleGFX].handleDB = msgParticipant.handleDB;
  guiParticipants[handleGFX].laps = 0;
  guiParticipants[handleGFX].thisLapStart = 0;
  guiParticipants[handleGFX].seriesLaps = seriesLaps;
  //guiParticipants[handleGFX].seriesRSSI = seriesRSSI;
  guiParticipants[handleGFX].labelToRace = labelToRace;
  guiParticipants[handleGFX].labelToRace = labelToRace;
  guiParticipants[handleGFX].ledColor0 = ledColor0;
  guiParticipants[handleGFX].ledColor1 = ledColor1;
  guiParticipants[handleGFX].textAreaName = textAreaName;
  guiParticipants[handleGFX].labelDist = labelDist;
  guiParticipants[handleGFX].labelLaps = labelLaps;
  guiParticipants[handleGFX].labelTime = labelTime;
  guiParticipants[handleGFX].labelConnectionStatus = labelConnectionStatus;
  guiParticipants[handleGFX].labelBattery = labelBattery;
  guiParticipants[handleGFX].inRace = false;
  guiParticipants[handleGFX].objRace = nullptr;
  guiParticipants[handleGFX].ledRaceColor0 = nullptr;
  guiParticipants[handleGFX].ledRaceColor1 = nullptr;
  guiParticipants[handleGFX].labelRaceName = nullptr;
  guiParticipants[handleGFX].labelRaceDist = nullptr;
  guiParticipants[handleGFX].labelRaceLaps = nullptr;
  guiParticipants[handleGFX].labelRaceTime = nullptr;
  guiParticipants[handleGFX].labelRaceConnectionStatus = nullptr;
  return true;
}

void guiRace::createGUITabRaceGraph()
{
  ESP_LOGI(TAG,"createGUITabRaceGraph()");

  lv_obj_t * parent = tabGraph;
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_column(parent,5,0);
  lv_obj_set_style_pad_row(parent,5,0);
  lv_obj_set_style_pad_all(parent, 5,0);

  ESP_LOGI(TAG,"createGUITabRaceGraph() Create graph!");
  chartLaps = lv_chart_create(parent);
  //lv_obj_align_to(chartLaps, parent, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_center(chartLaps);
  lv_obj_set_size(chartLaps, LV_PCT(90), LV_PCT(85));
  lv_chart_set_type(chartLaps, LV_CHART_TYPE_SCATTER);
  // Do not display points on the data
  lv_obj_set_style_size(chartLaps, 0, LV_PART_INDICATOR);

  if(isDataValid()) {
    updateGUITabRaceGraph();
  }
}

void guiRace::updateGUITabRaceGraph()
{
  ESP_LOGI(TAG,"createGUITabRaceGraph()");
  if(!isDataValid()) {
    ESP_LOGE(TAG,"createGUITabRaceGraph() called before all data is valid, do nothing");
    return;
  }

  bool timeBasedRace = isTimeBasedRace();
  time_t maxTime = getMaxTime();
  uint32_t laps = getLaps();
  uint32_t raceDist = getDistance();

  if (!timeBasedRace) {
    maxTime +=1; // add some space for slow runners to still be visiable
    raceDist += (raceDist/laps)*2; // add some laps if someone get feeling for one extra
    laps += 2; // add some laps if someone get feeling for one extra
  }
  else {
    laps=300; //TODO use 999, 300 should be enough for VXO24h for now so lets keep it safe and test more before going bananas
  }

  if (!timeBasedRace) {
    lv_chart_set_div_line_count(chartLaps, laps+1, maxTime+1);  //reversed order Y-Horizontal first
    lv_chart_set_axis_tick(chartLaps, LV_CHART_AXIS_PRIMARY_X, 20, 10, maxTime+1, 4, true, 50);
    lv_chart_set_axis_tick(chartLaps, LV_CHART_AXIS_PRIMARY_Y, 5, 3, laps+1, 1, true, 50);
    lv_chart_set_range(chartLaps, LV_CHART_AXIS_PRIMARY_X, 0, maxTime*60*60);
    lv_chart_set_range(chartLaps, LV_CHART_AXIS_PRIMARY_Y, 0, raceDist);
    //lv_chart_set_range(chartLaps, LV_CHART_AXIS_SECONDARY_Y, 0, raceDist);
    ESP_LOGI(TAG,"createGUITabRaceGraph() x:[0,%d] y:[0,%d]",maxTime*60*60,raceDist);
  }
  else {
    lv_chart_set_div_line_count(chartLaps, maxTime+1, maxTime+1); //reversed order Y-Horizontal first
    lv_chart_set_axis_tick(chartLaps, LV_CHART_AXIS_PRIMARY_X, 20, 10, (maxTime/2)+1, 2, false, 25);
    lv_chart_set_axis_tick(chartLaps, LV_CHART_AXIS_SECONDARY_Y, 20, 10, (maxTime/4)+1, 4, true, 150);
    lv_chart_set_range(chartLaps, LV_CHART_AXIS_PRIMARY_X, 0, maxTime*60*60);
    lv_chart_set_range(chartLaps, LV_CHART_AXIS_PRIMARY_Y, 0, maxTime*10*1000);
    //lv_chart_set_range(chartLaps, LV_CHART_AXIS_SECONDARY_X, 0, maxTime*60*60);
    lv_chart_set_range(chartLaps, LV_CHART_AXIS_SECONDARY_Y, 0, maxTime);
    ESP_LOGI(TAG,"createGUITabRaceGraph() x:[0,%d] y:[0,%d]",maxTime*60*60,maxTime*10*1000);
  }

  if (!timeBasedRace) {
    // Save 2 points per lap, "Arrave" and "Leave", with long few laps you take a short refill break
    lv_chart_set_point_count(chartLaps, (laps*2*ITAG_COUNT));
  }
  else {
    // Save 1 point per lap, "Arrave" with sort laps you usually just pass by this make it use a lot less data in the graph
    lv_chart_set_point_count(chartLaps, (laps*ITAG_COUNT));
  }
}

// Update Graphs 
// TODO handle subtraction of lap

static void gfxUpdateParticipantChartNewLap(uint32_t handleGFX, uint32_t lap, time_t time, uint32_t dist)
{
  if (!guiRace.isTimeBasedRace()) {
    ESP_LOGI(TAG,"gfxUpdateParticipantChartNewLap(handleGFX:%d, lap:%d, time:%d, dist: %d)",handleGFX,lap,time,dist);
    guiParticipants[handleGFX].seriesLaps->x_points[2*lap] = time;
    guiParticipants[handleGFX].seriesLaps->y_points[2*lap] = dist;
    guiParticipants[handleGFX].seriesLaps->x_points[2*lap+1] = time;
    guiParticipants[handleGFX].seriesLaps->y_points[2*lap+1] = dist;
  }
  else {
    //time=time/100;
    //dist=dist/100;
    ESP_LOGI(TAG,"gfxUpdateParticipantChartNewLap(handleGFX:%d, lap:%d, time:%d, dist: %d) (timebased only save arrive)",handleGFX,lap,time,dist);
    guiParticipants[handleGFX].seriesLaps->x_points[lap] = time;
    guiParticipants[handleGFX].seriesLaps->y_points[lap] = dist;
  }
  lv_chart_refresh(chartLaps); //Required after direct set
}

static void gfxUpdateParticipantChartLastSeen(uint32_t handleGFX, uint32_t lap, time_t time, uint32_t dist)
{
  if (!guiRace.isTimeBasedRace()) {
    ESP_LOGI(TAG,"gfxUpdateParticipantChartLastSeen(handleGFX:%d, lap:%d, time:%d, dist: %d)",handleGFX,lap,time,dist);
    guiParticipants[handleGFX].seriesLaps->x_points[2*lap+1] = time;
    guiParticipants[handleGFX].seriesLaps->y_points[2*lap+1] = dist;
    lv_chart_refresh(chartLaps); //Required after direct set
  }
  else {
    //time=time/100;
    //dist=dist/100;
    ESP_LOGI(TAG,"gfxUpdateParticipantChartLastSeen(handleGFX:%d, lap:%d, time:%d, dist: %d) (timebased do nothing)",handleGFX,lap,time,dist);
    //TODO or do nothing here ????
    //guiParticipants[handleGFX].seriesLaps->x_points[lap] = time;
    //guiParticipants[handleGFX].seriesLaps->y_points[lap] = dist;
    //lv_chart_refresh(chartLaps); //Required after direct set
  }
}

/*
static void gfxUpdateParticipantChartRSSI(uint32_t handleGFX, int8_t RSSI)
{
  if (guiParticipants[handleGFX].seriesRSSI == nullptr) {
    return;
  }
  uint8_t plotValue = 0;
  if (RSSI >= -100 && RSSI < 0) {
    plotValue = 100 - std::abs(RSSI); 
  }
  //ESP_LOGI(TAG,"gfxUpdateParticipantChartRSSI(handleGFX:%d, RSSI:%d) -> Plot: %d",handleGFX,RSSI,plotValue);
  lv_chart_set_next_value(chartRSSI, guiParticipants[handleGFX].seriesRSSI, plotValue);
}
*/

static void gfxClearAllParticipantData()
{
//  ESP_LOGI(TAG,"gfxClearAllParticipantData()");
  for(uint32_t handleGFX = 0; handleGFX < ITAG_COUNT; handleGFX++)
  {
    guiParticipants[handleGFX].laps = 0;
    if(guiParticipants[handleGFX].seriesLaps) {
      lv_chart_set_all_value(chartLaps, guiParticipants[handleGFX].seriesLaps, LV_CHART_POINT_NONE);
    }
    //Keep this ??  lv_chart_set_all_value(chartRSSI, guiParticipants[handleGFX].seriesRSSI, LV_CHART_POINT_NONE)
  }
}

static void gfxClearParticipantData(uint32_t handleGFX, uint32_t fromLap)
{
  ESP_LOGI(TAG,"gfxClearParticipantData(handleGFX:%d,fromLap:%d)",handleGFX,fromLap);

  for(uint32_t lap = fromLap; lap <= DRAW_MAX_LAPS_IN_CHART; lap++)
  {
    if (!guiRace.isTimeBasedRace()) {
      guiParticipants[handleGFX].seriesLaps->x_points[2*lap] = LV_CHART_POINT_NONE;
      guiParticipants[handleGFX].seriesLaps->y_points[2*lap] = LV_CHART_POINT_NONE;
      guiParticipants[handleGFX].seriesLaps->x_points[2*lap+1] = LV_CHART_POINT_NONE;
      guiParticipants[handleGFX].seriesLaps->y_points[2*lap+1] = LV_CHART_POINT_NONE;
    }
    else {
      guiParticipants[handleGFX].seriesLaps->x_points[lap] = LV_CHART_POINT_NONE;
      guiParticipants[handleGFX].seriesLaps->y_points[lap] = LV_CHART_POINT_NONE;
    }
  }
  lv_chart_refresh(chartLaps); //Required after direct set
}

// Update Race info (Laps/Dist)
static void gfxUpdateParticipantData(msg_UpdateParticipantData msg)
{
  uint32_t handleGFX = msg.handleGFX;

    gfxUpdateInRace(msg.inRace, handleGFX);

    //ESP_LOGI(TAG,"guiParticipants[handleGFX].laps:%d msg.laps:%d",guiParticipants[handleGFX].laps,msg.laps);

    if (msg.laps > guiParticipants[handleGFX].laps) {
      // new lap
      gfxUpdateParticipantChartNewLap(handleGFX, msg.laps, msg.lastLapTime, msg.distance);
    }
    else if (msg.laps < guiParticipants[handleGFX].laps) {
      // lap deleted
      gfxClearParticipantData(handleGFX, msg.laps); 
    }
    else if ( msg.lastLapTime != guiParticipants[handleGFX].thisLapStart) {
      // lap updated
      gfxUpdateParticipantChartNewLap(handleGFX, msg.laps, msg.lastLapTime, msg.distance);
    }
    
    guiParticipants[handleGFX].laps = msg.laps;
    guiParticipants[handleGFX].thisLapStart = msg.lastLapTime;

    lv_label_set_text_fmt(guiParticipants[handleGFX].labelDist, "%4.3fkm",msg.distance/1000.0);
    if (guiParticipants[handleGFX].inRace) {
      lv_label_set_text_fmt(guiParticipants[handleGFX].labelRaceDist, "%4.3fkm",msg.distance/1000.0);
    }

    if (!guiRace.isTimeBasedRace()) {
      lv_label_set_text_fmt(guiParticipants[handleGFX].labelLaps, "(%2d/%2d)",msg.laps,guiRace.getLaps());
      if (guiParticipants[handleGFX].inRace) {
        lv_label_set_text_fmt(guiParticipants[handleGFX].labelRaceLaps, "(%2d/%2d)",msg.laps,guiRace.getLaps());
      }
    }
    else {
      lv_label_set_text_fmt(guiParticipants[handleGFX].labelLaps, "(%2d)",msg.laps);
      if (guiParticipants[handleGFX].inRace) {
        lv_label_set_text_fmt(guiParticipants[handleGFX].labelRaceLaps, "(%2d)",msg.laps);
      }
    }
    struct tm timeinfo;
    time_t tt = msg.lastLapTime;
    localtime_r(&tt, &timeinfo);
    lv_label_set_text_fmt(guiParticipants[handleGFX].labelTime, "%3d:%02d:%02d", (timeinfo.tm_mday-1)*24+timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    if (guiParticipants[handleGFX].inRace) {
      lv_label_set_text_fmt(guiParticipants[handleGFX].labelRaceTime, "%3d:%02d:%02d", (timeinfo.tm_mday-1)*24+timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
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
      // if msg.connectionStatus < 0 (as it should) it is the RSSI value of the tag
      gfxUpdateParticipantChartLastSeen(handleGFX, msg.laps, msg.lastSeenTime, msg.distance);
      conn = std::string(LV_SYMBOL_EYE_OPEN);
      // TODO plot RSSI??
    }

    lv_label_set_text(guiParticipants[handleGFX].labelConnectionStatus, conn.c_str());
    if (guiParticipants[handleGFX].inRace) {
      lv_label_set_text(guiParticipants[handleGFX].labelRaceConnectionStatus, conn.c_str());
    }
    //gfxUpdateParticipantChartRSSI(handleGFX,msg.connectionStatus);
}

static void gfxUpdateParticipantStatus(msg_UpdateParticipantStatus msg)
{
  uint32_t handleGFX = msg.handleGFX;

  gfxUpdateInRace(msg.inRace, handleGFX);

  std::string conn;
  if (msg.connectionStatus == 1)
  {
    conn = std::string(LV_SYMBOL_EYE_CLOSE);
  }
  else if (msg.connectionStatus == 0)
  {
    conn = std::string("");

  } else {
    // if msg.connectionStatus < 0 (as it should) it is the RSSI value of the tag
    conn = std::string(LV_SYMBOL_EYE_OPEN);
  }
  
  lv_label_set_text(guiParticipants[handleGFX].labelConnectionStatus, conn.c_str());
  if (guiParticipants[handleGFX].inRace) {
    lv_label_set_text(guiParticipants[handleGFX].labelRaceConnectionStatus, conn.c_str());
  }

  if(msg.battery >= 0 && msg.battery <=100) {
    lv_label_set_text_fmt(guiParticipants[handleGFX].labelBattery, "%3d%%",msg.battery);
  }

  //gfxUpdateParticipantChartRSSI(handleGFX,msg.connectionStatus);
}

// updated same fields as gfxAddParticipant() but without creating a new 
static void gfxUpdateParticipant(msg_UpdateParticipant &msgParticipant)
{
  uint32_t handleGFX = msgParticipant.handleGFX;

  gfxUpdateInRace(msgParticipant.inRace, handleGFX);

  lv_obj_set_style_bg_color(guiParticipants[handleGFX].ledColor1, lv_color_hex(msgParticipant.color1),0);
  lv_obj_set_style_bg_color(guiParticipants[handleGFX].ledColor0, lv_color_hex(msgParticipant.color0),0);
  lv_textarea_set_text(guiParticipants[handleGFX].textAreaName, msgParticipant.name);
  
  if (guiParticipants[handleGFX].inRace) {
    lv_obj_set_style_bg_color(guiParticipants[handleGFX].ledRaceColor1, lv_color_hex(msgParticipant.color1),0);
    lv_obj_set_style_bg_color(guiParticipants[handleGFX].ledRaceColor0, lv_color_hex(msgParticipant.color0),0);
    lv_label_set_text(guiParticipants[handleGFX].labelRaceName, msgParticipant.name);
  }
}

// return used handleGFX or negative value in case of error
static uint32_t gfxAddParticipant(msg_AddParticipant &msgParticipant)
{
  uint32_t handleGFX = globalHandleGFX;

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
  gfxUpdateInRace(msgParticipant.inRace, handleGFX);
  gfxUpdateParticipantChartNewLap(handleGFX,0,0,0);
  globalHandleGFX++;
  if(handleGFX >= ITAG_COUNT) {
    ESP_LOGI(TAG,"Added all Users handleGFX:%d from MSG:0x%x handleDB:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d --------- COME ON LETS PARTY!!!!!!!!!!!!!!!!!!", handleGFX,
               msgParticipant.header.msgType, msgParticipant.handleDB, msgParticipant.color0, msgParticipant.color1, msgParticipant.name, msgParticipant.inRace);

  }

  // All Ok return used handleGFX
  return handleGFX;
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

void guiRace::receiveConfigRace(msg_RaceConfig *raceConfig)
{
  ESP_LOGI(TAG,"receiveConfigRace()");


  //ESP_LOGI(TAG,"Received: MSG_RACE_CONFIG MSG:0x%x filename:%s name:%s distace:%d laps:%d blockNewLapTime:%d updateCloserTime:%d, raceStartInTime:%d",
  //      raceConfig->header.msgType, raceConfig->fileName, raceConfig->name,raceConfig->distance, raceConfig->laps, 
  //      raceConfig->blockNewLapTime, raceConfig->updateCloserTime, raceConfig->raceStartInTime);
  std::string fileName = std::string(raceConfig->fileName);
  std::string name = std::string(raceConfig->name);
  bool timeBasedRace = raceConfig->timeBasedRace;
  std::string maxTime = std::to_string(raceConfig->maxTime);
  std::string distance = std::to_string(raceConfig->distance);
  laps = raceConfig->laps;
  time_t blockNewLapTime = raceConfig->blockNewLapTime;
  std::string updateCloserTime = std::to_string(raceConfig->updateCloserTime);
  std::string raceStartInTime = std::to_string(raceConfig->raceStartInTime);

  if (laps == 0) {
    // Laps can't be 0 assume 1
    laps = 1;
  }
  uint32_t lapsDistances;
  if (timeBasedRace) {
    lapsDistances = raceConfig->distance;
  }
  else {
    lapsDistances = raceConfig->distance / laps;
  }

  // Genaral info page
  lv_label_set_text(globalLabelRaceName, name.c_str());

  // Race config info page
  lv_textarea_set_text(textAreaConfigRaceFileName,fileName.c_str());
  lv_textarea_set_text(textAreaConfigRaceName,name.c_str());

  if (timeBasedRace) {
    lv_obj_add_state(textAreaConfigRaceTimebased, LV_STATE_CHECKED);
    lv_textarea_set_text(textAreaConfigRaceLaps,std::to_string(laps).c_str());
    lv_obj_add_state(textAreaConfigRaceLaps, LV_STATE_DISABLED);
  }
  else {
    lv_obj_clear_state(textAreaConfigRaceTimebased, LV_STATE_CHECKED);
    lv_textarea_set_text(textAreaConfigRaceLaps,std::to_string(laps).c_str());
    lv_obj_clear_state(textAreaConfigRaceLaps, LV_STATE_DISABLED);
  }

  lv_textarea_set_text(textAreaConfigRaceMaxTime,maxTime.c_str());
  lv_textarea_set_text(textAreaConfigRaceDistance,distance.c_str());
  lv_label_set_text(textAreaConfigRaceLapsDistances,std::to_string(lapsDistances).c_str());
  setBlockNewLapTime(blockNewLapTime);
  lv_textarea_set_text(textAreaConfigRaceUpdateCloserTime,updateCloserTime.c_str());
  lv_textarea_set_text(textAreaConfigRaceRaceStartIn,raceStartInTime.c_str());
  dataValid = true;
  updateGUITabRaceGraph(); // Update graph to new race dimensions
}

time_t guiRace::getMaxTime()
{
  time_t maxTime = std::stoi( std::string(lv_textarea_get_text(textAreaConfigRaceMaxTime)));
  return maxTime;
}

void guiRace::sendConfigRace()
{
  std::string configRaceFileName    = std::string(lv_textarea_get_text(textAreaConfigRaceFileName));
  std::string configRaceName        = std::string(lv_textarea_get_text(textAreaConfigRaceName));
  bool timeBasedRace = lv_obj_get_state(textAreaConfigRaceTimebased) & LV_STATE_CHECKED;
  time_t maxTime = std::stoi( std::string(lv_textarea_get_text(textAreaConfigRaceMaxTime)));
  uint32_t configRaceDistance       = std::stoi( std::string(lv_textarea_get_text(textAreaConfigRaceDistance)));
  //laps                              = std::stoi( std::string(lv_textarea_get_text(textAreaConfigRaceLaps)));
  time_t configRaceBlockNewLapTime  = std::stoi( std::string(lv_textarea_get_text(textAreaConfigRaceBlockNewLapTime)));
  time_t configRaceUpdateCloserTime = std::stoi( std::string(lv_textarea_get_text(textAreaConfigRaceUpdateCloserTime)));
  time_t configRaceRaceStartIn      = std::stoi( std::string(lv_textarea_get_text(textAreaConfigRaceRaceStartIn)));

  if (laps == 0) {
    // Laps cant be 0 assume 1
    laps = 1;
    lv_textarea_set_text(textAreaConfigRaceLaps,std::to_string(laps).c_str());
  }

  uint32_t configRaceLapsDistances = configRaceDistance / laps;
  lv_label_set_text(textAreaConfigRaceLapsDistances,std::to_string(configRaceLapsDistances).c_str());

  msg_RaceDB msg;
  msg.Broadcast.RaceConfig.header.msgType = MSG_RACE_CONFIG;
  size_t len = configRaceFileName.copy(msg.Broadcast.RaceConfig.fileName, PARTICIPANT_NAME_LENGTH);
  msg.Broadcast.RaceConfig.fileName[len] = '\0';
  len = configRaceName.copy(msg.Broadcast.RaceConfig.name, PARTICIPANT_NAME_LENGTH);
  msg.Broadcast.RaceConfig.name[len] = '\0';
  msg.Broadcast.RaceConfig.timeBasedRace = timeBasedRace;
  msg.Broadcast.RaceConfig.maxTime = maxTime;
  msg.Broadcast.RaceConfig.distance = configRaceDistance;
  msg.Broadcast.RaceConfig.laps = laps;
  msg.Broadcast.RaceConfig.blockNewLapTime = configRaceBlockNewLapTime;
  msg.Broadcast.RaceConfig.updateCloserTime = configRaceUpdateCloserTime;
  msg.Broadcast.RaceConfig.raceStartInTime = configRaceRaceStartIn;

  //ESP_LOGI(TAG,"Send: MSG_RACE_CONFIG MSG:0x%x filename:%s name:%d distace:%d laps:%d blockNewLapTime:%d updateCloserTime:%d, raceStartInTime:%d",
  //      msg.Broadcast.RaceConfig.header.msgType, msg.Broadcast.RaceConfig.fileName, msg.Broadcast.RaceConfig.name,msg.Broadcast.RaceConfig.distance, msg.Broadcast.RaceConfig.laps, 
  //      msg.Broadcast.RaceConfig.blockNewLapTime, msg.Broadcast.RaceConfig.updateCloserTime, msg.Broadcast.RaceConfig.raceStartInTime);

  BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 )); // TODO add resend ?
  if (!xReturned) {
    // it it fails let the user click again
    ESP_LOGW(TAG,"WARNING: Send: MSG_RACE_CONFIG MSG:0x%x could not be sent in 2000ms. USER need to retry", msg.Broadcast.RaceConfig.header.msgType);
  }

}

static void taConfigText_event_cb(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  //uint32_t handleGFX = reinterpret_cast<uint32_t>(lv_event_get_user_data(e));
  if(code == LV_EVENT_FOCUSED) {
    if(lv_indev_get_type(lv_indev_get_act()) != LV_INDEV_TYPE_KEYPAD) {
      lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
      lv_keyboard_set_textarea(keyboard, ta);
      lv_obj_set_style_max_height(keyboard, LV_HOR_RES * 2 / 3, 0);
      lv_obj_update_layout(tabView);   /*Be sure the sizes are recalculated*/
      lv_obj_set_height(tabView, LV_VER_RES - lv_obj_get_height(keyboard));
      lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
      lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON);
    }
  }
  else if(code == LV_EVENT_DEFOCUSED) {
    lv_keyboard_set_textarea(keyboard, NULL);
    lv_obj_set_height(tabView, LV_VER_RES);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_indev_reset(NULL, ta);
  }
  else if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_set_height(tabView, LV_VER_RES);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(ta, LV_STATE_FOCUSED);
    lv_indev_reset(NULL, ta);   /*To forget the last clicked object to make it focusable again*/
  }

  // Config updated -> send update signal to RaceDB, and update signal will be send back
  // that will resync name in all tabs.
  if(code == LV_EVENT_READY || code == LV_EVENT_DEFOCUSED || code == LV_EVENT_CANCEL)
  {
    guiRace.sendConfigRace();
  }
}

static void taConfigNum_event_cb(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  //uint32_t handleGFX = reinterpret_cast<uint32_t>(lv_event_get_user_data(e));
  if(code == LV_EVENT_FOCUSED) {
    if(lv_indev_get_type(lv_indev_get_act()) != LV_INDEV_TYPE_KEYPAD) {
      lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
      lv_keyboard_set_textarea(keyboard, ta);
      lv_obj_set_style_max_height(keyboard, LV_HOR_RES * 2 / 3, 0);
      lv_obj_update_layout(tabView);   /*Be sure the sizes are recalculated*/
      lv_obj_set_height(tabView, LV_VER_RES - lv_obj_get_height(keyboard));
      lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
      lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON);
    }
  }
  else if(code == LV_EVENT_DEFOCUSED) {
    lv_keyboard_set_textarea(keyboard, NULL);
    lv_obj_set_height(tabView, LV_VER_RES);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_indev_reset(NULL, ta);
  }
  else if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_set_height(tabView, LV_VER_RES);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(ta, LV_STATE_FOCUSED);
    lv_indev_reset(NULL, ta);   /*To forget the last clicked object to make it focusable again*/
  }

  // Config updated -> send update signal to RaceDB, and update signal will be send back
  // that will resync name in all tabs.
  if(code == LV_EVENT_READY || code == LV_EVENT_DEFOCUSED || code == LV_EVENT_CANCEL)
  {
    // Update Laps
    if (guiRace.isTextAreaLaps(ta)) {
        //uint32_t raceDistance = std::stoi( std::string(lv_textarea_get_text(textAreaConfigRaceDistance)));
        uint32_t laps = std::stoi( std::string(lv_textarea_get_text(ta)));
        if (laps == 0) {
          laps = 1;
          lv_textarea_set_text(ta,"1");
        }
        guiRace.setLaps(laps);
    }
    // Update Distance
    if (guiRace.isTextAreaDistance(ta)) {
        //uint32_t raceDistance = std::stoi( std::string(lv_textarea_get_text(textAreaConfigRaceDistance)));
        uint32_t dist = std::stoi( std::string(lv_textarea_get_text(ta)));
        guiRace.setDistance(dist);
    }
    // Check if we should update block time
    if (guiRace.isTextAreaLaps(ta) || guiRace.isTextAreaDistance(ta)) {
      uint32_t laps = 1;
      if (!guiRace.isTimeBasedRace()) {
        laps = guiRace.getLaps();
        if (laps == 0) {
            laps = 1;
        }
      }
      // Max speed is 2,83min/km (or 170s/km e.g. Marathon on 2h) on the lap, this is used to not count a new lap in less time then this
      time_t blockNewLapTime = ((170*guiRace.getDistance()/laps)/1000);
      guiRace.setBlockNewLapTime(blockNewLapTime);
    }

    guiRace.sendConfigRace();
  }
}

static void taConfigBool_event_cb(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *cb = lv_event_get_target(e);
  //uint32_t handleGFX = reinterpret_cast<uint32_t>(lv_event_get_user_data(e));
  //if(code == LV_EVENT_VALUE_CHANGED) {
  //    const char * txt = lv_checkbox_get_text(obj);
  //    const char * state = lv_obj_get_state(obj) & LV_STATE_CHECKED ? "Checked" : "Unchecked";
  //    LV_LOG_USER("%s: %s", txt, state);
  //}

  // Config updated -> send update signal to RaceDB, and update signal will be send back
  // that will resync name in all tabs.
  if(code == LV_EVENT_VALUE_CHANGED)
  {
    // Check if we should update block time
    if (guiRace.isCheckBoxTimeBased(cb)) {
      uint32_t laps = 1;
      if (!guiRace.isTimeBasedRace()) {
        laps = guiRace.getLaps();
        if (laps == 0) {
            laps = 1;
        }
      }
      // Max speed is 2,83min/km (or 170s/km e.g. Marathon on 2h) on the lap, this is used to not count a new lap in less time then this
      time_t blockNewLapTime = ((170*guiRace.getDistance()/laps)/1000);
      guiRace.setBlockNewLapTime(blockNewLapTime);
    }

    guiRace.sendConfigRace();
  }
}

static lv_obj_t * addConfigText(lv_obj_t * parent, uint8_t row, char *labelText, char* content, char* extra, bool editable)
{
  uint8_t col = 0;

  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_grid_cell(label, LV_GRID_ALIGN_END, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);
  lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_add_style(label, &styleTagSmallText, 0);
  lv_label_set_text(label, labelText);
  lv_obj_center(label);

  lv_obj_t * lvobj;
  if (editable) {
    lvobj = lv_textarea_create(parent);
    lv_textarea_set_one_line(lvobj, true);
    lv_textarea_set_password_mode(lvobj, false);
    lv_textarea_set_text(lvobj, content);
    lv_obj_add_event_cb(lvobj, taConfigText_event_cb, LV_EVENT_ALL, reinterpret_cast<void *>(0));
  }
  else {
    lvobj = lv_label_create(parent);
    lv_label_set_text(lvobj, content);
  }
  lv_obj_add_style(lvobj, &styleTagSmallText, 0);
  lv_obj_set_grid_cell(lvobj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

  label = lv_label_create(parent);
  lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);
  lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_add_style(label, &styleTagSmallText, 0);
  lv_label_set_text(label, extra);
  lv_obj_center(label);

  return lvobj;
}

static lv_obj_t * addConfigNumber(lv_obj_t * parent, uint8_t row, char *labelText, uint32_t content, char* extra, bool editable)
{
  uint8_t col = 0;

  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_grid_cell(label, LV_GRID_ALIGN_END, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);
  lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_add_style(label, &styleTagSmallText, 0);
  lv_label_set_text(label, labelText);
  lv_obj_center(label);

  lv_obj_t * lvobj;
  if (editable) {
    lvobj = lv_textarea_create(parent);
    lv_textarea_set_accepted_chars(lvobj, "0123456789");
    lv_textarea_set_one_line(lvobj, true);
    lv_textarea_set_password_mode(lvobj, false);
    lv_textarea_set_text(lvobj, std::to_string(content).c_str());
    lv_obj_add_event_cb(lvobj, taConfigNum_event_cb, LV_EVENT_ALL, reinterpret_cast<void *>(0));
  }
  else {
    lvobj = lv_label_create(parent);
    lv_label_set_text(lvobj, std::to_string(content).c_str());
  }
  lv_obj_add_style(lvobj, &styleTagSmallText, 0);
  lv_obj_set_grid_cell(lvobj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

  label = lv_label_create(parent);
  lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);
  lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_add_style(label, &styleTagSmallText, 0);
  lv_label_set_text(label, extra);
  lv_obj_set_align(label,LV_ALIGN_LEFT_MID);
  //lv_obj_center(label);

  return lvobj;
}

static lv_obj_t * addConfigBool(lv_obj_t * parent, uint8_t row, char *labelText, bool content, char* extra, bool editable)
{
  uint8_t col = 0;

  col++; //Nothing here "labelText" is placed in checkbox

  lv_obj_t * lvobj;
  lvobj = lv_checkbox_create(parent);
  lv_checkbox_set_text(lvobj, labelText);
  if (content) {
    lv_obj_add_state(lvobj, LV_STATE_CHECKED);
  }
  else {
    lv_obj_clear_state(lvobj, LV_STATE_CHECKED);
  }
  if (editable) {
    lv_obj_clear_state(lvobj, LV_STATE_DISABLED);
    lv_obj_add_event_cb(lvobj, taConfigBool_event_cb, LV_EVENT_ALL, reinterpret_cast<void *>(0));
  }
  else {
    lv_obj_add_state(lvobj, LV_STATE_DISABLED);
  }
  lv_obj_add_style(lvobj, &styleTagSmallText, 0);
  lv_obj_set_grid_cell(lvobj, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);

  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, col++, 1, LV_GRID_ALIGN_CENTER, row, 1);
  lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_add_style(label, &styleTagSmallText, 0);
  lv_label_set_text(label, extra);
  lv_obj_set_align(label,LV_ALIGN_LEFT_MID);
  //lv_obj_center(label);

  return lvobj;
}

/*
static void scroll_begin_event(lv_event_t * e)
{
    // Disable the scroll animations. Triggered when a tab button is clicked
    if(lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
        lv_anim_t * a = lv_event_get_param(e);
        if(a)  a->time = 0;
    }
}
*/

void guiRace::createGUITabConfig(lv_obj_t * settingTab)
{
  lv_obj_t * tabview;
  tabview = lv_tabview_create(settingTab, LV_DIR_LEFT, 80);
  //lv_obj_add_event_cb(lv_tabview_get_content(tabview), scroll_begin_event, LV_EVENT_SCROLL_BEGIN, NULL);

  lv_obj_t * tabConf = lv_tabview_add_tab(tabview, LV_SYMBOL_DIRECTORY);
  //lv_obj_t * tabSignalStrenght = lv_tabview_add_tab(tabview, LV_SYMBOL_WIFI);
#ifdef TESTCODE
  lv_obj_t * tabTest = lv_tabview_add_tab(tabview, "Test");
#endif
  // ------------------------------ RaceConfig
  lv_obj_set_flex_flow(tabConf, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_column(tabConf,2,0);
  lv_obj_set_style_pad_row(tabConf,2,0);
  lv_obj_set_style_pad_all(tabConf, 2,0);

  // ---- Load & Save buttons

  lv_obj_t * panel1 = lv_obj_create(tabConf);
  lv_obj_set_height(panel1, LV_SIZE_CONTENT);
  lv_obj_set_width(panel1, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(panel1, LV_FLEX_FLOW_ROW);

  lv_obj_t * btnLoad = lv_btn_create(panel1); 
  lv_obj_add_event_cb(btnLoad, btnLoad_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *labelLoad = lv_label_create(btnLoad);          /*Add a label to the button*/
  lv_label_set_text(labelLoad, "Load");                     /*Set the labels text*/
  lv_obj_center(labelLoad);
  lv_obj_add_style(labelLoad, &styleTime, 0);

  lv_obj_t * btnSave = lv_btn_create(panel1); 
  lv_obj_add_event_cb(btnSave, btnSave_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *labelSave = lv_label_create(btnSave);          /*Add a label to the button*/
  lv_label_set_text(labelSave, "Save");                     /*Set the labels text*/
  lv_obj_center(labelSave);
  lv_obj_add_style(labelSave, &styleTime, 0);

  // ---- Race Config table
  static lv_coord_t col_dsc[] = {                 LV_GRID_CONTENT, LV_GRID_FR(10), LV_GRID_CONTENT,       LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, 
                                 LV_GRID_CONTENT,
                                 LV_GRID_CONTENT,
                                 LV_GRID_CONTENT,
                                 LV_GRID_CONTENT,
                                 LV_GRID_CONTENT,
                                 LV_GRID_CONTENT,
                                 LV_GRID_CONTENT,
                                 LV_GRID_CONTENT,
                                 LV_GRID_CONTENT,
                                 LV_GRID_TEMPLATE_LAST};

  lv_obj_t * configGrid = lv_obj_create(tabConf);
  lv_obj_set_size(configGrid, LV_PCT(100), LV_SIZE_CONTENT);

  lv_obj_set_grid_dsc_array(configGrid, col_dsc, row_dsc);

  uint32_t row = 0;
  textAreaConfigRaceFileName         = addConfigText(  configGrid, row++, "File name:","FileName", "", true);          //TODO add Load/save buttons here instead?
  textAreaConfigRaceName             = addConfigText(  configGrid, row++, "Race name:","Name", "", true);
  textAreaConfigRaceTimebased        = addConfigBool(  configGrid, row++, "Timebased race",false, "", true);
  textAreaConfigRaceMaxTime          = addConfigNumber(configGrid, row++, "Max time:",6, "hours", true);
  textAreaConfigRaceDistance         = addConfigNumber(configGrid, row++, "Distance:",0, "meter", true);
  textAreaConfigRaceLaps             = addConfigNumber(configGrid, row++, "Laps:",0, "", true);
  textAreaConfigRaceLapsDistances    = addConfigNumber(configGrid, row++, "Laps distance:", 0, "meter", false);  // Not editable will be a label instead of textarea
  textAreaConfigRaceBlockNewLapTime  = addConfigNumber(configGrid, row++, "Block new lap until:", 0, "s", true);
  textAreaConfigRaceUpdateCloserTime = addConfigNumber(configGrid, row++, "Participand closing in time:", 0, "s", true);
  textAreaConfigRaceRaceStartIn      = addConfigNumber(configGrid, row++, "Race start countdown:", 0, "s", true);

  // ------------------------------ TagSignal
  //createGUITabRSSI(tabSignalStrenght);

#ifdef TESTCODE
  // ------------------------------ Test
  lv_obj_set_flex_flow(tabTest, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_column(tabTest,2,0);
  lv_obj_set_style_pad_row(tabTest,2,0);
  lv_obj_set_style_pad_all(tabTest, 2,0);

  lv_obj_t * btnm1 = lv_btnmatrix_create(tabTest);
  lv_btnmatrix_set_map(btnm1, btnTest_map);
  lv_obj_align(btnm1, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_event_cb(btnm1, btnTest_event_cb, LV_EVENT_ALL, NULL);
  lv_obj_set_height(btnm1, LV_PCT(100));
  lv_obj_set_width(btnm1, LV_PCT(100));


#endif
  ESP_LOGI(TAG,"createGUITabConfig() Done");
}

/*
void guiRace::createGUITabRSSI(lv_obj_t * parent)
{
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_column(parent,5,0);
  lv_obj_set_style_pad_row(parent,5,0);
  lv_obj_set_style_pad_all(parent, 5,0);

  chartRSSI = lv_chart_create(parent);
  lv_obj_align_to(chartRSSI, parent, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_size(chartRSSI, LV_PCT(100), LV_PCT(100));
  lv_chart_set_type(chartRSSI, LV_CHART_TYPE_LINE);

  lv_chart_set_div_line_count(chartRSSI, 10, 10);
  lv_obj_set_style_size(chartRSSI, 0, LV_PART_INDICATOR); // Do not display points on the data

#define RSSI_CHART_POINTS 780

  lv_chart_set_range(chartRSSI, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_point_count(chartRSSI, RSSI_CHART_POINTS);

  lv_chart_set_update_mode(chartRSSI, LV_CHART_UPDATE_MODE_SHIFT);
}
*/


void createGUI(void)
{
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

  keyboard = lv_keyboard_create(lv_scr_act());
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);

  tabView = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, TAB_HIGHT); // height of tab area
  lv_obj_set_style_text_font(lv_scr_act(), fontTag, 0);

  lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabView);
  lv_obj_set_style_pad_left(tab_btns, TAB_POS, 0);

//  lv_obj_t * logo = lv_img_create(tab_btns);
//  LV_IMG_DECLARE(img_lvgl_logo);
//  lv_img_set_src(logo, &img_lvgl_logo);
//  lv_obj_align(logo, LV_ALIGN_LEFT_MID, -TAB_POS + 25, 0);

  globalLabelRaceName = lv_label_create(tab_btns);
  lv_label_set_text(globalLabelRaceName, "VXO SM24H 2023");
  lv_obj_add_style(globalLabelRaceName, &styleTitle, 0);
//  lv_obj_align_to(globalLabelRaceName, logo, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);
  lv_obj_align(globalLabelRaceName, LV_ALIGN_OUT_LEFT_TOP, -TAB_POS + 10, 10);

  globalLabelRaceTag = lv_label_create(tab_btns);
  lv_label_set_text(globalLabelRaceTag, "Crazy Capy Time");
  lv_obj_add_style(globalLabelRaceTag, &styleTagSmallText, 0);
  lv_obj_add_style(globalLabelRaceTag, &styleTextMuted, 0);
//  lv_obj_align_to(globalLabelRaceTag, logo, LV_ALIGN_OUT_RIGHT_BOTTOM, 10, 0);
  lv_obj_align(globalLabelRaceTag, LV_ALIGN_OUT_LEFT_TOP, -TAB_POS + 10, 35);
//  lv_obj_align_to(globalLabelRaceTag, labelRaceName, LV_ALIGN_OUT_LEFT_BOTTOM, 0, 20);

  lv_obj_t * btnTime = lv_btn_create(tab_btns); 
  lv_obj_align_to(btnTime, globalLabelRaceName, LV_ALIGN_OUT_RIGHT_TOP, 10, -10);
  //lv_obj_align(btnTime, LV_ALIGN_CENTER, -30, -((TAB_HIGHT-10)/2));
  //lv_obj_set_pos(btnTime, 10, 10);                            /*Set its position*/
  //lv_obj_set_size(btnTime, 120, 50);                          /*Set its size*/
  lv_obj_set_width(btnTime,TAB_TIME_WIDTH);
  lv_obj_add_event_cb(btnTime, btnTime_event_cb, LV_EVENT_ALL, NULL);

  globalLabelRaceTime = lv_label_create(btnTime);  // Add a label to the button
  lv_label_set_text(globalLabelRaceTime, "Start!");
  lv_obj_center(globalLabelRaceTime);
  lv_obj_add_style(globalLabelRaceTime, &styleTime, 0);

  tabRace = lv_tabview_add_tab(tabView, "Race"); 
  tabParticipants = lv_tabview_add_tab(tabView, LV_SYMBOL_LIST );
  lv_obj_t * tabGraph = lv_tabview_add_tab(tabView, LV_SYMBOL_IMAGE );
  //lv_obj_t * tab4 = lv_tabview_add_tab(tabView, LV_SYMBOL_WIFI );
  lv_obj_t * tabConf = lv_tabview_add_tab(tabView, LV_SYMBOL_EDIT );

  createGUITabRace(tabRace);
  createGUITabParticipant(tabParticipants);
  guiRace.setTabGraph(tabGraph); // Will be re-configurated later with when we get the race config msg
  guiRace.createGUITabConfig(tabConf);
}

void updateGUITime()
{
  char buff[30];
  tm timeNow = rtc.getTimeStruct();
  time_t now = mktime(&timeNow);

  if (raceOngoing) {
    time_t currentRaceTime = difftime(now, guiRace.getRaceStart());
    strftime (buff, 30, "%H:%M:%S", localtime(&currentRaceTime));
    lv_label_set_text(globalLabelRaceTime, buff);
  }
  else if(raceStartIn) {
    lv_label_set_text_fmt(globalLabelRaceTime, ">>  %d  <<", raceStartIn);
  }

  strftime (buff, 30, "%Y-%m-%d %H:%M:%S", localtime(&now));
  lv_label_set_text(globalLabelRaceTag, buff);
}

void loopHandlLVGL()
{
  for( ;; )
  {
    msg_GFX msg;
    while ( xQueueReceive(queueGFX, &(msg), (TickType_t)portMAX_DELAY) == pdPASS)
    {
      //ESP_LOGE(TAG,"----- loopHandlLVGL() msg.header.msgType = 0x%x -----",msg.header.msgType);
      switch(msg.header.msgType) {
        case MSG_GFX_TIMER:
        {
          static unsigned long lastTimeUpdate = 356*24*60*60; //start on something we will never match like 1971
          unsigned long now = rtc.getEpoch();
          if (lastTimeUpdate != now) {
            lastTimeUpdate = now;
            updateGUITime();
          }
          lv_timer_handler();
          // Done! No response on this msg
          break;        
        }
        case MSG_GFX_UPDATE_USER_DATA:
        {
          //ESP_LOGI(TAG,"Recived MSG_GFX_UPDATE_USER_DATA: MSG:0x%x handleGFX:0x%08x distance:%d laps:%d lastlaptime:%d,connectionStatus:%d",
          //        msg.Update.header.msgType, msg.Update.handleGFX, msg.Update.distance, msg.Update.laps, msg.Update.lastlaptime, msg.Update.connectionStatus);
          gfxUpdateParticipantData(msg.UpdateUserData);
          // Done! No response on this msg
          break;
        }
        case MSG_GFX_UPDATE_USER_STATUS:
        {
          //ESP_LOGI(TAG,"Recived MSG_GFX_UPDATE_USER_STATUS: MSG:0x%x handleGFX:0x%08x connectionStatus:%d battery:%d inRace:%d",
          //              msg.UpdateStatus.header.msgType, msg.UpdateStatus.handleGFX, msg.UpdateStatus.connectionStatus, msg.UpdateStatus.battery, msg.UpdateStatus.inRace);
          gfxUpdateParticipantStatus(msg.UpdateStatus);
          break;
        }
        case MSG_GFX_UPDATE_USER:
        {
          //ESP_LOGI(TAG,"Received: MSG_GFX_UPDATE_USER MSG:0x%x handleGFX:0x%08x color:(0x%x,0x%x) Name:%s inRace:%d", 
          //             msg.UpdateUser.header.msgType, msg.UpdateUser.handleGFX, msg.UpdateUser.color0, msg.UpdateUser.color1, msg.UpdateUser.name, msg.UpdateUser.inRace);
          gfxUpdateParticipant(msg.UpdateUser);
          // Done! No response on this msg
          break;
        }
        case MSG_GFX_ADD_USER:
        {
          //ESP_LOGI(TAG,"Received: MSG_GFX_ADD_USER MSG:0x%x handleDB:0x%08x color:(0x%x,0x%x) Name:%s inRace:%d", 
          //             msg.AddUser.header.msgType, msg.AddUser.handleDB, AddUser.Add.color0, msg.AddUser.color1, msg.AddUser.name, msg.AddUser.inRace);
          uint32_t handle = gfxAddParticipant(msg.AddUser);

          msg_RaceDB msgResponse;
          msgResponse.AddedToGFX.header.msgType = MSG_ITAG_GFX_ADD_USER_RESPONSE;
          msgResponse.AddedToGFX.handleDB = msg.AddUser.handleDB;
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
          // Broadcast Messages
          case MSG_RACE_START:
          {
            //ESP_LOGI(TAG,"Received: MSG_RACE_START MSG:0x%x startTime:%d DO NOTHING", msg.Broadcast.RaceStart.header.msgType,msg.Broadcast.RaceStart.startTime);
            // TODO gfxRaceStart(msg.Broadcast.RaceStart.startTime);
            guiRace.setRaceStart(msg.Broadcast.RaceStart.startTime);
            break;
          }
          case MSG_RACE_CLEAR:
          {
            ESP_LOGI(TAG,"Received: MSG_RACE_CLEAR MSG:0x%x", msg.Broadcast.RaceStart.header.msgType);
            gfxClearAllParticipantData();
            break;
          }
          case MSG_RACE_CONFIG:
          {
            //ESP_LOGI(TAG,"Received: MSG_RACE_CONFIG MSG:0x%x", msg.Broadcast.RaceConfig.header.msgType);
            guiRace.receiveConfigRace(&msg.Broadcast.RaceConfig);
            break;
          }
          default:
          ESP_LOGE(TAG,"ERROR received bad msg: 0x%x",msg.header.msgType);
          //break;
      }
      //ESP_LOGE(TAG,"----- loopHandlLVGL() msg.header.msgType = 0x%x DONE -----",msg.header.msgType);
    }
  }
}

// ######################################################## GFX & Touch Driver stuff

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

void vTaskLVGL( void *pvParameters )
{
  /* The parameter value is expected to be 2 as 2 is passed in the
     pvParameters value in the call to xTaskCreate() below. 
  */
  //configASSERT( ( ( uint32_t ) pvParameters ) == 2 );

  ESP_LOGI(TAG, "Setup GFX");
  
  int32_t prefer_speed = 16000000; // MAKERFAB_800x480
  if (HW_Platform == HWPlatform::Sunton_800x480 ) {
    // SUNTON_800x480
    prefer_speed = 14000000;
  }

  gfx = new Arduino_RPi_DPI_RGBPanel(
    bus,
    800 /* width */, 0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
    480 /* height */, 0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
    1 /* pclk_active_neg */, prefer_speed /* prefer_speed */, true /* auto_flush */);

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
  uint32_t buffersize = sizeof(lv_color_t) * screenWidth * screenHeight / 4;
  ESP_LOGE(TAG, "Alloc gfx framebuffer: %d bytes",buffersize);
  //static uint8_t framebuf[192000];
  //disp_draw_buf = (lv_color_t *) &framebuf;
  //disp_draw_buf = (lv_color_t *)heap_caps_malloc(buffersize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(buffersize, MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT);
//#else
//  disp_draw_buf = (lv_color_t *)malloc(buffersize);
//#endif
  if (!disp_draw_buf)
  {
    ESP_LOGE(TAG, "LVGL disp_draw_buf allocate failed! size:%d",buffersize);
    showHeapInfo();
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

    createGUI(); // MUST be done before adding participants
    ESP_LOGI(TAG, "Setup GFX done");

    for(;;)
    {
      ESP_LOGI(TAG,"----- loopHandlLVGL() -----");
      loopHandlLVGL();
      delay(5);
    }
  }

  ESP_LOGE(TAG,"FATAL ERROR: in vTaskLVGL() Setup Failed");
  ESP_LOGE(TAG,"----- esp_restart() -----");
  esp_restart();
  vTaskDelete( NULL ); // Should never be reached in the good case
}

// WARNING Executes in the timer deamon contex, NO blocking and NO touching of our data, we will just send a msg to out thread and handle everything there.
void vTaskGUITimer( TimerHandle_t xTimer )
{
  //Send a tick message to our message queue to do all work in our own thread
  msg_GFX msg;
  msg.Timer.header.msgType = MSG_GFX_TIMER;
  //ESP_LOGI(TAG,"Send: MSG_GFX_TIMER MSG:0x%x", msg.Timer.header.msgType);
  BaseType_t xReturned = xQueueSend(queueGFX, (void*)&msg, (TickType_t)0); //No blocking
  if( xReturned != pdPASS )
  {
    ESP_LOGW(TAG,"WARNING: Send: MSG_GFX_TIMER MSG:0x%x  Failed, do nothing, we try again in 2000ms", msg.Timer.header.msgType);
  }
}

void initLVGL()
{
  // Start LVGL Task
  BaseType_t xReturned;
  
  /* Create the task, storing the handle. */
  xReturned = xTaskCreate(
                  vTaskLVGL,         /* Function that implements the task. */
                  "GUI",             /* Text name for the task. */
                  TASK_GUI_STACK,    /* Stack size in words, not bytes. */
                  NULL,              /* Parameter passed into the task. */
                  TASK_GUI_PRIO,     /* Priority  0-(configMAX_PRIORITIES-1)   idle = 0 = tskIDLE_PRIORITY*/
                  &xHandleGUI );     /* Used to pass out the created task's handle. */

  if( xReturned != pdPASS )
  {
    ESP_LOGE(TAG,"FATAL ERROR: xTaskCreate(vTaskRaceDB, GUI,..) Failed");
    ESP_LOGE(TAG,"----- esp_restart() -----");
    esp_restart();
  }

  TimerHandle_t timerHandle = xTimerCreate("GUITimer", pdMS_TO_TICKS(100),pdTRUE, (void *) 0, vTaskGUITimer);
  if( timerHandle == NULL ) {
    ESP_LOGE(TAG,"FATAL ERROR: xTimerCreate(GUITimer,...) Failed");
    ESP_LOGE(TAG,"----- esp_restart() -----");
    esp_restart();
  }

  if( xTimerStart( timerHandle, pdMS_TO_TICKS(2000) ) != pdPASS ) {
    ESP_LOGE(TAG,"FATAL ERROR: xTimerStart(GUITimer) Failed");
    ESP_LOGE(TAG,"----- esp_restart() -----");
    esp_restart();
  }
}
