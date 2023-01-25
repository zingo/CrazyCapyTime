/*
  Handle iTag BT devices
  
  Based on some info found from the nice guides of Fernando K Tecnologia https://www.instructables.com/Multiple-ITags-With-ESP32/ and
  https://www.youtube.com/watch?v=uNGMq_U3ydw

*/
#include <mutex>

#include <ArduinoJson.h>
#include <LittleFS.h>
#include "common.h"
#include "iTag.h"
#include "messages.h"
#include "bluetooth.h"

#define TAG "iTAG"


std::mutex mutexTags; // Lock when access runtime writable data in any tag TODO make one mutex per tag?

bool updateTagsNow = false;

void refreshTagGUI()
{
  updateTagsNow = true;
}

iTag::iTag(std::string inAddress,std::string inName, uint32_t inColor0, uint32_t inColor1)
{
    address = inAddress;
    color0 = inColor0;
    color1 = inColor1;
    battery = -1; //Unknown or Not read yet
    RSSI = -9999;
    active = false;
    connected = false;
    participant.setName(inName);
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

void iTag::saveGUIObjects(lv_obj_t * ledCol, lv_obj_t * labelNam, lv_obj_t * labelDistance, lv_obj_t * labelLap, lv_obj_t * labelTim,lv_obj_t * labelConnStatus, /*lv_obj_t * labelBatterySym,*/ lv_obj_t * labelBat)
{
  std::lock_guard<std::mutex> lck(mutexTags);
  ledColor = ledCol;
  labelName = labelNam;
  labelDist = labelDistance;
  labelLaps = labelLap;
  labelTime = labelTim;
  labelConnectionStatus = labelConnStatus;
  //labelBatterySymbol = labelBatterySym;
  labelBattery = labelBat;
}

void iTag::updateGUI(void)
{
  std::lock_guard<std::mutex> lck(mutexTags);
  updateGUI_locked();
}

void iTag::updateGUI_locked(void)
{
  lv_led_set_color(ledColor, lv_color_hex(color0));
  lv_label_set_text(labelName, participant.getName().c_str());
//  lv_label_set_text(labelLaps, (std::string("L:") + std::to_string(laps)).c_str());
  lv_label_set_text_fmt(labelDist, "%4.3f km",(participant.getLapCount()*RACE_DISTANCE_LAP)/1000.0);
  lv_label_set_text_fmt(labelLaps, "(%2d/%2d)",participant.getLapCount(),RACE_LAPS);

  struct tm timeinfo;
  time_t tt = participant.getCurrentLapStart();
  localtime_r(&tt, &timeinfo);
  lv_label_set_text_fmt(labelTime, "%3d:%02d:%02d", (timeinfo.tm_mday-1)*24+timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);

  if (active)
  {
    //lv_obj_set_style_opa(labelConnectionStatus, 0, 0);
    if (connected) {
      if (participant.getTimeSinceLastSeen() < 20) {
        lv_label_set_text(labelConnectionStatus, LV_SYMBOL_EYE_OPEN);
      }
      else {
        lv_label_set_text(labelConnectionStatus, LV_SYMBOL_EYE_CLOSE);
      }
    }
    else {
      lv_label_set_text(labelConnectionStatus, "");
    }
  }
  else {
    //lv_label_set_text(labelConnectionStatus, LV_SYMBOL_BLUETOOTH);
    lv_label_set_text(labelConnectionStatus, "");
  }

  if (raceOngoing)
  {
    if (connected && participant.getTimeSinceLastSeen() < 20) {
      //lv_label_set_text(labelBatterySymbol, LV_SYMBOL_WIFI);
      lv_label_set_text_fmt(labelBattery, "%d", getRSSI());
    }
    else {
      //lv_label_set_text(labelBatterySymbol, "");
      lv_label_set_text(labelBattery, "");

    }
  }
  else {
    if (battery > 0) {  
/*
      if (battery >= 90) {
        lv_label_set_text(labelBatterySymbol, LV_SYMBOL_BATTERY_FULL);
      }
      else if (battery >= 70) {
        lv_label_set_text(labelBatterySymbol, LV_SYMBOL_BATTERY_3);
      }
      else if (battery >= 40) {
        lv_label_set_text(labelBatterySymbol, LV_SYMBOL_BATTERY_2);
      }
      else if (battery >= 10) {
        lv_label_set_text(labelBatterySymbol, LV_SYMBOL_BATTERY_1);
      }
      else {
        lv_label_set_text(labelBatterySymbol, LV_SYMBOL_BATTERY_EMPTY);
      }
*/
      lv_label_set_text_fmt(labelBattery, "%3d%%",battery);
    }
    else {
//      lv_label_set_text(labelBatterySymbol, "");
      lv_label_set_text(labelBattery, "");
    }
  }
}


#define ITAG_COLOR_DARKBLUE 0x0b0b45 //Navy blue
#define ITAG_COLOR_ORANGE 0xFA8128 //Tangerine
#define ITAG_COLOR_GREEN 0xAEF359 //LIME
#define ITAG_COLOR_WHITE 0xffffff

iTag iTags[ITAG_COUNT] = {
  iTag("ff:ff:10:7e:be:67", "Zingo",   ITAG_COLOR_ORANGE,ITAG_COLOR_WHITE), //Orange
  iTag("ff:ff:10:7d:d2:08", "Stefan",  ITAG_COLOR_DARKBLUE,ITAG_COLOR_WHITE), //Dark blue
  iTag("ff:ff:10:82:ef:1e", "Ross", ITAG_COLOR_GREEN,ITAG_COLOR_GREEN)  //Light green BT4
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

void updateiTagStatus()
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
    if (iTags[j].updated) {
      iTags[j].updated = false;
      iTags[j].updateGUI_locked(); //TODO don't update all, only what is needed
      iTags[j].participant.updateChart();
    }

   // if(iTags[j].active) {
   //   ESP_LOGI(TAG,"Active: %3d/%3d (max:%3d) %s RSSI:%d %3d%% Laps: %5d | %s", iTags[j].participant.getTimeSinceLastSeen(),MINIMUM_LAP_TIME_IN_SECONDS, longestNonSeen, iTags[j].connected? "#":" ", iTags[j].getRSSI(), iTags[j].battery ,iTags[j].participant.getLapCount() , iTags[j].participant.getName().c_str());
   // }
  }
//  ESP_LOGI(TAG,"------------------------");
}


void printDirectory(File dir, int numTabs) {
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

void loadRace()
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

  File raceFile = LittleFS.open("/RaceData.json", "r");
  if (!raceFile) {
    ESP_LOGE(TAG,"ERROR: LittleFS open(/RaceData.json,r) failed");
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

    JsonArray lapArrayJson = tagJson.createNestedArray("lap");
    for(int lap=0; lap<=iTags[i].participant.getLapCount(); lap++)
    {
      JsonObject lapJson = lapArrayJson.createNestedObject(); 
      lapJson["StartTime"] = iTags[i].participant.getLap(lap).getLapStart();
      lapJson["LastSeen"] = iTags[i].participant.getLap(lap).getLastSeen();
    }
  }


}

void saveRace()
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

  File raceFile = LittleFS.open("/RaceData2.json", "w");
  if (!raceFile) {
    ESP_LOGE(TAG,"ERROR: LittleFS open(/RaceData.json,w) failed");
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
  for( ;; )
  {
    msg_iTagDetected msg;
    if( xQueueReceive(queueiTagDetected, &(msg), (TickType_t)portMAX_DELAY))
    {
      switch(msg.msgType) {
        case MSG_ITAG_DETECTED:
        {
          //ESP_LOGI(TAG,"MSG_ITAG_DETECTED");
          //delay(2000);
          // iTag detected
          std::string bleAddress = convertBLEAddressToString(msg.address);
          for(int j=0; j<ITAG_COUNT; j++)
          {
            std::lock_guard<std::mutex> lck(mutexTags);
            if(bleAddress == iTags[j].address) {
              //ESP_LOGI(TAG,"Scaning iTAGs MATCH: %s",String(advertisedDevice->toString().c_str()).c_str());
              //ESP_LOGI(TAG,"####### Spotted %s Time: %s", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());

              time_t newLapTime = msg.time;
              iTags[j].setRSSI(msg.RSSI);
              if (msg.battery != INT8_MIN) {
                iTags[j].battery = msg.battery;
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

              if (!iTags[j].active) {
                ESP_LOGI(TAG,"%s Activate Time: %s", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());
                msg.msgType = MSG_ITAG_CONFIG;
                BaseType_t xReturned = xQueueSend(queueBTConnect, (void*)&msg, (TickType_t)0); //Don't wait if queue is full, just retry next time we scan the tag
                if (xReturned)
                {
                  //Only mark active if it was possible to put in on the queue, if not it will just retry next time we scan the tag
                  iTags[j].active = true;  //TODO tristate, falst->asking->true (only send one msg)
                  //iTags[j].reset(); // will set active = true
                }
              }
            }
          }
          break;
        }
        case MSG_ITAG_CONFIGURED:
        {
          ESP_LOGI(TAG,"MSG_ITAG_CONFIGURED");
          //delay(2000);

          // iTag active
          std::string bleAddress = convertBLEAddressToString(msg.address);
          for(int j=0; j<ITAG_COUNT; j++)
          {
            std::lock_guard<std::mutex> lck(mutexTags);
            // TODO send index? so we don't need the string compare here???
            if(bleAddress == iTags[j].address) {
                            time_t newLapTime = msg.time;
              iTags[j].setRSSI(msg.RSSI);
              if (msg.battery != INT8_MIN) {
                iTags[j].battery = msg.battery;
              }
              iTags[j].participant.setTimeSinceLastSeen(0);
              iTags[j].active = true;
            }
          }          
          break;
        }
        default:
          ESP_LOGE(TAG,"%s ERROR received bad msg: 0x%x",msg.msgType);
          break;
      }
    }
  }
    vTaskDelete( NULL ); // Should never be reached
}

void initiTAGs()
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

  if( xReturned == pdPASS )
  {
      /* The task was created.  Use the task's handle to delete the task. */
      //vTaskDelete( xHandle );
  }

}

void loopHandlTAGs()
{
  // Show connection in log
  uint32_t now = millis();
  if (updateTagsNow) {
    updateTagsNow = false;
    lastScanTime = now;
    updateiTagStatus();
  }
  else if (now - lastScanTime > 2000) { 
    updateTagsNow = false;
    lastScanTime = now;
    updateiTagStatus();
    //saveRace();
    //rtc.setTime(rtc.getEpoch()+30,0); //fake faster time REMOVE
  }
}
