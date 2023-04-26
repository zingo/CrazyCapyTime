/*
  Handle iTag BT devices
  
  Based on some info found from the nice guides of Fernando K Tecnologia https://www.instructables.com/Multiple-ITags-With-ESP32/ and
  https://www.youtube.com/watch?v=uNGMq_U3ydw

*/
#include <mutex>
#include <string>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "common.h"
#include "iTag.h"
#include "messages.h"
#include "bluetooth.h"

#define TAG "iTAG"

static void refreshTagGUI();
static void DBsaveGlobalConfig();

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

class Race {
  public:
    Race() : 
      fileName("vxoSM24H2023.json"),
      name("VXO SM 24H"),
      timeBasedRace(true),
      maxTime(24),
      distance(821),
      laps(2),
      blockNewLapTime(3*60),
      updateCloserTime(30),
      raceStartInTime(15),
      raceOngoing(false),
      raceStart(0)
    {
      if(laps == 0) {  //TODO make this a compiler check
        laps = 1; // Should never be 0 but if it is lets fix it
      }
      uint32_t lapLenght;

      if (!timeBasedRace) {
        lapLenght = distance/laps;
      }
      else {
        // In timebase races lap lenght = dist  
        lapLenght = distance;
      }

      // Max speed is 2,83min/km (or 170s/km e.g. Marathon on 2h) on the lap, this is used to not count a new lap in less time then this
      blockNewLapTime = ((170*lapLenght)/1000);
    }

    void receive_ConfigMsg(msg_RaceConfig *raceConfig)
    {
      bool globalConfigIsUpdated = false;
      if (fileName != std::string(raceConfig->fileName)) {
        // Current race filename changes -> save global config
        globalConfigIsUpdated = true;
      }
      fileName = std::string(raceConfig->fileName);
      name = std::string(raceConfig->name);
      timeBasedRace = raceConfig->timeBasedRace;
      maxTime = raceConfig->maxTime;
      distance = raceConfig->distance;
      laps = raceConfig->laps;
      blockNewLapTime = raceConfig->blockNewLapTime;
      updateCloserTime = raceConfig->updateCloserTime;
      raceStartInTime = raceConfig->raceStartInTime;
      if(laps == 0) {
        laps = 1; // Should never be 0 but if it is lets fix it
      }
      if (globalConfigIsUpdated) {
        // Something in the global config has changes, save it.
        DBsaveGlobalConfig();
      }
    }

    void send_ConfigMsg(QueueHandle_t queue)
    {
        if(laps == 0) {
          laps = 1; // Should never be 0 but if it is lets fix it
        }
        msg_RaceDB msg;
        msg.Broadcast.RaceConfig.header.msgType = MSG_RACE_CONFIG;
        size_t len = fileName.copy(msg.Broadcast.RaceConfig.fileName, PARTICIPANT_NAME_LENGTH);
        msg.Broadcast.RaceConfig.fileName[len] = '\0';
        len = name.copy(msg.Broadcast.RaceConfig.name, PARTICIPANT_NAME_LENGTH);
        msg.Broadcast.RaceConfig.name[len] = '\0';

        msg.Broadcast.RaceConfig.timeBasedRace = timeBasedRace;
        msg.Broadcast.RaceConfig.maxTime = maxTime;
        msg.Broadcast.RaceConfig.distance = distance;
        msg.Broadcast.RaceConfig.laps = laps;
        msg.Broadcast.RaceConfig.blockNewLapTime = blockNewLapTime;
        msg.Broadcast.RaceConfig.updateCloserTime = updateCloserTime;
        msg.Broadcast.RaceConfig.raceStartInTime = raceStartInTime;

        ESP_LOGI(TAG,"Send: MSG_RACE_CONFIG MSG:0x%x filename:%s name:%s distace:%d laps:%d blockNewLapTime:%d updateCloserTime:%d, raceStartInTime:%d",
        msg.Broadcast.RaceConfig.header.msgType, msg.Broadcast.RaceConfig.fileName, msg.Broadcast.RaceConfig.name,msg.Broadcast.RaceConfig.distance, msg.Broadcast.RaceConfig.laps, 
        msg.Broadcast.RaceConfig.blockNewLapTime, msg.Broadcast.RaceConfig.updateCloserTime, msg.Broadcast.RaceConfig.raceStartInTime);

        BaseType_t xReturned = xQueueSend(queue, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 )); // TODO add resend ?
        if (!xReturned) {
          // it it fails let the user click again
          ESP_LOGW(TAG,"WARNING: Send: MSG_RACE_CONFIG MSG:0x%x could not be sent in 2000ms. USER need to retry", msg.Broadcast.RaceConfig.header.msgType);
        }
    }

    std::string getFileName() {return fileName;}
    std::string getName() {return name;}
    bool isTimeBasedRace() {return timeBasedRace;}
    time_t getMaxTime() {return maxTime;}
    uint32_t getDistance() {return distance;}
    uint32_t getLaps()
    {
      if(laps == 0) {
        laps = 1; // Should never be 0 but if it is lets fix it
      }
      return laps;
    }
    double getLapDistance()
    {
      if (!timeBasedRace) {
        if(laps == 0) {
          laps = 1; // Should never be 0 but if it is lets fix it before the division below
        }
        return static_cast<double>(distance)/static_cast<double>(laps);
      }
      else {
        // In timebased race distance is lap
        return distance;
      }
    }
    time_t getBlockNewLapTime() {return blockNewLapTime;}
    time_t getUpdateCloserTime() {return updateCloserTime;}
    time_t getRaceStartInTime() {return raceStartInTime;}
    time_t getRaceStart() {return raceStart;}
    bool isRaceOngoing() {return raceOngoing;}

    void setFileName(std::string inName) {fileName=inName;}
    void setName(std::string inName) {name=inName;}
    void setTimeBasedRace(bool inTimeBasedRace) {timeBasedRace=inTimeBasedRace;}
    void setMaxTime(time_t inMaxTime) {maxTime=inMaxTime;}
    void setDistance(uint32_t inDist) {distance=inDist;}
    void setLaps(uint32_t inLaps) {laps=inLaps;}
    void setBlockNewLapTime(time_t inTime) {blockNewLapTime=inTime;}
    void setRaceStartInTime(time_t inTime) {raceStartInTime=inTime;}
    void setUpdateCloserTime(time_t inTime) {updateCloserTime=inTime;}

    void setRaceStart(time_t inStart) {raceStart=inStart;}
    void setRaceOngoing(bool race) {raceOngoing=race;}

  private:
    std::string fileName;
    std::string name;
    bool timeBasedRace;
    time_t maxTime;
    uint32_t distance;
    uint32_t laps;
    time_t blockNewLapTime;  // We need to NOT detect the tag for this amout of time before adding a new lap if we the the tag.
    time_t updateCloserTime; // If we get a stronger RSSI signal during this time in seconds after a initial detection, the lap time is updated.
    time_t raceStartInTime;  // Race Start Countdown time
    // Ongoing Race stuff
    bool raceOngoing;
    time_t raceStart;
};

static Race theRace;



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
    participantData(): name("Name"), laps(0), timeCurrentLapFirstDetected(0), timeSinceLastSeen(0) { handleGFX_isValid = false;  inRace = false; updated = false; lapsData.resize(MAX_SAVED_LAPS); clearLaps(); }
    std::string getName() {return name;}
    void setName(std::string inName) {name = inName;}
    uint32_t getLapCount() {return laps;}

    void prevLap()
    {
      if (laps>0) laps--;
      setUpdated();
      refreshTagGUI();
    }

    // usefull when loading a lap and lapstart is not "now"
    bool nextLap(time_t lapStart,time_t lastSeen)
    {
      timeCurrentLapFirstDetected = lapStart;
      if ((laps + 1) < (MAX_SAVED_LAPS)) {
        laps++;
        setCurrentLap(lapStart, lastSeen);
        setUpdated();
        refreshTagGUI();
        return true;
      }
      else {
        //MAX LAP ERROR - Just make last lap extra long
        setCurrentLastSeen(lapStart-getCurrentLapStart());
        setUpdated();
        refreshTagGUI();
        return false;  
      }
    }

    // usefull for triggering a new lap "now" (during race)
    bool nextLap(time_t newLapTime)
    {
      return nextLap(newLapTime, 0);
    }

    // Called when we close to a new lap get stringer RSSI so we assume the new lap is "now" instead of a few seconds ago
    // This is used to get a closer lap time to the unit and try to avoid saving early BT detections.
    // Used together with bestRSSInearNewLap
    void updateLapTagIsCloser(time_t newLapTime)
    {
      setCurrentLap(newLapTime, 0);
      setUpdated();
      refreshTagGUI();
    }

    void clearLaps() {
      laps = 0;
      timeCurrentLapFirstDetected = 0;
      timeSinceLastSeen = 0;
      for (lapData& lap : lapsData) {
        lap.setLap(0,0);
      }
    }

    lapData& getLap(uint32_t lap) { return lapsData.at(lap);}

    time_t getCurrentLapFirstDetected() {return timeCurrentLapFirstDetected;}
    int8_t getBestRSSInearNewLap() {return bestRSSInearNewLap;}
    void setBestRSSInearNewLap(int8_t newRSSI) {bestRSSInearNewLap = newRSSI;}

    time_t getCurrentLapStart() {return lapsData.at(laps).getLapStart();}
    void setCurrentLapStart(time_t timeSinceRaceStart) {lapsData.at(laps).setLapStart(timeSinceRaceStart);}
    time_t getCurrentLastSeen() {return lapsData.at(laps).getLastSeen();}
    time_t getCurrentLastSeenSinceRaceStart() {return lapsData.at(laps).getLapStart() + lapsData.at(laps).getLastSeen();}

    void setCurrentLastSeen(time_t timeSinceLapStart) {lapsData.at(laps).setLastSeen(timeSinceLapStart);}
    void setCurrentLap(time_t timeSinceRaceStart,time_t timeSinceLapStart) {lapsData.at(laps).setLap(timeSinceRaceStart,timeSinceLapStart);}

    uint32_t getTimeSinceLastSeen() {return timeSinceLastSeen;}
    void setTimeSinceLastSeen(time_t inTime) {timeSinceLastSeen=inTime;}

    bool getInRace() {return inRace;}
    void setInRace(bool val) {inRace = val;}

    bool isUpdated() {return updated;}
    void handledUpdate() {updated = false;}
    void setUpdated() {updated = true;}

    bool isHandleGFXValid() { return handleGFX_isValid;}

    uint32_t getHandleGFX() { return handleGFX;}


    void setHandleGFX(uint32_t inHandleGFX, bool valid) {
      handleGFX = inHandleGFX;
      handleGFX_isValid = valid;
    }

  private:
    std::string name;     // Participant name
    uint32_t laps;
    time_t timeCurrentLapFirstDetected; // First time in this lap the tag is ever detected, getCurrentLapStart() might get updated to a theRace.getUpdateCloserTime() seconds after detection if RSSI get "stronger".
    int8_t bestRSSInearNewLap; // Updated theRace.getUpdateCloserTime() seconds after a new lap is detected and used to present a lap time closer to unit in case of early detection (with some smooting maybe?)
    uint32_t timeSinceLastSeen; // in seconds, used to update UI Update when calculated
    std::vector<lapData> lapsData;
    uint32_t handleGFX;
    bool handleGFX_isValid;
    bool inRace;
    bool updated; // use to trigger GUI update
};


class iTag {
  public:
    std::string address;  // BT UUID
    uint32_t color0;      // Color of iTag
    uint32_t color1;      // Color of iTag holder
    int32_t battery;      // 0-100 and -1 when unknown
    bool active;          // As in part of race, has been configured (or on it's way to be)
    bool connected;       // As near enough right now, e.g. spoted recently
  
    participantData participant;
    iTag(std::string inAddress,std::string inName, bool isInRace, uint32_t inColor0, uint32_t inColor1);
    bool UpdateParticipantInGFX();
    bool UpdateParticipantStatusInGUI();
    bool UpdateParticipantStatsInGUI();
    void reset();

    //void saveGUIObjects(lv_obj_t * ledColor0, lv_obj_t * ledColor1, lv_obj_t * labelName, lv_obj_t * labelDist, lv_obj_t * labelLaps, lv_obj_t * labelTime, lv_obj_t * labelConnStatus, /*lv_obj_t * labelBatterySym,*/ lv_obj_t * labelBat);
    int getRSSI() {return RSSI;}
    void setRSSI(int val) {RSSI=val;}
  private:
    int RSSI;
};

#define ITAG_COLOR_PINK     0xfdb9c8 // Lemonade
#define ITAG_COLOR_WHITE    0xeBeEe8 //0xFBFEF8 // Pearl White
#define ITAG_COLOR_ORANGE   0xFA8128 // Tangerine
#define ITAG_COLOR_DARKBLUE 0x1034a6 // Egyptian Blue,    0x0b0b45 // Navy blue
#define ITAG_COLOR_BLACK    0x000000 // Black
#define ITAG_COLOR_GREEN    0xAEF359 // Lime

//TODO update BTUUIDs, names and color, also make name editable from GUI
iTag iTags[ITAG_COUNT] = {
  iTag("ff:ff:10:7e:be:67", "OrangeBlue",   false,  ITAG_COLOR_ORANGE,  ITAG_COLOR_DARKBLUE), //00
  iTag("ff:ff:10:7f:7c:b7", "Zingo0",  true,  ITAG_COLOR_BLACK,   ITAG_COLOR_PINK), //01
  iTag("ff:ff:10:7d:53:fe", "Zingo1",  true, ITAG_COLOR_DARKBLUE,   ITAG_COLOR_PINK),//02
  iTag("ff:ff:10:80:71:e7", "Zingo2",  true, ITAG_COLOR_ORANGE,   ITAG_COLOR_BLACK), //03
  iTag("ff:ff:10:7e:82:46", "Zingo", true,  ITAG_COLOR_ORANGE,  ITAG_COLOR_ORANGE), //04
  iTag("ff:ff:10:7d:d2:08", "BlueOrange",  false,  ITAG_COLOR_DARKBLUE,ITAG_COLOR_ORANGE),  //05
  iTag("ff:ff:10:7e:52:e0", "BlueBlack",    false,  ITAG_COLOR_DARKBLUE,  ITAG_COLOR_BLACK), //06
  iTag("ff:ff:10:7d:96:2a", "WhitePink",  false,  ITAG_COLOR_WHITE,   ITAG_COLOR_PINK), //07
  iTag("ff:ff:10:7f:7a:4e", "BlackWhite",    false,  ITAG_COLOR_BLACK,  ITAG_COLOR_WHITE), //08
  iTag("ff:ff:10:7f:8a:0f", "WhiteOrange",  false, ITAG_COLOR_WHITE,  ITAG_COLOR_ORANGE), //09
  iTag("ff:ff:10:73:66:5f", "PinkBlue",   false, ITAG_COLOR_PINK,ITAG_COLOR_DARKBLUE), //10
  iTag("ff:ff:10:6a:79:b4", "PinkWhite",  false,  ITAG_COLOR_PINK,    ITAG_COLOR_WHITE), //11
  iTag("ff:ff:10:80:73:95", "OrangeWhite", false, ITAG_COLOR_ORANGE,ITAG_COLOR_WHITE), //12
  iTag("ff:ff:10:7f:39:ff", "WhiteBlue",   false,  ITAG_COLOR_WHITE,   ITAG_COLOR_DARKBLUE), //13
  iTag("ff:ff:10:7e:04:4e", "BlueBlue",   false, ITAG_COLOR_DARKBLUE,ITAG_COLOR_DARKBLUE), //14
  iTag("ff:ff:10:74:90:fe", "PinkBlack",  false,  ITAG_COLOR_PINK,    ITAG_COLOR_BLACK), //15
  iTag("ff:ff:10:7f:2f:ee", "BlackOrange",  false, ITAG_COLOR_BLACK,   ITAG_COLOR_ORANGE), //16
  iTag("ff:ff:10:82:ef:1e", "Green",   false, ITAG_COLOR_GREEN,   ITAG_COLOR_GREEN)   //17 Light green BT4
};


static void AddParticipantToGFX(uint32_t handleDB, participantData &participant,uint32_t col0, uint32_t col1)
{
  msg_GFX msg;
  msg.AddUser.header.msgType = MSG_GFX_ADD_USER;
  msg.AddUser.handleDB = handleDB;
  msg.AddUser.color0 = col0;
  msg.AddUser.color1 = col1;
  std::string name = participant.getName();
  size_t len = name.copy(msg.AddUser.name, PARTICIPANT_NAME_LENGTH);
  msg.AddUser.name[len] = '\0';
  msg.AddUser.inRace = participant.getInRace();  

  //ESP_LOGI(TAG,"Send: MSG_GFX_ADD_USER MSG:0x%x handleDB:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d",
  //             msg.AddUser.header.msgType, msg.AddUser.handleDB, msg.AddUser.color0, msg.AddUser.color1, msg.AddUser.name, msg.AddUser.inRace);

  BaseType_t xReturned = xQueueSend(queueGFX, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 )); // TODO add resend ? if not participant.isHandleGFXValid() sometimes later
  if (!xReturned) {
    // it it fails er are probably smoked
    ESP_LOGE(TAG,"FATAL ERROR: Send: MSG_GFX_ADD_USER MSG:0x%x handleDB:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d could not be sent in 2000ms. INITIAL SETUP ERROR",
                 msg.AddUser.header.msgType, msg.AddUser.handleDB, msg.AddUser.color0, msg.AddUser.color1, msg.AddUser.name, msg.AddUser.inRace);
    ESP_LOGE(TAG,"----- esp_restart() -----");
    esp_restart();
  }
}

bool iTag::UpdateParticipantInGFX()
{
  msg_GFX msg;
  msg.UpdateUser.header.msgType = MSG_GFX_UPDATE_USER;
  msg.UpdateUser.handleGFX = participant.getHandleGFX();
  msg.UpdateUser.color0 = color0;
  msg.UpdateUser.color1 = color1;
  std::string name = participant.getName();
  size_t len = name.copy(msg.UpdateUser.name, PARTICIPANT_NAME_LENGTH);
  msg.UpdateUser.name[len] = '\0';
  msg.UpdateUser.inRace = participant.getInRace();

  //ESP_LOGI(TAG,"Send: MSG_GFX_UPDATE_USER MSG:0x%x handleGFX:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d",
  //             msg.UpdateUser.header.msgType, msg.UpdateUser.handleGFX, msg.UpdateUser.color0, msg.UpdateUser.color1, msg.UpdateUser.name, msg.UpdateUser.inRace);

  BaseType_t xReturned = xQueueSend(queueGFX, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 )); // TODO add resend ? if not participant.isHandleGFXValid() sometimes later
  if (!xReturned) {
    // it it fails let the user click again
    ESP_LOGW(TAG,"WARNING: Send: MSG_GFX_UPDATE_USER MSG:0x%x handleGFX:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d could not be sent in 2000ms. USER need to retry",
                 msg.UpdateUser.header.msgType, msg.UpdateUser.handleGFX, msg.UpdateUser.color0, msg.UpdateUser.color1, msg.UpdateUser.name, msg.UpdateUser.inRace);
  }
  return xReturned;
}

bool iTag::UpdateParticipantStatusInGUI()
{
  if(participant.isHandleGFXValid())
  {
    msg_GFX msg;
    msg.UpdateStatus.header.msgType = MSG_GFX_UPDATE_USER_STATUS;
    msg.UpdateStatus.handleGFX = participant.getHandleGFX();

    if (active)
    {
      if (connected) {
        if (participant.getTimeSinceLastSeen() < 20) {
          msg.UpdateStatus.connectionStatus = getRSSI();
        }
        else {
          msg.UpdateStatus.connectionStatus = 1;
        }
      }
      else {
          msg.UpdateStatus.connectionStatus = 0;
      }
    }
    else {
          msg.UpdateStatus.connectionStatus = 0;
    }

    msg.UpdateStatus.battery = battery;
    msg.UpdateStatus.inRace = participant.getInRace();

    //ESP_LOGI(TAG,"Send MSG_GFX_UPDATE_USER_STATUS: MSG:0x%x handleGFX:0x%08x connectionStatus:%d battery:%d inRace:%d",
    //            msg.UpdateStatus.header.msgType, msg.UpdateStatus.handleGFX, msg.UpdateStatus.connectionStatus, msg.UpdateStatus.battery, msg.UpdateStatus.inRace);

    BaseType_t xReturned = xQueueSend(queueGFX, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 200 )); // TODO add resend ?
    return xReturned;
  }
  else {
  // TODO ERROR maybe redo AddParticipantToGFX()
    return false;
  }
  return true;
}


bool iTag::UpdateParticipantStatsInGUI()
{
  if(participant.isHandleGFXValid())
  {
    participant.handledUpdate();
    msg_GFX msg;
    msg.UpdateUserData.header.msgType = MSG_GFX_UPDATE_USER_DATA;
    msg.UpdateUserData.handleGFX = participant.getHandleGFX();

    msg.UpdateUserData.distance = participant.getLapCount() * theRace.getLapDistance();
    msg.UpdateUserData.laps = participant.getLapCount();
    msg.UpdateUserData.lastLapTime = participant.getCurrentLapStart();
    msg.UpdateUserData.lastSeenTime = participant.getCurrentLastSeenSinceRaceStart();
    if (active)
    {
      if (connected) {
        if (participant.getTimeSinceLastSeen() < 20) {
          msg.UpdateUserData.connectionStatus = getRSSI();
        }
        else {
          msg.UpdateUserData.connectionStatus = 1;
        }
      }
      else {
          msg.UpdateUserData.connectionStatus = 0;
      }
    }
    else {
          msg.UpdateUserData.connectionStatus = 0;
    }
    msg.UpdateUserData.inRace = participant.getInRace();
    //ESP_LOGI(TAG,"Send MSG_GFX_UPDATE_USER_DATA: MSG:0x%x handleGFX:0x%08x distance:%d laps:%d lastlaptime:%d connectionStatus:%d",
    //            msg.UpdateUserData.header.msgType, msg.UpdateUserData.handleGFX, msg.UpdateUserData.distance, msg.UpdateUserData.laps,
    //            msg.UpdateUserData.lastLapTime, msg.UpdateUserData.connectionStatus);

    BaseType_t xReturned = xQueueSend(queueGFX, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 200 )); // TODO add resend ?
    return xReturned;
  }
  else {
  // TODO ERROR maybe redo AddParticipantToGFX()
    return false;
  }
  return true;
}

iTag::iTag(std::string inAddress, std::string inName, bool isInRace, uint32_t inColor0, uint32_t inColor1)
{
  address = inAddress;
  color0 = inColor0;
  color1 = inColor1;
  battery = -1; //Unknown or Not read yet
  RSSI = -9999;
  active = false;
  connected = false;

  // TODO make sure string is shorter then PARTICIPANT_NAME_LENGTH
  participant.setName(inName);
  participant.setInRace(isInRace);
  participant.clearLaps();
  participant.handledUpdate(); //clear it
}

void iTag::reset()
{
  {
    // TODO clear lap data?
    participant.setCurrentLap(0,0); // TODO set race start so a late tag get full race
    participant.setTimeSinceLastSeen(0);
    active = true;
    participant.setUpdated(); // will trigger GUI update later
  }
}

static void raceCleariTags()
{
  theRace.setRaceStart(0);
  theRace.setRaceOngoing(false);
  for(int j=0; j<ITAG_COUNT; j++)
  {
    iTags[j].participant.clearLaps();
    iTags[j].participant.setCurrentLap(0,0);
    iTags[j].participant.setUpdated();
  }
  refreshTagGUI();
}


static void raceStartiTags(time_t raceStartTime)
{
  theRace.setRaceStart(raceStartTime);
  theRace.setRaceOngoing(true);
  // Make sure all data is cleared
  for(int j=0; j<ITAG_COUNT; j++)
  {
    iTags[j].participant.clearLaps();
    iTags[j].participant.setCurrentLap(0,0);
    iTags[j].participant.setUpdated();
  }
  refreshTagGUI();
  saveRace(); // Queue up a MSG_ITAG_SAVE_RACE
}

static void raceStopiTags()
{
  theRace.setRaceOngoing(false);
  saveRace(); // Queue up a MSG_ITAG_SAVE_RACE
}

static void raceContinueiTags()
{
  theRace.setRaceOngoing(true);
  saveRace(); // Queue up a MSG_ITAG_SAVE_RACE
}

void refreshTagGUI()
{
//  ESP_LOGI(TAG,"----- Active tags: -----");
  for(int j=0; j<ITAG_COUNT; j++)
  {
    if (iTags[j].active && iTags[j].connected) {
      // Check if "long time no see" and "disconnect"
      tm timeNow = rtc.getTimeStruct();
      time_t timeNowfromEpoc = mktime(&timeNow);
      time_t timeFromRaceStart = difftime(timeNowfromEpoc, theRace.getRaceStart());      
      time_t lastSeenSinceStart = iTags[j].participant.getCurrentLapStart() + iTags[j].participant.getCurrentLastSeen();
      uint32_t timeSinceLastSeen = difftime( timeFromRaceStart, lastSeenSinceStart);
      iTags[j].participant.setTimeSinceLastSeen(timeSinceLastSeen);

      if (timeSinceLastSeen > theRace.getBlockNewLapTime()) {
        ESP_LOGI(TAG,"%s Disconnected Time: %s delta %d timeSinceLastSeen: %d", iTags[j].address.c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),iTags[j].participant.getTimeSinceLastSeen(),timeSinceLastSeen);
        iTags[j].connected = false;
      }
      iTags[j].participant.setUpdated();
    }

    if (iTags[j].participant.isUpdated()) {
      iTags[j].UpdateParticipantStatsInGUI();
    }

   // if(iTags[j].active) {
   //   ESP_LOGI(TAG,"Active: %3d/%3d (max:%3d) %s RSSI:%d %3d%% Laps: %5d | %s", iTags[j].participant.getTimeSinceLastSeen(),theRace.getBlockNewLapTime(), longestNonSeen, iTags[j].connected? "#":" ", iTags[j].getRSSI(), iTags[j].battery ,iTags[j].participant.getLapCount() , iTags[j].participant.getName().c_str());
   // }
  }
//  ESP_LOGI(TAG,"------------------------");
}


static void printDirectory(File dir, int numTabs) {
  while (true) {
 
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
//    for (uint8_t i = 0; i < numTabs; i++) {
//      Serial.print('\t');
//    }
    if (entry.isDirectory()) {
      ESP_LOGI(TAG,"  %s/",entry.name());
      printDirectory(entry, numTabs + 1);
    } else {
      ESP_LOGI(TAG,"  %s %d",entry.name(),entry.size());
      // files have sizes, directories do not
    }
    entry.close();
  }
}

static void checkDisk()
{
  unsigned int totalBytes = LittleFS.totalBytes();
  unsigned int usedBytes = LittleFS.usedBytes();
  unsigned int freeBytes  = totalBytes - usedBytes;

  ESP_LOGI(TAG,"File system info: -----------");
  ESP_LOGI(TAG,"Total space     : %d bytes\n",totalBytes);
  ESP_LOGI(TAG,"Total space used: %d bytes\n",usedBytes);
  ESP_LOGI(TAG,"Total space free: %d bytes\n",freeBytes);

  // Open dir folder
  File dir = LittleFS.open("/");
  // Cycle all the content
  printDirectory(dir,0);
  dir.close();
  ESP_LOGI(TAG,"-----------------------------");
}

static void DBloadGlobalConfig()
{
  std::string fileName = std::string("/CrazyCapyTime.json");

  File raceFile = LittleFS.open(fileName.c_str(), "r");
  if (!raceFile) {
    ESP_LOGE(TAG,"LoadGlobalConfig ERROR: LittleFS open(%s) for read failed",fileName.c_str());
    checkDisk(); // just for debug
    ESP_LOGE(TAG,"LoadGlobalConfig ERROR: LittleFS open(%s) for read failed",fileName.c_str());
    return;
  }

  DynamicJsonDocument raceJson(50000);
  DeserializationError err = deserializeJson(raceJson, raceFile);
  raceFile.close();
  if (err) {
    ESP_LOGE(TAG,"LoadGlobalConfig ERROR: deserializeJson() failed with code %s",err.f_str());
    checkDisk(); // just for debug
    return;
  }


  std::string version = raceJson["fileformatversion"];

  if ( ! (version == "0.1")) {
    ESP_LOGE(TAG,"LoadGlobalConfig ERROR fileformatversion=%s != 0.1 (NOK) Try anyway JSON is kind of build for this.",version.c_str());
  }

  std::string appname = raceJson["Appname"]; // "CrazyCapyTime";
  std::string filetype = raceJson["filetype"]; // "globalconfig";

  if ( ! ((appname == "CrazyCapyTime") && (filetype == "filetype"))) {
    ESP_LOGE(TAG,"LoadGlobalConfig ERROR Appname=%s filetype=%s",appname.c_str() ,filetype.c_str());
  }
  std::string currentRace = raceJson["currentRace"];
  theRace.setFileName(currentRace);
}

static void DBsaveGlobalConfig()
{
  DynamicJsonDocument raceJson(50000);
  raceJson["Appname"] = "CrazyCapyTime";
  raceJson["filetype"] = "globalconfig";
  raceJson["fileformatversion"] = "0.1";
  raceJson["currentRace"] = theRace.getFileName();

  std::string fileName = std::string("/CrazyCapyTime.json");
  File raceFile = LittleFS.open(fileName.c_str(), "w");
  if (!raceFile) {
    ESP_LOGE(TAG,"ERROR: LittleFS open(%s,w) for write failed", fileName.c_str());
    checkDisk(); // just for debug
    ESP_LOGE(TAG,"ERROR: LittleFS open(%s,w) for write failed", fileName.c_str());
    return;
  }
  serializeJson(raceJson, raceFile);
  raceFile.close();
}


static void DBloadRace()
{
  uint64_t start_time = micros();

  std::string fileName = std::string("/").append(theRace.getFileName());

  File raceFile = LittleFS.open(fileName.c_str(), "r");
  if (!raceFile) {
    ESP_LOGE(TAG,"ERROR: LittleFS open(%s) for read failed",fileName.c_str());
    checkDisk(); // just for debug
    return;
  }

  DynamicJsonDocument raceJson(50000);  // TODO verify with a maximum Tags/Laps file
  DeserializationError err = deserializeJson(raceJson, raceFile);
  raceFile.close();
  if (err) {
    ESP_LOGE(TAG,"ERROR: deserializeJson() failed with code %s",err.f_str());
    checkDisk(); // just for debug
    return;
  }
  
  //String output = "";
  //serializeJsonPretty(raceJson, output);
  //ESP_LOGI(TAG,"Loaded json:\n%s", output.c_str());
  std::string version = raceJson["fileformatversion"];

  if ( ! (version == "0.3")) {
    ESP_LOGE(TAG,"LoadRace ERROR fileformatversion=%s != 0.3 (NOK) Try anyway JSON is kind of build for this.",version.c_str());
  }
  //ESP_LOGI(TAG,"fileformatversion=%s (OK)",version.c_str());

  std::string name = raceJson["racename"];
  bool raceTimeBased = raceJson["raceTimeBased"] | true;
  time_t raceMaxTime = raceJson["raceMaxTime"] | 24;
  uint32_t raceDist = raceJson["distance"]  | 821;
  uint32_t raceLaps = raceJson["laps"];
  double raceLapDist = raceJson["lapdistance"] | 821;
  uint32_t raceTagCount = raceJson["tags"];

  time_t raceBlockNewLapTime = raceJson["raceBlockNewLapTime"] | 5*60;
  time_t raceStartInTime = raceJson["raceStartInTime"] | 15;
  time_t raceUpdateCloserTime = raceJson["raceUpdateCloserTime"] | 30;


  time_t raceStart = raceJson["start"];
  bool raceOngoing = raceJson["raceOngoing"] | false;

  theRace.setName(name);
  theRace.setTimeBasedRace(raceTimeBased);
  theRace.setMaxTime(raceMaxTime);
  theRace.setDistance(raceDist);
  theRace.setLaps(raceLaps);

  theRace.setBlockNewLapTime(raceBlockNewLapTime);
  theRace.setUpdateCloserTime(raceUpdateCloserTime);
  theRace.setRaceStartInTime(raceStartInTime);

  theRace.setRaceStart(raceStart);
  theRace.setRaceOngoing(raceOngoing);
  if (raceStart > rtc.getEpoch()) {
    // If our clock is older the race jump to that time
    ESP_LOGW(TAG,"LoadRace WARNING race time is after NOW faking a timejump to race time by force");
    rtc.setTime(raceStart,0);
  }


  if ( std::abs(raceLapDist - theRace.getLapDistance()) > 1.0) {
    ESP_LOGE(TAG,"LoadRace ERROR lapdistance=%f != %f (calculated lap distance from dist:%d laps:%d) (NOK) Do nothing",raceLapDist,theRace.getLapDistance(),raceDist,raceLaps);
  } 
  theRace.send_ConfigMsg(queueGFX);

  // Start with clearing UI

  msg_GFX msg;
  msg.Broadcast.RaceStart.header.msgType = MSG_RACE_CLEAR;  // We send this to "Clear data" before countdown, this would be what a user expect
  //ESP_LOGI(TAG,"Send: MSG_RACE_CLEAR MSG:0x%x",msg.Broadcast.RaceStart.header.msgType);
  xQueueSend(queueGFX, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress

  // And Start a race at "correct time" (from file)

  msg.Broadcast.RaceStart.header.msgType = MSG_RACE_START;  // We send this to "Clear data" before countdown, this would be what a user expect
  msg.Broadcast.RaceStart.startTime = raceStart;
  //ESP_LOGI(TAG,"Send: MSG_RACE_START MSG:0x%x startTime:%d",msg.Broadcast.RaceStart.header.msgType,msg.Broadcast.RaceStart.startTime);
  xQueueSend(queueGFX, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress

  if ( raceTagCount > ITAG_COUNT) {
    // TODO make ITAG_COUNT database dynamic? We have like 8MB ram anyway :)
    ESP_LOGW(TAG,"tags=%d is larger then number of supported tags: %d (will only read %d first tags SORRY)",raceTagCount, ITAG_COUNT,ITAG_COUNT);
    raceTagCount = ITAG_COUNT;
  }

  for(int i=0; i<raceTagCount; i++)
  {
    JsonObject tagJson = raceJson["tag"][i];
    if (tagJson == nullptr)
    {
      ESP_LOGE(TAG,"ERROR: File should have at least %d tags, but problem reading tag:%d Stop reading tags SORRY",raceTagCount, i);
      break;
    }
    std::string tagAddress = tagJson["address"]; // -> iTags[i].address;
    uint32_t tagColor0 = tagJson["color0"]; // -> iTags[i].color0;
    uint32_t tagColor1 = tagJson["color1"]; // -> iTags[i].color1;
    //bool tagActive = tagJson["active"]; // -> iTags[i].active;
 
    JsonObject participantJson = tagJson["participant"];
    std::string participantName = participantJson["name"]; // -> iTags[i].participant.getName();
    //uint32_t participantLaps = participantJson["laps"]; // -> iTags[i].participant.getLapCount();
    uint32_t participantTimeSinceLastSeen = participantJson["timeSinceLastSeen"]; // -> iTags[i].participant.getTimeSinceLastSeen();
    bool participantInRace = participantJson["inRace"]; // -> iTags[i].participant.getInRace();

    //ESP_LOGI(TAG,"iTag[%02d] %s (0x%06x 0x%06x) %s Participant: laps:%4d, lastSeen:%8d %s %s",i,tagAddress.c_str(),tagColor0,tagColor1,tagActive?"ACTIVE":"  NO  ",participantLaps,participantTimeSinceLastSeen,participantInRace?"Race":" NO ",participantName.c_str());
    iTags[i].address = tagAddress;
    iTags[i].color0 = tagColor0;
    iTags[i].color1 = tagColor1;
    //iTags[i].active = tagActive;
    iTags[i].participant.setName(participantName);
    //iTags[i].participant.set (participantLaps); //will be handled by the lap loop
    iTags[i].participant.setTimeSinceLastSeen(participantTimeSinceLastSeen);
    iTags[i].participant.setInRace(participantInRace); //TOD do not update correctly
    iTags[i].participant.clearLaps();
    iTags[i].participant.setUpdated(); // TODO triger resend of name and tag color also

    iTags[i].UpdateParticipantInGFX();

    if ( version == "0.1" ) {
      // TODO remove support for 0.1
      for(int lap=0; lap<=MAX_SAVED_LAPS; lap++)
      {
        JsonObject lapJson = tagJson["lap"][lap];
        if (lapJson == nullptr)
        {
          // No more laps saved
          break;
        }
        time_t lapStart = lapJson["StartTime"]; // -> iTags[i].participant.getLap(lap).getLapStart();
        time_t lapLastSeen = lapJson["LastSeen"]; // -> iTags[i].participant.getLap(lap).getLastSeen();
        //ESP_LOGI(TAG,"         lap[%4d] StartTime:%8d, lastSeen:%8d",lap,lapStart,lapLastSeen);
        if (lap==0) {
          iTags[i].participant.setCurrentLap(lapStart,lapLastSeen);
        }
        else {
          iTags[i].participant.nextLap(lapStart, lapLastSeen);
        }
      }

    }
    else {
      for(int lap=0; lap<=MAX_SAVED_LAPS; lap++)
      {
        JsonObject lapJson = participantJson["laps"][lap];
        if (lapJson == nullptr)
        {
          // No more laps saved
          break;
        }
        time_t lapStart = lapJson["StartTime"]; // -> iTags[i].participant.getLap(lap).getLapStart();
        time_t lapLastSeen = lapJson["LastSeen"]; // -> iTags[i].participant.getLap(lap).getLastSeen();
        unsigned long now = rtc.getEpoch();

        if ((raceStart+lapStart+lapLastSeen) > now) {
          // If our clock is older the lapLastSeen jump to that time
          ESP_LOGW(TAG,"LoadRace WARNING race lapLastSeen is after NOW by %d s faking a timejump to race time by force",(raceStart+lapStart+lapLastSeen) - now);
          rtc.setTime((raceStart+lapStart+lapLastSeen),0);
        }

        //ESP_LOGI(TAG,"         lap[%4d] StartTime:%8d, lastSeen:%8d",lap,lapStart,lapLastSeen);
        if (lap==0) {
          iTags[i].participant.setCurrentLap(lapStart,lapLastSeen);
        }
        else {
          iTags[i].participant.nextLap(lapStart, lapLastSeen);
        }
        //delay(10); // allow some time for other stuff if loading a big file
      }
    }
  }
  uint64_t stop_time = micros();
  uint32_t tot_time = stop_time - start_time;
  ESP_LOGI(TAG,"Loaded race as %s time %d us", fileName.c_str(),tot_time );
  delay(20);
  if (theRace.isRaceOngoing()) {
    // Load race was in started state
    ESP_LOGI(TAG,"Loaded race was started when saved");
    refreshTagGUI();
    continueRace(theRace.getRaceStart()); //TODO move to signal
  }
}

static void DBsaveRace()
{
  delay(20);
  uint64_t start_time = micros();
  DynamicJsonDocument raceJson(50000); // TODO verify with a maximum Tags/Laps file

  raceJson["Appname"] = "CrazyCapyTime";
  raceJson["filetype"] = "racedata";
  raceJson["fileformatversion"] = "0.3";
  raceJson["racename"] = theRace.getName();
  raceJson["raceTimeBased"] = theRace.isTimeBasedRace();
  raceJson["raceMaxTime"] = theRace.getMaxTime();
  raceJson["distance"] = theRace.getDistance();
  raceJson["laps"] = theRace.getLaps();
  raceJson["lapdistance"] = theRace.getLapDistance();
  raceJson["tags"] = ITAG_COUNT;
  raceJson["raceBlockNewLapTime"] = theRace.getBlockNewLapTime();
  raceJson["raceUpdateCloserTime"] = theRace.getUpdateCloserTime();
  raceJson["raceStartInTime"] = theRace.getRaceStartInTime();
  raceJson["start"] = theRace.getRaceStart();
  raceJson["raceOngoing"] = theRace.isRaceOngoing();

  JsonArray tagArrayJson = raceJson.createNestedArray("tag");
  for(int i=0; i<ITAG_COUNT; i++)
  {
    JsonObject tagJson = tagArrayJson.createNestedObject();
    tagJson["address"] = iTags[i].address;
    tagJson["color0"] = iTags[i].color0;
    tagJson["color1"] = iTags[i].color1;
    tagJson["active"] = iTags[i].active;
    JsonObject participantJson = tagJson.createNestedObject("participant");
    participantJson["name"] = iTags[i].participant.getName();
    participantJson["laps"] = iTags[i].participant.getLapCount();
    participantJson["timeSinceLastSeen"] = iTags[i].participant.getTimeSinceLastSeen();
    participantJson["inRace"] = iTags[i].participant.getInRace();

    JsonArray lapArrayJson = participantJson.createNestedArray("laps");
    for(int lap=0; lap<=iTags[i].participant.getLapCount(); lap++)
    {
      JsonObject lapJson = lapArrayJson.createNestedObject();
      lapJson["StartTime"] = iTags[i].participant.getLap(lap).getLapStart();
      lapJson["LastSeen"] = iTags[i].participant.getLap(lap).getLastSeen();
    }
  }

  // Print a prettified JSON document to the serial port
  //String output = "";
  //serializeJsonPretty(raceJson, output);
  //ESP_LOGI(TAG,"json: \n%s", output.c_str());

  std::string fileName = std::string("/").append(theRace.getFileName());
  File raceFile = LittleFS.open(fileName.c_str(), "w");
  if (!raceFile) {
    ESP_LOGE(TAG,"ERROR: LittleFS open(%s,w) failed", fileName.c_str());
    checkDisk(); // just for debug
    return;
  }
  serializeJson(raceJson, raceFile);
  raceFile.close();

  uint64_t stop_time = micros();
  uint32_t tot_time = stop_time - start_time;
  ESP_LOGI(TAG,"Saved Race as %s time %d us", fileName.c_str(),tot_time );

//  checkDisk(); // just for debug
}

void vTaskRaceDB( void *pvParameters )
{
  /* The parameter value is expected to be 2 as 2 is passed in the
     pvParameters value in the call to xTaskCreate() below. 
  */
  //configASSERT( ( ( uint32_t ) pvParameters ) == 2 );

  // Add all Participants in race to race page
  for(int handleDB=0; handleDB<ITAG_COUNT; handleDB++)
  {
    // Add Participand to GUI tabs
    // Use index into iTags as the "secret" handleDB
    AddParticipantToGFX(handleDB, iTags[handleDB].participant,iTags[handleDB].color0,iTags[handleDB].color1); 
  }

  ESP_LOGI(TAG,"Setup Race");
  DBloadGlobalConfig();
  DBloadRace();

  // Send Race setup to GUI
  ESP_LOGI(TAG,"Send Race to GUI");
  theRace.send_ConfigMsg(queueGFX);

  int lastAutoSaveMinute = rtc.getMinute();
  bool autoSaveTainted = false;


  for( ;; )
  {
    msg_RaceDB msg;
    if( xQueueReceive(queueRaceDB, &(msg), (TickType_t)portMAX_DELAY) == pdPASS)
    {
      switch(msg.header.msgType) {
        case MSG_ITAG_DETECTED:
        {
          //ESP_LOGI(TAG,"Received: MSG_ITAG_DETECTED");
          // iTag detected
          std::string bleAddress = convertBLEAddressToString(msg.iTag.address);
          bool found = false;
          for(int j=0; j<ITAG_COUNT; j++)
          {
            if (bleAddress == iTags[j].address) {
              //ESP_LOGI(TAG,"Scaning iTAGs MATCH: %s",bleAddress.c_str());
              found = true;

              // First check if TAG needs to be configurated (to not beep when out of range)
              if (!iTags[j].active) {
                ESP_LOGI(TAG,"%s Activate Time: %s", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());                
                // TODO we should not rely on this struct being the same as MSG_ITAG_DETECTED and it should probably be a new struct
                msg.iTag.header.msgType = MSG_ITAG_CONFIG;
                BaseType_t xReturned = xQueueSend(queueBTConnect, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 0 )); //Don't wait if queue is full, just retry next time we scan the tag
                if (xReturned)
                {
                  //Only mark active if it was possible to put in on the queue, if not it will just retry next time we scan the tag
                  iTags[j].active = true;  //TODO tristate, falst->asking->true (only send one msg)
                }
              }

#ifdef ALL_TAGS_TRIGGER_DEFAULT_PARTICIPANT
              // Override and always trigger this participant -> one man race mode use all TAGs
              // This is done AFTER check for MSG_ITAG_CONFIG is sent to ensure every tag is configurated
              //ESP_LOGI(TAG,"####### Spotted TAG:%d but fake it as TAG:%d %s Time: %s", j, DEFAULT_PARTICIPANT, iTags[DEFAULT_PARTICIPANT].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());
              j = DEFAULT_PARTICIPANT;
#else
              //ESP_LOGI(TAG,"####### Spotted %s Time: %s", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());
#endif
              time_t iTagLapTime = msg.iTag.time;
              time_t newLapTime = difftime(iTagLapTime, theRace.getRaceStart());
              iTags[j].setRSSI(msg.iTag.RSSI);
              if (msg.iTag.battery != INT8_MIN) {
                iTags[j].battery = msg.iTag.battery;
              }

              iTags[j].connected = true;
              iTags[j].participant.setTimeSinceLastSeen(0);
              //tm timeNow = rtc.getTimeStruct();
              time_t lastSeenSinceStart = iTags[j].participant.getCurrentLapStart() + iTags[j].participant.getCurrentLastSeen();
              uint32_t timeSinceLastSeen = difftime(newLapTime, lastSeenSinceStart);
              ESP_LOGI(TAG,"%s Connected Time: %s               timeSinceLastSeen: %d = difftime(newLapTime:%d, lastSeenSinceStart:%d) ", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),timeSinceLastSeen,newLapTime,lastSeenSinceStart);

              ESP_LOGI(TAG,"%s Connected Time: %s Check new lap timeSinceLastSeen: %d > theRace.getBlockNewLapTime():%d ?", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),timeSinceLastSeen,theRace.getBlockNewLapTime());
              if (timeSinceLastSeen > theRace.getBlockNewLapTime()) {                
                // New Lap!
                ESP_LOGI(TAG,"%s Connected Time: %s delta %d->%d (%d,%d) NEW LAP", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),newLapTime,timeSinceLastSeen, iTags[j].participant.getCurrentLapStart(), iTags[j].participant.getCurrentLastSeen());
                iTags[j].participant.setBestRSSInearNewLap(msg.iTag.RSSI); // Save RSSI
                if(!iTags[j].participant.nextLap(newLapTime)) {
                  //TODO GUI popup ??
                  ESP_LOGE(TAG,"%s NEW LAP ERROR can't handle more then %d Laps during race", iTags[j].participant.getName().c_str(),iTags[j].participant.getLapCount());
                }
                if (theRace.isRaceOngoing()) {
                  // Save every lap (is this to aggresive?)
                  saveRace(); // Queue up a MSG_ITAG_SAVE_RACE
                }
              }
              else {
                uint32_t timeSinceThisLap = difftime(newLapTime, iTags[j].participant.getCurrentLapFirstDetected());
                
                // theRace.getUpdateCloserTime() seconds after first BT detection, we update the Lap time if we get stringer signal (typical 30s)
                if (timeSinceThisLap <= theRace.getUpdateCloserTime())
                {
                  // We are within the grace period from BT first detected
                  // If saved RSSI is better then iTags[j].participant.getBestRSSInearNewLap() then update lap
                  if (msg.iTag.RSSI > iTags[j].participant.getBestRSSInearNewLap() ) //TODO Maybe add some margin of better like 5%
                  {
                    // We are withing grace period and RSS was better -> update lap!
                    iTags[j].participant.setBestRSSInearNewLap(msg.iTag.RSSI);
                    iTags[j].participant.updateLapTagIsCloser(newLapTime);
                  }
                }
                time_t newLastSeenSinceLapStart = difftime(newLapTime, iTags[j].participant.getCurrentLapStart());
                ESP_LOGI(TAG,"%s Connected Time: %s delta %d->%d (%d,%d) %d To early", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),newLapTime,timeSinceLastSeen,iTags[j].participant.getCurrentLapStart(), iTags[j].participant.getCurrentLastSeen(),newLastSeenSinceLapStart);
                iTags[j].participant.setCurrentLastSeen(newLastSeenSinceLapStart);
              }
              autoSaveTainted = true;
              iTags[j].participant.setUpdated(); // Make it redraw when GUI loop looks at it
              iTags[j].UpdateParticipantStatusInGUI();
              
              break; // No need to check more TAGs if we got a match
            }
          }
          if (found == false) {
            ESP_LOGW(TAG,"Scaning iTAGs NO MATCH: %s",bleAddress.c_str());
          }
          break;
        }
        case MSG_ITAG_CONFIGURED:
        {
          ESP_LOGI(TAG,"Received: MSG_ITAG_CONFIGURED");

          // iTag active
          std::string bleAddress = convertBLEAddressToString(msg.iTag.address);
          for(int j=0; j<ITAG_COUNT; j++)
          {
            // TODO send index? so we don't need the string compare here???
            if(bleAddress == iTags[j].address) {
              //time_t newLapTime = msg.iTag.time;
              iTags[j].setRSSI(msg.iTag.RSSI);
              if (msg.iTag.battery != INT8_MIN) {
                iTags[j].battery = msg.iTag.battery;
              }
              iTags[j].participant.setTimeSinceLastSeen(0);
              iTags[j].active = true;
              iTags[j].UpdateParticipantStatusInGUI();
            }
          }          
          break;
        }
        case MSG_ITAG_GFX_ADD_USER_RESPONSE:
        {
          //ESP_LOGI(TAG,"Received: MSG_ITAG_GFX_ADD_USER_RESPONSE MSG:0x%x handleDB:0x%08x handleGFX:0x%08x wasOK:%d", 
          //     msg.AddedToGFX.header.msgType, msg.AddedToGFX.handleDB, msg.AddedToGFX.handleGFX, msg.AddedToGFX.wasOK);
          iTags[msg.AddedToGFX.handleDB].participant.setHandleGFX(msg.AddedToGFX.handleGFX, msg.AddedToGFX.wasOK);
          break;
        }
        case MSG_ITAG_UPDATE_USER:
        {
          ESP_LOGI(TAG,"Received: MSG_ITAG_UPDATE_USER MSG:0x%x handleDB:0x%08x handleGFX:0x%08x inRace:%d", 
               msg.UpdateParticipantRaceStatus.header.msgType, msg.UpdateParticipantRaceStatus.handleDB, msg.UpdateParticipantRaceStatus.handleGFX, msg.UpdateParticipantRaceStatus.inRace);

          uint32_t handleDB = msg.UpdateParticipant.handleDB;
          iTags[handleDB].participant.setHandleGFX(msg.UpdateParticipant.handleGFX, true);
          iTags[handleDB].color0 = msg.UpdateParticipant.color0;
          iTags[handleDB].color1 = msg.UpdateParticipant.color1;
          iTags[handleDB].participant.setName(msg.UpdateParticipant.name);
          iTags[handleDB].participant.setInRace(msg.UpdateParticipant.inRace);
          // Send update to GUI
          iTags[handleDB].UpdateParticipantInGFX();

          break;
        }
        case MSG_ITAG_UPDATE_USER_RACE_STATUS:
        {
          uint32_t handleDB = msg.UpdateParticipantRaceStatus.handleDB;
          //ESP_LOGI(TAG,"Received: MSG_ITAG_UPDATE_USER_RACE_STATUS MSG:0x%x handleDB:0x%08x handleGFX:0x%08x inRace:%d ? myinRace:%d", 
          //     msg.UpdateParticipantRaceStatus.header.msgType, msg.UpdateParticipantRaceStatus.handleDB, msg.UpdateParticipantRaceStatus.handleGFX, msg.UpdateParticipantRaceStatus.inRace,iTags[handleDB].participant.getInRace());
          if (iTags[handleDB].participant.getInRace() !=  msg.UpdateParticipantRaceStatus.inRace)
          {
            // In race changed, update
            iTags[handleDB].participant.setInRace(msg.UpdateParticipantRaceStatus.inRace);
            // Send update to GUI
            iTags[handleDB].UpdateParticipantStatusInGUI();
          }
          break;
        }
        case MSG_ITAG_UPDATE_USER_LAP_COUNT:
        {
          uint32_t handleDB = msg.UpdateParticipantLapCount.handleDB;
          ESP_LOGI(TAG,"Received: MSG_ITAG_UPDATE_USER_LAP_COUNT MSG:0x%x handleDB:0x%08x handleGFX:0x%08x lapDiff:%d", 
               msg.UpdateParticipantLapCount.header.msgType, msg.UpdateParticipantLapCount.handleDB, msg.UpdateParticipantLapCount.handleGFX, msg.UpdateParticipantLapCount.lapDiff);
  
          // We add/sub the laps one at a time to make sure all is handled
          // If we ever start adding/removing large amount of laps this can be reworked but
          // this should typical (probably always?) only be +1,-1
          int32_t lapDiff = msg.UpdateParticipantLapCount.lapDiff;
          if (lapDiff < 0) {
            // Negative remove laps
            ESP_LOGI(TAG," Negative diff %d laps",lapDiff);
            for(int i = 0; i > lapDiff; i--)
            {
              ESP_LOGI(TAG," Removing %d/%d laps",i, lapDiff);
              iTags[handleDB].participant.prevLap();
            }
            if (theRace.isRaceOngoing()) {
              saveRace(); // Queue up a MSG_ITAG_SAVE_RACE
            }
          }
          else  if (lapDiff > 0) {
            // Positive add laps
            ESP_LOGI(TAG," Positive diff %d laps",lapDiff);
            for(int i = 0; i < lapDiff; i++)
            {
              ESP_LOGI(TAG," Adding %d/%d laps",i, lapDiff);
              tm timeNow = rtc.getTimeStruct();
              time_t lapStart = difftime(mktime(&timeNow), theRace.getRaceStart());
              // TODO Now this will add a "lap block" so this ONLY works when participant is in "LAP AREA"
              // TODO maybe something like      time_t newLapTime = mktime(&timeNow) - theRace.getBlockNewLapTime(); // remove theRace.getBlockNewLapTime() to make it possible to detect next lap directly
              iTags[handleDB].participant.nextLap(lapStart,0);
            }
            if (theRace.isRaceOngoing()) {
              saveRace(); // Queue up a MSG_ITAG_SAVE_RACE
            }
          }
          else {
            ESP_LOGW(TAG,"Received: MSG_ITAG_UPDATE_USER_LAP_COUNT MSG:0x%x handleDB:0x%08x handleGFX:0x%08x lapDiff:%d = 0 -> Do nothing",
                msg.UpdateParticipantLapCount.header.msgType, msg.UpdateParticipantLapCount.handleDB, msg.UpdateParticipantLapCount.handleGFX, msg.UpdateParticipantLapCount.lapDiff);
          }
          break;
        }
        case MSG_ITAG_LOAD_RACE:
        {
          ESP_LOGI(TAG,"Received: MSG_ITAG_LOAD_RACE MSG:0x%x", msg.LoadRace.header.msgType);
          lastAutoSaveMinute = rtc.getMinute(); // Reset autosave timer
          autoSaveTainted = false;
          DBloadRace();
          break;
        }
        case MSG_ITAG_SAVE_RACE:
        {
          ESP_LOGI(TAG,"Received: MSG_ITAG_SAVE_RACE MSG:0x%x", msg.SaveRace.header.msgType);
          lastAutoSaveMinute = rtc.getMinute(); // Reset autosave timer
          autoSaveTainted = false;
          DBsaveRace();
          break;
        }
        case MSG_ITAG_TIMER_2000:
        {
          // update GUI and handle the check if "long time no see" and "disconnect" status
          // This is used to not accedently count a lap in "too short laps"
          refreshTagGUI();

          if (theRace.isRaceOngoing()) {
            time_t now = rtc.getEpoch();
            if ( (theRace.getRaceStart() + theRace.getMaxTime()*60*60 ) < now ) {
              // Race is done
              theRace.setRaceOngoing(false);
              stopRace(); //TODO signal/message
            }
          }

          int nowMinute = rtc.getMinute();
          if (lastAutoSaveMinute != nowMinute) {
            // Autosave each 5min if tainted (e.g. something changed)
            if (autoSaveTainted && nowMinute % 5 == 0) {
              lastAutoSaveMinute = nowMinute;
              autoSaveTainted = false;
              DBsaveRace();
            }
          }
          break;
        }
        // Broadcast Messages
        case MSG_RACE_START:
        {
          ESP_LOGI(TAG,"Received: MSG_RACE_START MSG:0x%x startTime:%d", msg.Broadcast.RaceStart.header.msgType,msg.Broadcast.RaceStart.startTime);
          lastAutoSaveMinute = rtc.getMinute(); // Reset autosave timer
          autoSaveTainted = true;
          raceStartiTags(msg.Broadcast.RaceStart.startTime);
          break;
        }
        case MSG_RACE_STOP:
        {
          ESP_LOGI(TAG,"Received: MSG_RACE_STOP MSG:0x%x", msg.Broadcast.RaceStop.header.msgType);
          lastAutoSaveMinute = rtc.getMinute(); // Reset autosave timer
          autoSaveTainted = true;
          raceStopiTags();
          break;
        }
        case MSG_RACE_CLEAR:
        {
          ESP_LOGI(TAG,"Received: MSG_RACE_CLEAR MSG:0x%x", msg.Broadcast.RaceStart.header.msgType);
          lastAutoSaveMinute = rtc.getMinute(); // Reset autosave timer
          autoSaveTainted = true;
          raceCleariTags();
          break;
        }
        case MSG_RACE_CONFIG:
        {
          ESP_LOGI(TAG,"Received: MSG_RACE_CONFIG MSG:0x%x", msg.Broadcast.RaceConfig.header.msgType);
          theRace.receive_ConfigMsg(&msg.Broadcast.RaceConfig);
          theRace.send_ConfigMsg(queueGFX); //Make sure GUI is in sync
          break;
        }
        default:
          ESP_LOGE(TAG,"ERROR received bad msg: 0x%x",msg.header.msgType);
          break;
      }
    }
  }
  vTaskDelete( NULL ); // Should never be reached
}

// WARNING Executes in the timer deamon contex, NO blocking and NO touching of our data, we will just send a msg to out thread and handle everything there.
void vTaskRaceDBTimer2000( TimerHandle_t xTimer )
{
  //Send a tick message to our message queue to do all work in our own thread
  msg_RaceDB msg;
  msg.Timer.header.msgType = MSG_ITAG_TIMER_2000;
  //ESP_LOGI(TAG,"Send: MSG_ITAG_TIMER_2000 MSG:0x%x", msg.Timer.header.msgType);
  BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)0); //No blocking
  if( xReturned != pdPASS )
  {
    ESP_LOGW(TAG,"WARNING: Send: MSG_ITAG_TIMER_2000 MSG:0x%x Failed, do nothing, we try again in 2000ms", msg.Timer.header.msgType);
  }
}

void initRaceDB()
{
  // Start iTag Task
  BaseType_t xReturned;
  /* Create the task, storing the handle. */
  xReturned = xTaskCreate(
                  vTaskRaceDB,       /* Function that implements the task. */
                  "RaceDB",          /* Text name for the task. */
                  TASK_RACEDB_STACK, /* Stack size in words, not bytes. */
                  NULL,              /* Parameter passed into the task. */
                  TASK_RACEDB_PRIO,  /* Priority  0-(configMAX_PRIORITIES-1)   idle = 0 = tskIDLE_PRIORITY*/
                  &xHandleRaceDB );  /* Used to pass out the created task's handle. */

  if( xReturned != pdPASS )
  {
    ESP_LOGE(TAG,"FATAL ERROR: xTaskCreate(vTaskRaceDB, RaceDB,..) Failed");
    ESP_LOGE(TAG,"----- esp_restart() -----");
    esp_restart();
  }

  TimerHandle_t timerHandle = xTimerCreate("RaceDBTimer2000", pdMS_TO_TICKS(2000),pdTRUE, (void *) 0, vTaskRaceDBTimer2000);
  if( timerHandle == NULL ) {
    ESP_LOGE(TAG,"FATAL ERROR: xTimerCreate(RaceDBTimer2000,..) Failed");
    ESP_LOGE(TAG,"----- esp_restart() -----");
    esp_restart();
  }

  if( xTimerStart( timerHandle, pdMS_TO_TICKS(2000) ) != pdPASS ) {
    ESP_LOGE(TAG,"FATAL ERROR: xTimerStart(RaceDBTimer2000) Failed");
    ESP_LOGE(TAG,"----- esp_restart() -----");
    esp_restart();
  }
}
