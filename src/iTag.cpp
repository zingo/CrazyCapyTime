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


static std::mutex mutexTags; // Lock when access runtime writable data in any tag TODO make one mutex per tag?


bool AddParticipantToGFX(uint32_t handleDB, participantData &participant,uint32_t col0, uint32_t col1)
{
  msg_GFX msg;
  msg.Add.header.msgType = MSG_GFX_ADD_USER_TO_RACE;
  msg.Add.handleDB = handleDB;
  msg.Add.color0 = col0;
  msg.Add.color1 = col1;
  std::string name = participant.getName();
  size_t len = name.copy(msg.Add.name, PARTICIPANT_NAME_LENGTH);
  msg.Add.name[len] = '\0';
  msg.Add.inRace = participant.getInRace();  

  ESP_LOGI(TAG,"Add User: MSG:0x%x handleDB:0x%08x color:(0x%06x,0x%06x) Name:%s inRace:%d",
               msg.Add.header.msgType, msg.Add.handleDB, msg.Add.color0, msg.Add.color1, msg.Add.name, msg.Add.inRace);

  BaseType_t xReturned = xQueueSend(queueGFX, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 )); // TODO add resend ? if not participant.isHandleGFXValid() sometimes later
  return xReturned;
}

bool UpdateParticipantStatusInGFX(iTag &tag)
{
  if(tag.participant.isHandleGFXValid())
  {
    msg_GFX msg;
    msg.UpdateStatus.header.msgType = MSG_GFX_UPDATE_STATUS_USER;
    msg.UpdateStatus.handleGFX = tag.participant.getHandleGFX();

    if (tag.active)
    {
      if (tag.connected) {
        if (tag.participant.getTimeSinceLastSeen() < 20) {
          msg.UpdateStatus.connectionStatus = tag.getRSSI();
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

    msg.UpdateStatus.connectionStatus = tag.getRSSI();
    msg.UpdateStatus.battery = tag.battery;
    msg.UpdateStatus.inRace = tag.participant.getInRace();

    //ESP_LOGI(TAG,"Send MSG_GFX_UPDATE_STATUS_USER: MSG:0x%x handleGFX:0x%08x connectionStatus:%d battery:%d inRace:%d",
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


bool iTag::UpdateParticipantInGFX()
{
  if(participant.isHandleGFXValid())
  {
    msg_GFX msg;
    msg.Update.header.msgType = MSG_GFX_UPDATE_USER;
    msg.Update.handleGFX = participant.getHandleGFX();

    msg.Update.distance = participant.getLapCount()* RACE_DISTANCE_LAP;
    msg.Update.laps = participant.getLapCount();
    msg.Update.lastlaptime = participant.getCurrentLapStart();

    if (active)
    {
      if (connected) {
        if (participant.getTimeSinceLastSeen() < 20) {
          msg.Update.connectionStatus = getRSSI();
        }
        else {
          msg.Update.connectionStatus = 1;
        }
      }
      else {
          msg.Update.connectionStatus = 0;
      }
    }
    else {
          msg.Update.connectionStatus = 0;
    }

    //ESP_LOGI(TAG,"Send MSG_GFX_UPDATE_USER: MSG:0x%x handleGFX:0x%08x distance:%d laps:%d lastlaptime:%d connectionStatus:%d",
    //            msg.Update.header.msgType, msg.Update.handleGFX, msg.Update.distance, msg.Update.laps, msg.Update.lastlaptime,msg.Update.connectionStatus);

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
  updated = false;
}

void iTag::reset()
{
  {
    std::lock_guard<std::mutex> lck(mutexTags);
    // TODO clear lap data?
    participant.setCurrentLap(0,0); // TODO set race start so a late tag get full race
    participant.setTimeSinceLastSeen(0);
    active = true;
    updated = true; // will trigger GUI update later
  }
}

void iTag::updateGUI(void)
{
  std::lock_guard<std::mutex> lck(mutexTags);
  updateGUI_locked();
}

void iTag::updateGUI_locked(void)
{
  UpdateParticipantInGFX();
}


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

uint32_t lastScanTime = 0;
static uint32_t longestNonSeen = 0;


void startRaceiTags()
{
  tm timeNow = rtc.getTimeStruct();
  time_t raceStartTime = mktime (&timeNow); //TODO raceStartTime should be something "global"

  std::lock_guard<std::mutex> lck(mutexTags);
  longestNonSeen = 0;
  for(int j=0; j<ITAG_COUNT; j++)
  {
    iTags[j].updated = true;
    iTags[j].participant.clearLaps();
    iTags[j].participant.setCurrentLap(raceStartTime,0);
  }
  refreshTagGUI();
}

void refreshTagGUI()
{
//  ESP_LOGI(TAG,"----- Active tags: -----");
  for(int j=0; j<ITAG_COUNT; j++)
  {
    std::lock_guard<std::mutex> lck(mutexTags);
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
      iTags[j].updated = true;
    }

// TODO send update msg to GUI

    if (iTags[j].updated) {
      iTags[j].updated = false;
      iTags[j].updateGUI_locked(); //TODO don't update all, only what is needed
    //  iTags[j].participant.updateChart(); //TODO don't update all, only what is needed
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

static void loadRace()
{
  {
    unsigned int totalBytes = LittleFS.totalBytes();
    unsigned int usedBytes = LittleFS.usedBytes();
    unsigned int freeBytes  = totalBytes - freeBytes;
 
    ESP_LOGI(TAG,"File sistem info: ----------- LOAD"); 
    ESP_LOGI(TAG,"Total space     : %d bytes\n",totalBytes); 
    ESP_LOGI(TAG,"Total space used: %d bytes\n",usedBytes);
    ESP_LOGI(TAG,"Total space free: %d bytes\n",freeBytes);
 
    // Open dir folder
    File dir = LittleFS.open("/");
    // Cycle all the content
    printDirectory(dir,0);
    dir.close();
  }

  std::string fileName = std::string("/RaceData2.json");

  File raceFile = LittleFS.open(fileName.c_str(), "r");
  if (!raceFile) {
    ESP_LOGE(TAG,"ERROR: LittleFS open(%s) failed",fileName.c_str());
    return;
  }

  DynamicJsonDocument raceJson(50000);
  DeserializationError err = deserializeJson(raceJson, raceFile);
  raceFile.close();
  if (err) {
    ESP_LOGE(TAG,"ERROR: deserializeJson() failed with code %s",err.f_str());
    return;
  }
  

  String output = "";
  serializeJsonPretty(raceJson, output);
  ESP_LOGI(TAG,"Loaded json:\n%s", output.c_str());
}

static void saveRace()
{
  DynamicJsonDocument raceJson(50000);
  //JsonObject raceJson = jsonDoc.createObject();
  //char JSONmessageBuffer[200];

  raceJson["Appname"] = "CrazyCapyTime";
  raceJson["filetype"] = "racedata";
  raceJson["fileformatversion"] = "0.1";
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

    JsonArray lapArrayJson = tagJson.createNestedArray("lap");
    for(int lap=0; lap<=iTags[i].participant.getLapCount(); lap++)
    {
      JsonObject lapJson = lapArrayJson.createNestedObject(); 
      lapJson["StartTime"] = iTags[i].participant.getLap(lap).getLapStart();
      lapJson["LastSeen"] = iTags[i].participant.getLap(lap).getLastSeen();
    }
  }
  // Print a minified JSON document to the serial port
  //serializeJson(raceJson, Serial);
  // Same with a prettified document
  //serializeJsonPretty(raceJson, Serial);
  String output = "";
  serializeJsonPretty(raceJson, output);
  ESP_LOGI(TAG,"json: \n%s", output.c_str());


  {
    unsigned int totalBytes = LittleFS.totalBytes();
    unsigned int usedBytes = LittleFS.usedBytes();
    unsigned int freeBytes  = totalBytes - freeBytes;
 
    ESP_LOGI(TAG,"File sistem info: ----------- PRE SAVE"); 
    ESP_LOGI(TAG,"Total space     : %d bytes\n",totalBytes); 
    ESP_LOGI(TAG,"Total space used: %d bytes\n",usedBytes);
    ESP_LOGI(TAG,"Total space free: %d bytes\n",freeBytes);
 
    // Open dir folder
    File dir = LittleFS.open("/");
    // Cycle all the content
    printDirectory(dir,0);
    dir.close();
  }
  std::string fileName = std::string("/RaceData3.json");
  File raceFile = LittleFS.open(fileName.c_str(), "w");
  if (!raceFile) {
    ESP_LOGE(TAG,"ERROR: LittleFS open(%s,w) failed", fileName.c_str());
    return;
  }
  serializeJson(raceJson, raceFile);
  raceFile.close();

    {
    unsigned int totalBytes = LittleFS.totalBytes();
    unsigned int usedBytes = LittleFS.usedBytes();
    unsigned int freeBytes  = totalBytes - freeBytes;
 
    ESP_LOGI(TAG,"File sistem info: ----------- SAVED"); 
    ESP_LOGI(TAG,"Total space     : %d bytes\n",totalBytes); 
    ESP_LOGI(TAG,"Total space used: %d bytes\n",usedBytes);
    ESP_LOGI(TAG,"Total space free: %d bytes\n",freeBytes);
 
    // Open dir folder
    File dir = LittleFS.open("/");
    // Cycle all the content
    printDirectory(dir,0);
    dir.close();
  }
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
            std::lock_guard<std::mutex> lck(mutexTags);
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
                //ESP_LOGI(TAG,"%s Connected Time: %s delta %d->%d (%d,%d) NEW LAP", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),newLapTime,timeSinceLastSeen, iTags[j].participant.getCurrentLapStart(), iTags[j].participant.getCurrentLastSeen());
                if(!iTags[j].participant.nextLap()) {
                  //TODO GUI popup ??
                  ESP_LOGE(TAG,"%s NEW LAP ERROR can't handle more then %d Laps during race", iTags[j].participant.getName().c_str(),iTags[j].participant.getLapCount());
                }
              }
              else {
                time_t newLastSeenSinceLapStart = difftime(newLapTime,iTags[j].participant.getCurrentLapStart());
                //ESP_LOGI(TAG,"%s Connected Time: %s delta %d->%d (%d,%d) %d To early", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),newLapTime,timeSinceLastSeen,iTags[j].participant.getCurrentLapStart(), iTags[j].participant.getCurrentLastSeen(),newLastSeenSinceLapStart);
                iTags[j].participant.setCurrentLastSeen(newLastSeenSinceLapStart);
              }

              iTags[j].updated = true; // Make it redraw when GUI loop looks at it
              UpdateParticipantStatusInGFX(iTags[j]);
              
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
            std::lock_guard<std::mutex> lck(mutexTags);
            // TODO send index? so we don't need the string compare here???
            if(bleAddress == iTags[j].address) {
                            time_t newLapTime = msg.iTag.time;
              iTags[j].setRSSI(msg.iTag.RSSI);
              if (msg.iTag.battery != INT8_MIN) {
                iTags[j].battery = msg.iTag.battery;
              }
              iTags[j].participant.setTimeSinceLastSeen(0);
              iTags[j].active = true;
              UpdateParticipantStatusInGFX(iTags[j]);
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
        case MSG_ITAG_UPDATE_USER_RACE_STATUS:
        {
          uint32_t handleDB = msg.UpdateParticipantRaceStatus.handleDB;
          //ESP_LOGI(TAG,"Received: MSG_ITAG_UPDATE_USER_RACE_STATUS MSG:0x%x handleDB:0x%08x handleGFX:0x%08x inRace:%d ? myinRace:%d-------------", 
          //     msg.UpdateParticipantRaceStatus.header.msgType, msg.UpdateParticipantRaceStatus.handleDB, msg.UpdateParticipantRaceStatus.handleGFX, msg.UpdateParticipantRaceStatus.inRace,iTags[handleDB].participant.getInRace());
          if (iTags[handleDB].participant.getInRace() !=  msg.UpdateParticipantRaceStatus.inRace)
          {
            // In race changed, update
            iTags[handleDB].participant.setInRace(msg.UpdateParticipantRaceStatus.inRace);
            // Send update to GUI
            UpdateParticipantStatusInGFX(iTags[msg.UpdateParticipantRaceStatus.handleDB]);
          }
          break;
        }
        case MSG_ITAG_LOAD_RACE:
        {
          loadRace();
          break;
        }
        case MSG_ITAG_SAVE_RACE:
        {
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
  BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)0); //No blocking in case of problem we will send a new one soon
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
