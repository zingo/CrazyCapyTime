/*
  Handle iTag BT devices
  
  Based on some info found from the nice guides of Fernando K Tecnologia https://www.instructables.com/Multiple-ITags-With-ESP32/ and
  https://www.youtube.com/watch?v=uNGMq_U3ydw

*/
#include <mutex>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "common.h"
#include "iTag.h"

#define TAG "iTAG"
#define BT_SCAN_TIME 5 // in seconds

std::mutex mutexTags; // Lock when access runtime writable data in any tag TODO make one mutex per tag?
static uint16_t appId = 3;

bool updateTagsNow = false;

static bool doBTConnect(uint32_t tagIndex);

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

#if 0

//When the BLE Server sends a new button reading with the notify property
static void BTiTagButtonPressed(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                        uint8_t* pData, size_t length, bool isNotify) {
  ESP_LOGI(TAG,"Button PRESSED");
}

#endif

bool BTupdateBattery( NimBLEClient* client) {
    // Battery READ, NOTIFY
    static const BLEUUID batteryServiceUUID("0000180f-0000-1000-8000-00805f9b34fb");
    static const BLEUUID batteryCharacteristicUUID("00002a19-0000-1000-8000-00805f9b34fb");

    NimBLERemoteService* remoteService = client->getService(batteryServiceUUID);
    if (remoteService) {
      NimBLERemoteCharacteristic* remoteCharacteristic = remoteService->getCharacteristic(batteryCharacteristicUUID);
      if (remoteCharacteristic) {
        // Read the value of the characteristic.
        if(remoteCharacteristic->canRead()) {
          uint32_t bat = remoteCharacteristic->readUInt8();
          ESP_LOGI(TAG,"Read battery value: 0x%x %d",bat,bat);

          //std::lock_guard<std::mutex> lck(mutexTags);
          //TODO iTags[tagIndex].battery = bat;
        }
  //          if(remoteCharacteristic->canNotify()) {
  //            remoteCharacteristic->registerForNotify(batteryNotifyCallback);
  //          }
        return true;
      }
    }
    return false;
}

bool BTtoggleBeep(NimBLEClient* client, bool beep) {
    // Alert WRITE, WRITE_WITHOUT_RESPONSE, NOTIFY 0x00-NoAlert 0x01-MildAlert 0x02-HighAlert
    static const BLEUUID alertServiceUUID("00001802-0000-1000-8000-00805f9b34fb");
    static const BLEUUID alertCharacteristicUUID("00002a06-0000-1000-8000-00805f9b34fb");
    NimBLERemoteService* remoteService = client->getService(alertServiceUUID);
    if (remoteService) {
    NimBLERemoteCharacteristic* remoteCharacteristic = remoteService->getCharacteristic(alertCharacteristicUUID);
    if (remoteCharacteristic) {
  //      uint8_t value = remoteCharacteristic->readUInt8();  // Read the value of the characteristic.
  //      ESP_LOGI(TAG,"Read alert value: 0x%x %d",value,value);

        const uint8_t NoAlert[]   = {0x0};
        const uint8_t MildAlert[] = {0x1};
        const uint8_t HighAlert[] = {0x2};
        if (beep) {
          remoteCharacteristic->writeValue((uint8_t*)&HighAlert, 1, false);
        }
        else {
          remoteCharacteristic->writeValue((uint8_t*)&NoAlert, 1, false);
        }
        return true;
    }
    }
    return false;
}

bool BTtoggleBeepOnLost(NimBLEClient* client, bool beep) {
    // Alert when disconnect WRITE,  0x00-NoAlert 0x01-Alert
    static const BLEUUID alertServiceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
    static const BLEUUID alertCharacteristicUUID("0000ffe2-0000-1000-8000-00805f9b34fb");
    NimBLERemoteService* remoteService = client->getService(alertServiceUUID);
    if (remoteService) {
      NimBLERemoteCharacteristic* remoteCharacteristic = remoteService->getCharacteristic(alertCharacteristicUUID);
      if (remoteCharacteristic) {
//        uint8_t value = remoteCharacteristic->readUInt8();  // Read the value of the characteristic.
//        ESP_LOGI(TAG,"Read alert value: 0x%x %d",value,value);

        const uint8_t NoAlert[] = {0x0};
        const uint8_t Alert[]   = {0x1};
        if (beep) {
          remoteCharacteristic->writeValue((uint8_t*)&Alert, 1, false);
        }
        else {
          remoteCharacteristic->writeValue((uint8_t*)&NoAlert, 1, false);
        }
        return true;
      }
    }
    return false;
}


bool BTconnect(NimBLEAddress &bleAddress)
{
  NimBLEClient* client;

  client = BLEDevice::createClient(appId++);
  client->setConnectTimeout(10); // 10s

  //NimBLEAddress bleAddress(address);
  if(!client->connect(bleAddress,true)) {
    // Created a client but failed to connect, don't need to keep it as it has no data */
    NimBLEDevice::deleteClient(client);
    ESP_LOGI(TAG,"Failed to connect, deleted client\n");
    return false;
  }
  ESP_LOGI(TAG,"Connected to: %s RSSI: %d",
          client->getPeerAddress().toString().c_str(),
          client->getRssi());

#if 0
  // Button READ, NOTIFY
  static const BLEUUID buttonServiceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
  static const BLEUUID buttonCharacteristicUUID("0000ffe1-0000-1000-8000-00805f9b34fb");
  NimBLERemoteService* remoteService = client->getService(buttonServiceUUID);
  if (remoteService) {
    NimBLERemoteCharacteristic* remoteCharacteristic = remoteService->getCharacteristic(buttonCharacteristicUUID);
    if (remoteCharacteristic) {
        std::string value = remoteCharacteristic->readValue();  // Read the value of the characteristic.
        ESP_LOGI(TAG,"Read characteristic value: %s",value.c_str());
/*
        const uint8_t bothOff[]        = {0x0, 0x0};
        const uint8_t notificationOn[] = {0x1, 0x0};
        const uint8_t indicationOn[]   = {0x2, 0x0};
        const uint8_t bothOn[]         = {0x3, 0x0};

        remoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true); 
*/
        remoteCharacteristic->registerForNotify(BTiTagButtonPressed);
    }
  }
#endif
  BTtoggleBeepOnLost(client, false);
  BTupdateBattery(client);

  //BTtoggleBeep(client, true);  // Welcome/setup beep
  //delay(200);
  //BTtoggleBeep(client, false);

  client->disconnect(); // no need to stay connected
  return true;
}

bool doBTScan = true;

// Called when BT scanning has ended, start a new if non is found
void scanBTCompleteCB(NimBLEScanResults scanResults)
{
  // Only restart scanning if not connecting
  if (doBTScan) {
    //ESP_LOGI(TAG,"BT Connect SCAN restart");
    NimBLEDevice::getScan()->start(BT_SCAN_TIME, scanBTCompleteCB);
  }
}

/* Define a class to handle the callbacks when advertisements are received */
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

  void onResult(NimBLEAdvertisedDevice* advertisedDevice)
  {
    //ESP_LOGI(TAG,"Scaning iTAGs Found: %s",String(advertisedDevice->toString().c_str()).c_str());

    for(int j=0; j<ITAG_COUNT; j++)
    {
      std::lock_guard<std::mutex> lck(mutexTags);  // TODO maybe just send a queue msg to a database/iTag task and handle all stuff there and remove this lock
      if(advertisedDevice->getAddress().toString() == iTags[j].address) {
        //ESP_LOGI(TAG,"Scaning iTAGs MATCH: %s",String(advertisedDevice->toString().c_str()).c_str());
        //ESP_LOGI(TAG,"####### Spotted %s Time: %s", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());

        iTags[j].connected = true;
        iTags[j].participant.setTimeSinceLastSeen(0);
        if (advertisedDevice->haveRSSI()) {
          iTags[j].setRSSI(advertisedDevice->getRSSI());
        }
        else {
          iTags[j].setRSSI(-9999);
        }
        tm timeNow = rtc.getTimeStruct();
        time_t newLapTime = mktime(&timeNow);
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
          doBTScan = false; // will be set to true when queue is handled
          if (doBTConnect(j))
          {
            //Only mark active if it was possible to put in on the queue, if not it will just retry next time we scan the tag
            iTags[j].active = true;
            //iTags[j].reset(); // will set active = true
          }
        }
      }
    }
  }
};

QueueHandle_t queueBTConnect;

static bool doBTConnect(uint32_t tagIndex)
{
  NimBLEAddress bleAddress(iTags[tagIndex].address);
  uint64_t bleAddress64 = static_cast<uint64_t>(bleAddress);
  BaseType_t xReturned = xQueueSend(queueBTConnect, (void*)&bleAddress64, (TickType_t)0); //Don't wait if queue is full, just retry next time we scan the tag
  return xReturned;
}

void vTaskBTConnect( void *pvParameters )
{
  /* The parameter value is expected to be 1 as 1 is passed in the
     pvParameters value in the call to xTaskCreate() below. 
  */
  configASSERT( ( ( uint32_t ) pvParameters ) == 1 );

  BLEScan* pBLEScan;
  ESP_LOGI(TAG,"BLEDevice init");
  NimBLEDevice::init("");

  // 64bit per tag lets just make the queue big enough for all (it should work to make it smaller)
  queueBTConnect = xQueueCreate(ITAG_COUNT, sizeof(uint64_t));  // ITAG_COUNT x NimBLEAddress e.g. static_cast<uint64_t>(NimBLEAddress) 
  if (queueBTConnect == 0){
    ESP_LOGE(TAG,"Failed to create queueBTConnect = %p\n", queueBTConnect);
    // TODO Something more clever here?
  }

  pBLEScan = NimBLEDevice::getScan();

  // create a callback that gets called when advertisers are found
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());

  // Set scan interval (how often) and window (how long) in milliseconds
  pBLEScan->setInterval(97);
  pBLEScan->setWindow(67);

  // Active scan will gather scan response data from advertisers but will use more energy from both devices
  pBLEScan->setActiveScan(true);

  // Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
  doBTScan = true;  // used by the callback to autorestart BT scan
  pBLEScan->start(BT_SCAN_TIME, scanBTCompleteCB);

  for( ;; )
  {
    ESP_LOGI(TAG,"Wait for BT Connect");
    uint64_t bleAddress64;
    if( xQueueReceive(queueBTConnect, &(bleAddress64), (TickType_t)portMAX_DELAY))
    {
      ESP_LOGI(TAG,"BT Connect SCAN Stop");

      doBTScan = false;
      NimBLEDevice::getScan()->stop();

      do {  // Connect to all in the queue
        NimBLEAddress bleAddress(bleAddress64);
        ESP_LOGI(TAG,"BT Connect %s", bleAddress.toString().c_str());
        BTconnect(bleAddress);
      } while (xQueueReceive(queueBTConnect, &(bleAddress64), (TickType_t)0)); // empty the queue
      
      ESP_LOGI(TAG,"BT Connect SCAN Start");
      doBTScan = true;
      NimBLEDevice::getScan()->start(BT_SCAN_TIME, scanBTCompleteCB);
    }
  }
    vTaskDelete( NULL ); // Should never be reached
}

void initiTAGs()
{

  // Start BT Task (scan and inital connect&config)
  BaseType_t xReturned;
  TaskHandle_t xHandle = NULL;
  /* Create the task, storing the handle. */
  xReturned = xTaskCreate(
                  vTaskBTConnect,       /* Function that implements the task. */
                  "BTConnect",          /* Text name for the task. */
                  4096,      /* Stack size in words, not bytes. */
                  ( void * ) 1,    /* Parameter passed into the task. */
                  tskIDLE_PRIORITY,/* Priority at which the task is created. */
                  &xHandle );      /* Used to pass out the created task's handle. */

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
