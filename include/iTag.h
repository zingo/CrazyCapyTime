#pragma once
#include <NimBLEDevice.h>
#include "gui.h"

#define ITAG_COUNT 3
class iTag;
extern iTag iTags[ITAG_COUNT];

// HANDLE_LAP_ON_SCAN will only connect the first time to config the tag, get battery info
// and activate it for the race, then it will handle the lap counting and timeing on the
// BT scanning this will probably avoid and speed up the time spend on detecting tag in the
// goal/aidstation and make it possible to run passed it faster and it still detect.
// If this works it will be the default way.
// Downside is that it will not be possible to make the iTag beep so maybe we want to add
// some sort of beeper on the main unit. We coauld also connect on the last lap to mark the finish
// with a beep. 
#define HANDLE_LAP_ON_SCAN


class iTag {
  public:
    std::string address;  // BT UUID
    std::string name;     // Readable name
    uint32_t color0;      // Color of iTag
    uint32_t color1;      // Color of iTag holder
    bool active;          // As in part of race
    uint32_t battery;
    bool connected; // As near enough right now
    uint32_t laps;
    tm timeLastShownUp;  // End time of last lap
    tm timeLastSeen;     // Start time of current lap (or a short while after)
    bool updated;
    iTag(std::string inAddress,std::string inName, uint32_t inColor0, uint32_t inColor1);
    bool connect(NimBLEAdvertisedDevice* advertisedDevice);

    void saveGUIObjects(lv_obj_t * ledColor, lv_obj_t * labelName, lv_obj_t * labelLaps, lv_obj_t * labelConnStatus, lv_obj_t * labelBatterySym, lv_obj_t * labelBat);
    void updateGUI(void);
    void updateGUI_locked(void);

  private:
    bool updateBattery(NimBLEClient* client);
    bool toggleBeep(NimBLEClient* client, bool beep);
    bool toggleBeepOnLost(NimBLEClient* client, bool beep);
#ifndef HANDLE_LAP_ON_SCAN
    NimBLEClient* pClient;
#endif
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