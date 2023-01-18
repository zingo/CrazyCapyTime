#pragma once

#include <vector>
#include <string>
#include <NimBLEDevice.h>
#include "gui.h"

#define ITAG_COUNT 3
class iTag;
extern iTag iTags[ITAG_COUNT];


void initiTAGs();
void loopHandlTAGs();

void startRaceiTags();
void refreshTagGUI();

// We will only connect the first time to config the tag, get battery info
// and activate it for the race, then it will handle the lap counting and timeing on the
// BT scanning this will probably avoid and speed up the time spend on detecting tag in the
// goal/aidstation and make it possible to run passed it faster and it still detect.
// If this works it will be the default way.
// Downside is that it will not be possible to make the iTag beep so maybe we want to add
// some sort of beeper on the main unit. We coauld also connect on the last lap to mark the finish
// with a beep. 

 //Hopefully good enough for any 6D race
#define MAX_SAVED_LAPS 1000

class lapData {
  public:
    lapData(): StartTime(0), LastSeen(0) {}
    time_t getLapStart() {return StartTime;}
    time_t getLastSeen() {return LastSeen;}
    void setLap(time_t timeSinceRaceStart,time_t timeSinceLapStart) {StartTime = timeSinceRaceStart; LastSeen = timeSinceLapStart;}
    void setLapStart(time_t timeSinceRaceStart) {StartTime = timeSinceRaceStart;}
    void setLastSeen(time_t timeSinceLapStart) {LastSeen = timeSinceLapStart;}
  private:
    time_t StartTime; // Start and End time of last lap
    time_t LastSeen;  // Start time of running lap e.g. last time Tag was seen (noot needed to save for lap but could be good for debug)
    //time_t LapTime; // Not needed next entry will contain this
    //uint32_t Distance; // Not neede for now all laps have equal length
};

class participantData {
  public:
    participantData(): name("Name"), laps(0), timeSinceLastSeen(0) { lapsData.resize(MAX_SAVED_LAPS); }
    std::string getName() {return name;}
    void setName(std::string inName) {name = inName;}
    uint32_t getLapCount() {return laps;}
    void prevLap() {if (laps>0) laps--; } //debug and correcting
    bool nextLap() {
      tm timeNow = rtc.getTimeStruct();
      time_t newLapTime = mktime(&timeNow);
      if ((laps + 1) < (MAX_SAVED_LAPS-1)) {
        laps++;
        setCurrentLap(newLapTime, 0);
        refreshTagGUI();
        return true;
      }
      //MAX LAP ERROR - Just make last lap extra long
      setCurrentLastSeen(newLapTime-getCurrentLapStart());
      refreshTagGUI();
      return false;  
    }

    void clearLaps() {
      laps = 0;
      timeSinceLastSeen = 0;
      for (lapData& lap : lapsData) {
        lap.setLap(0,0);
      }
    }

    lapData& getLap(uint32_t lap) { return lapsData.at(lap);}
    time_t getCurrentLapStart() {return lapsData.at(laps).getLapStart();}
    void setCurrentLapStart(time_t timeSinceRaceStart) {lapsData.at(laps).setLapStart(timeSinceRaceStart);}
    time_t getCurrentLastSeen() {return lapsData.at(laps).getLastSeen();}
    void setCurrentLastSeen(time_t timeSinceLapStart) {lapsData.at(laps).setLastSeen(timeSinceLapStart);}
    void setCurrentLap(time_t timeSinceRaceStart,time_t timeSinceLapStart) {lapsData.at(laps).setLap(timeSinceRaceStart,timeSinceLapStart);}

    uint32_t getTimeSinceLastSeen() {return timeSinceLastSeen;}
    void setTimeSinceLastSeen(time_t inTime) {timeSinceLastSeen=inTime;}
    void saveGUIObjects(lv_obj_t * chart, lv_chart_series_t * series) {
      lapsChart = chart;
      lapsSeries = series;
      updateChart();
    }
    void updateChart()
    {
      for(int lap=0;lap<=DRAW_MAX_LAPS_IN_CHART;lap++)
      {
        if (lap<=laps) {
          lapData theLap = getLap(lap);
          //two "dots" per lap, lap start and "last seen" e.g. left aidstation
          lapsSeries->x_points[2*lap] = theLap.getLapStart();
          lapsSeries->y_points[2*lap] = lap;
          lapsSeries->x_points[2*lap+1] = theLap.getLapStart() + theLap.getLastSeen() ;
          lapsSeries->y_points[2*lap+1] = lap;
        }
        else {
          lapsSeries->x_points[2*lap] = LV_CHART_POINT_NONE;
          lapsSeries->y_points[2*lap] = LV_CHART_POINT_NONE;
          lapsSeries->x_points[2*lap+1] = LV_CHART_POINT_NONE;
          lapsSeries->y_points[2*lap+1] = LV_CHART_POINT_NONE;
        }
      }
      lv_chart_refresh(lapsChart); /*Required after direct set*/
    }  
  private:
    std::string name;     // Participant name
    uint32_t laps;
    uint32_t timeSinceLastSeen; // in seconds, used to update UI Update when calculated
    std::vector<lapData> lapsData;
    //GUI stuff
    lv_obj_t * lapsChart;
    lv_chart_series_t * lapsSeries;
//TODO?    bool updated; // if true update GUI
 
};


class iTag {
  public:
    std::string address;  // BT UUID
    uint32_t color0;      // Color of iTag
    uint32_t color1;      // Color of iTag holder
    int32_t battery;      // 0-100 and -1 when unknown
    bool active;          // As in part of race
    bool connected;       // As near enough right now
  
    participantData participant;
    bool updated;
    iTag(std::string inAddress,std::string inName, uint32_t inColor0, uint32_t inColor1);
    bool connect(NimBLEAdvertisedDevice* advertisedDevice);

    void saveGUIObjects(lv_obj_t * ledColor, lv_obj_t * labelName, lv_obj_t * labelDist, lv_obj_t * labelLaps, lv_obj_t * labelTime, lv_obj_t * labelConnStatus, /*lv_obj_t * labelBatterySym,*/ lv_obj_t * labelBat);
    void updateGUI(void);
    void updateGUI_locked(void);
    int getRSSI() {return RSSI;}
    void setRSSI(int val) {RSSI=val;}
  private:
    int RSSI;
    bool updateBattery(NimBLEClient* client);
    bool toggleBeep(NimBLEClient* client, bool beep);
    bool toggleBeepOnLost(NimBLEClient* client, bool beep);

    // GUI LVGL object used when updating
    lv_obj_t * ledColor;
    lv_obj_t * labelName;
    lv_obj_t * labelDist;
    lv_obj_t * labelLaps;
    lv_obj_t * labelTime;
    lv_obj_t * labelConnectionStatus;
    //lv_obj_t * labelBatterySymbol;
    lv_obj_t * labelBattery;
};


