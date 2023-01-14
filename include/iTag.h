#pragma once
#include <NimBLEDevice.h>
#include "gui.h"

#define ITAG_COUNT 3
class iTag;
extern iTag iTags[ITAG_COUNT];

class iTag {
  public:
    std::string address;  // BT UUID
    std::string name;     // Readable name
    uint32_t color0;      // Color of iTag
    uint32_t color1;      // Color of iTag holder
    uint32_t battery;
    bool active;    // As in part of race
    bool connected; // As near enough right now
    uint32_t Last;
    uint32_t lapsScan;
    uint32_t laps;
    tm timeLastShownUp;  // End time of last lap
    tm timeLastSeen;     // Start time of current lap (or a short while after)
    iTag(std::string inAddress,std::string inName, uint32_t inColor0, uint32_t inColor1);
    bool updateBattery();
    bool toggleBeep(bool beep);
    bool toggleBeepOnLost(bool beep);
    bool connect(NimBLEAdvertisedDevice* advertisedDevice);

    void saveGUIObjects(lv_obj_t * ledColor, lv_obj_t * labelName, lv_obj_t * labelLaps, lv_obj_t * labelConnStatus, lv_obj_t * labelBatterySym, lv_obj_t * labelBat);
    void updateGUI(void);

  private:
    NimBLEClient* pClient;

    // GUI LVGL object used when updating
    lv_obj_t * ledColor;
    lv_obj_t * labelName;
    lv_obj_t * labelLaps;
    lv_obj_t * labelConnectionStatus;
    lv_obj_t * labelBatterySymbol;
    lv_obj_t * labelBattery;

};

void initiTAGs();
void loopHandlTAGs();