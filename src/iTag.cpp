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

    // Called when we clsoe to a new lap get stringer RSSI so we assume the new lap is "now" instead of a few seconds ago
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
    time_t timeCurrentLapFirstDetected; // First time in this lap the tag is ever detected, getCurrentLapStart() might get updated to a LAP_UPDATED_GRACE_PERIOD seconds after detection if RSSI get "stronger".
    int8_t bestRSSInearNewLap; // Updated LAP_UPDATED_GRACE_PERIOD seconds after a new lap is detected and used to present a lap time closer to unit in case of early detection (with some smooting maybe?)
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
#define ITAG_COLOR_WHITE    0xFBFEF8 // Pearl White
#define ITAG_COLOR_ORANGE   0xFA8128 // Tangerine
#define ITAG_COLOR_DARKBLUE 0x1034a6 // Egyptian Blue,    0x0b0b45 // Navy blue
#define ITAG_COLOR_BLACK    0x000000 // Black
#define ITAG_COLOR_GREEN    0xAEF359 // Lime

//TODO update BTUUIDs, names and color, also make name editable from GUI
iTag iTags[ITAG_COUNT] = {
  iTag("ff:ff:10:7f:7c:b7", "Johan",   true,  ITAG_COLOR_BLACK,   ITAG_COLOR_PINK),
  iTag("ff:ff:10:7e:82:46", "Johanna", true,  ITAG_COLOR_ORANGE,  ITAG_COLOR_ORANGE),
  iTag("ff:ff:10:74:90:fe", "Niklas",  true,  ITAG_COLOR_PINK,    ITAG_COLOR_BLACK),
  iTag("ff:ff:10:7f:39:ff", "Pavel",   true,  ITAG_COLOR_WHITE,   ITAG_COLOR_DARKBLUE),
  iTag("ff:ff:10:7d:d2:08", "Stefan",  true,  ITAG_COLOR_DARKBLUE,ITAG_COLOR_ORANGE),  //02
  iTag("ff:ff:10:7e:be:67", "Zingo",   true,  ITAG_COLOR_ORANGE,  ITAG_COLOR_DARKBLUE), //01
  iTag("ff:ff:10:7e:52:e0", "Tony",    false,  ITAG_COLOR_DARKBLUE,  ITAG_COLOR_BLACK),
  iTag("ff:ff:10:7d:96:2a", "Markus",  false,  ITAG_COLOR_WHITE,   ITAG_COLOR_PINK),
  iTag("ff:ff:10:7f:7a:4e", "Black1",    false,  ITAG_COLOR_BLACK,  ITAG_COLOR_WHITE),
  iTag("ff:ff:10:7f:8a:0f", "White1",  false, ITAG_COLOR_WHITE,  ITAG_COLOR_ORANGE),
  iTag("ff:ff:10:73:66:5f", "Pink1",   false, ITAG_COLOR_PINK,ITAG_COLOR_DARKBLUE),
  iTag("ff:ff:10:6a:79:b4", "Pink2",  false,  ITAG_COLOR_PINK,    ITAG_COLOR_WHITE),
  iTag("ff:ff:10:80:73:95", "Orange1", false, ITAG_COLOR_ORANGE,ITAG_COLOR_WHITE),
  iTag("ff:ff:10:80:71:e7", "Orange2",  false, ITAG_COLOR_ORANGE,   ITAG_COLOR_BLACK),
  iTag("ff:ff:10:7e:04:4e", "Blue1",   false, ITAG_COLOR_DARKBLUE,ITAG_COLOR_DARKBLUE),
  iTag("ff:ff:10:7d:53:fe", "Blue2",  false, ITAG_COLOR_DARKBLUE,   ITAG_COLOR_PINK),//---
  iTag("ff:ff:10:7f:2f:ee", "Black2",  false, ITAG_COLOR_BLACK,   ITAG_COLOR_ORANGE),
  iTag("ff:ff:10:82:ef:1e", "Green",   false, ITAG_COLOR_GREEN,   ITAG_COLOR_GREEN)     //Light green BT4
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

    msg.UpdateUserData.distance = participant.getLapCount()* RACE_DISTANCE_LAP;
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

iTag::iTag(std::string inAddress,std::string inName, bool isInRace, uint32_t inColor0, uint32_t inColor1)
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

static uint32_t longestNonSeen = 0; // debug

static void raceCleariTags()
{
  longestNonSeen = 0;
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
  longestNonSeen = 0;
  for(int j=0; j<ITAG_COUNT; j++)
  {
    iTags[j].participant.setCurrentLap(raceStartTime,0);
    iTags[j].participant.setUpdated();
  }
  refreshTagGUI();
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
      time_t lastSeenSinceStart = iTags[j].participant.getCurrentLapStart() + iTags[j].participant.getCurrentLastSeen();
      uint32_t timeSinceLastSeen = difftime( timeNowfromEpoc, lastSeenSinceStart);
      iTags[j].participant.setTimeSinceLastSeen(timeSinceLastSeen);
    if (longestNonSeen <  timeSinceLastSeen) {
      longestNonSeen = timeSinceLastSeen;
    }


      if (timeSinceLastSeen > MINIMUM_LAP_TIME_IN_SECONDS) {
        ESP_LOGI(TAG,"%s Disconnected Time: %s delta %d", iTags[j].address.c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),iTags[j].participant.getTimeSinceLastSeen());
        iTags[j].connected = false;
      }
      iTags[j].participant.setUpdated();
    }

    if (iTags[j].participant.isUpdated()) {
      iTags[j].UpdateParticipantStatsInGUI();
    }

   // if(iTags[j].active) {
   //   ESP_LOGI(TAG,"Active: %3d/%3d (max:%3d) %s RSSI:%d %3d%% Laps: %5d | %s", iTags[j].participant.getTimeSinceLastSeen(),MINIMUM_LAP_TIME_IN_SECONDS, longestNonSeen, iTags[j].connected? "#":" ", iTags[j].getRSSI(), iTags[j].battery ,iTags[j].participant.getLapCount() , iTags[j].participant.getName().c_str());
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
  unsigned int freeBytes  = totalBytes - freeBytes;

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


static void loadRace()
{
  checkDisk(); // just for debug

  std::string fileName = std::string("/RaceData2.json");

  File raceFile = LittleFS.open(fileName.c_str(), "r");
  if (!raceFile) {
    ESP_LOGE(TAG,"ERROR: LittleFS open(%s) failed",fileName.c_str());
    return;
  }

  DynamicJsonDocument raceJson(50000);  // TODO verify with a maximum Tags/Laps file
  DeserializationError err = deserializeJson(raceJson, raceFile);
  raceFile.close();
  if (err) {
    ESP_LOGE(TAG,"ERROR: deserializeJson() failed with code %s",err.f_str());
    return;
  }
  

  String output = "";
  serializeJsonPretty(raceJson, output);
  ESP_LOGI(TAG,"Loaded json:\n%s", output.c_str());
  std::string version = raceJson["fileformatversion"];

  if ( ! (version == "0.1" || version == "0.2")) {   //TODO remove support for 0.1
    ESP_LOGI(TAG,"fileformatversion=%s != 0.1 (NOK) Do nothing",version.c_str());
    return;
  }
  //ESP_LOGI(TAG,"fileformatversion=%s (OK)",version.c_str());

  time_t raceStart = raceJson["start"];
  uint32_t raceDist = raceJson["distance"];
  uint32_t raceLaps = raceJson["laps"];
  double raceLapDist = raceJson["lapsdistance"];
  uint32_t raceTagCount = raceJson["tags"];

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
    bool tagActive = tagJson["active"]; // -> iTags[i].active;
 
    JsonObject participantJson = tagJson["participant"];
    std::string participantName = participantJson["name"]; // -> iTags[i].participant.getName();
    uint32_t participantLaps = participantJson["laps"]; // -> iTags[i].participant.getLapCount();
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
        //ESP_LOGI(TAG,"         lap[%4d] StartTime:%8d, lastSeen:%8d",lap,lapStart,lapLastSeen);
        if (lap==0) {
          iTags[i].participant.setCurrentLap(lapStart,lapLastSeen);
        }
        else {
          iTags[i].participant.nextLap(lapStart, lapLastSeen);
        }
      }
    }
  }
}

static void saveRace()
{
  DynamicJsonDocument raceJson(50000); // TODO verify with a maximum Tags/Laps file
  //JsonObject raceJson = jsonDoc.createObject();
  //char JSONmessageBuffer[200];

  raceJson["Appname"] = "CrazyCapyTime";
  raceJson["filetype"] = "racedata";
  raceJson["fileformatversion"] = "0.2";
  raceJson["start"] = 0;
  raceJson["distance"] = RACE_DISTANCE_TOTAL;
  raceJson["laps"] = RACE_LAPS;
  raceJson["lapsdistance"] = RACE_DISTANCE_LAP;
  raceJson["tags"] = ITAG_COUNT;

  JsonArray tagArrayJson = raceJson.createNestedArray("tag");
  for(int i=0; i<ITAG_COUNT; i++)
  {
    //JsonObject tagJson = raceJson.createNestedObject("tag");
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


  checkDisk(); // just for debug

  std::string fileName = std::string("/RaceData3.json");
  File raceFile = LittleFS.open(fileName.c_str(), "w");
  if (!raceFile) {
    ESP_LOGE(TAG,"ERROR: LittleFS open(%s,w) failed", fileName.c_str());
    return;
  }
  serializeJson(raceJson, raceFile);
  raceFile.close();

  checkDisk(); // just for debug

}

void vTaskRaceDB( void *pvParameters )
{
  /* The parameter value is expected to be 2 as 2 is passed in the
     pvParameters value in the call to xTaskCreate() below. 
  */
  configASSERT( ( ( uint32_t ) pvParameters ) == 2 );

  // Add all Participants in race to race page
  for(int handleDB=0; handleDB<ITAG_COUNT; handleDB++)
  {
    // Add Participand to GUI tabs
    // Use index into iTags as the "secret" handleDB
    AddParticipantToGFX(handleDB, iTags[handleDB].participant,iTags[handleDB].color0,iTags[handleDB].color1); 
  }

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
              found = true;
              //ESP_LOGI(TAG,"Scaning iTAGs MATCH: %s",String(advertisedDevice->toString().c_str()).c_str());
              //ESP_LOGI(TAG,"####### Spotted %s Time: %s", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());

              time_t newLapTime = msg.iTag.time;
              iTags[j].setRSSI(msg.iTag.RSSI);
              if (msg.iTag.battery != INT8_MIN) {
                iTags[j].battery = msg.iTag.battery;
              }

              iTags[j].connected = true;
              iTags[j].participant.setTimeSinceLastSeen(0);
              //tm timeNow = rtc.getTimeStruct();
              time_t lastSeenSinceStart = iTags[j].participant.getCurrentLapStart() + iTags[j].participant.getCurrentLastSeen();
              uint32_t timeSinceLastSeen = difftime(newLapTime, lastSeenSinceStart);

              if (timeSinceLastSeen > MINIMUM_LAP_TIME_IN_SECONDS) {
                
                // New Lap!
                //ESP_LOGI(TAG,"%s Connected Time: %s delta %d->%d (%d,%d) NEW LAP", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),newLapTime,timeSinceLastSeen, iTags[j].participant.getCurrentLapStart(), iTags[j].participant.getCurrentLastSeen());
                iTags[j].participant.setBestRSSInearNewLap(msg.iTag.RSSI); // Save RSSI
                if(!iTags[j].participant.nextLap(newLapTime)) {
                  //TODO GUI popup ??
                  ESP_LOGE(TAG,"%s NEW LAP ERROR can't handle more then %d Laps during race", iTags[j].participant.getName().c_str(),iTags[j].participant.getLapCount());
                }
              }
              else {
                uint32_t timeSinceThisLap = difftime(newLapTime, iTags[j].participant.getCurrentLapFirstDetected());
                if (timeSinceThisLap <= LAP_UPDATED_GRACE_PERIOD)
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
                time_t newLastSeenSinceLapStart = difftime(newLapTime,iTags[j].participant.getCurrentLapStart());
                //ESP_LOGI(TAG,"%s Connected Time: %s delta %d->%d (%d,%d) %d To early", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),newLapTime,timeSinceLastSeen,iTags[j].participant.getCurrentLapStart(), iTags[j].participant.getCurrentLastSeen(),newLastSeenSinceLapStart);
                iTags[j].participant.setCurrentLastSeen(newLastSeenSinceLapStart);
              }

              iTags[j].participant.setUpdated(); // Make it redraw when GUI loop looks at it
              iTags[j].UpdateParticipantStatusInGUI();
              
              if (!iTags[j].active) {
                ESP_LOGI(TAG,"%s Activate Time: %s", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());
                
                // TODO we should not rely on this struct being the same and it should probably be a new struct
                msg.iTag.header.msgType = MSG_ITAG_CONFIG;
                BaseType_t xReturned = xQueueSend(queueBTConnect, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 0 )); //Don't wait if queue is full, just retry next time we scan the tag
                if (xReturned)
                {
                  //Only mark active if it was possible to put in on the queue, if not it will just retry next time we scan the tag
                  iTags[j].active = true;  //TODO tristate, falst->asking->true (only send one msg)
                  //iTags[j].reset(); // will set active = true
                }
              }
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
                            time_t newLapTime = msg.iTag.time;
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
          //ESP_LOGI(TAG,"Received: MSG_ITAG_UPDATE_USER MSG:0x%x handleDB:0x%08x handleGFX:0x%08x inRace:%d ? myinRace:%d", 
          //     msg.UpdateParticipantRaceStatus.header.msgType, msg.UpdateParticipantRaceStatus.handleDB, msg.UpdateParticipantRaceStatus.handleGFX, msg.UpdateParticipantRaceStatus.inRace,iTags[handleDB].participant.getInRace());

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
          }
          else  if (lapDiff > 0) {
            // Positive add laps
            ESP_LOGI(TAG," Positive diff %d laps",lapDiff);
            for(int i = 0; i < lapDiff; i++)
            {
              ESP_LOGI(TAG," Adding %d/%d laps",i, lapDiff);
              tm timeNow = rtc.getTimeStruct();
              time_t newLapTime = mktime(&timeNow) - MINIMUM_LAP_TIME_IN_SECONDS; // remove MINIMUM_LAP_TIME_IN_SECONDS to make it possible to detect next lap directly
              iTags[handleDB].participant.nextLap(newLapTime,0);
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
          loadRace();
          break;
        }
        case MSG_ITAG_SAVE_RACE:
        {
          ESP_LOGI(TAG,"Received: MSG_ITAG_SAVE_RACE MSG:0x%x", msg.SaveRace.header.msgType);
          saveRace();
          break;
        }
        case MSG_ITAG_TIMER_2000:
        {
          // update GUI and handle the check if "long time no see" and "disconnect" status
          // This is used to not accedently count a lap in "too short laps"
          refreshTagGUI();
          //rtc.setTime(rtc.getEpoch()+30,0); //DEBUG fake faster time for testing REMOVE
          break;
        }
        // Broadcast Messages
        case MSG_RACE_START:
        {
          ESP_LOGI(TAG,"Received: MSG_RACE_START MSG:0x%x startTime:%d", msg.Broadcast.RaceStart.header.msgType,msg.Broadcast.RaceStart.startTime);
          raceStartiTags(msg.Broadcast.RaceStart.startTime);
          break;
        }
        case MSG_RACE_CLEAR:
        {
          ESP_LOGI(TAG,"Received: MSG_RACE_CLEAR MSG:0x%x", msg.Broadcast.RaceStart.header.msgType);
          raceCleariTags();
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
  //ESP_LOGI(TAG,"Send: MSG_ITAG_TIMER_2000 MSG:0x%x handleDB:0x%08x", msg.Timer.header.msgType);
  BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)0); //No blocking
  if( xReturned != pdPASS )
  {
    ESP_LOGW(TAG,"WARNING: Send: MSG_ITAG_TIMER_2000 MSG:0x%x handleDB:0x%08x Failed, do nothing, we try again in 2000ms", msg.Timer.header.msgType);
  }
}

void initRaceDB()
{
  // Start iTag Task
  BaseType_t xReturned;
  TaskHandle_t xHandle = NULL;
  /* Create the task, storing the handle. */
  xReturned = xTaskCreate(
                  vTaskRaceDB,       /* Function that implements the task. */
                  "RaceDB",          /* Text name for the task. */
                  4096,              /* Stack size in words, not bytes. */
                  ( void * ) 2,      /* Parameter passed into the task. */
                  5,                 /* Priority  0-(configMAX_PRIORITIES-1)   idle = 0 = tskIDLE_PRIORITY*/
                  &xHandle );       /* Used to pass out the created task's handle. */

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
