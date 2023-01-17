/*
  Handle iTag BT devices
  
  Based on some info found from the nice guides of Fernando K Tecnologia https://www.instructables.com/Multiple-ITags-With-ESP32/ and
  https://www.youtube.com/watch?v=uNGMq_U3ydw

*/
#include <mutex>
#include <NimBLEDevice.h>
#include "common.h"
#include "iTag.h"

#define TAG "iTAG"

#define SCAN_INTERVAL 2000 //How often should we update GUI
#define BT_SCAN_TIME 5 // in seconds

std::mutex mutexTags; // Lock when access runtime writable data in any tag TODO make one mutex per tag?
static uint16_t appId = 3;

bool updateTagsNow = false;

std::string getTimeFormat(String format, struct tm *timeinfo)
{
	char s[128];
	char c[128];
	format.toCharArray(c, 127);
	strftime(s, 127, c, timeinfo);
	return std::string(s);
}


//When the BLE Server sends a new button reading with the notify property
static void buttonNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                        uint8_t* pData, size_t length, bool isNotify) {
  ESP_LOGI(TAG,"Button PRESSED");
}

iTag::iTag(std::string inAddress,std::string inName, uint32_t inColor0, uint32_t inColor1)
{
    address = inAddress;
    color0 = inColor0;
    color1 = inColor1;
    battery = -1; //Unknown or Not read yet
    active = false;
    connected = false;
    participant.setName(inName);
    participant.clearLaps();
    updated = false;
}

bool iTag::updateBattery( NimBLEClient* client) {
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

          std::lock_guard<std::mutex> lck(mutexTags);
          battery = bat;
        }
  //          if(remoteCharacteristic->canNotify()) {
  //            remoteCharacteristic->registerForNotify(batteryNotifyCallback);
  //          }
        return true;
      }
    }
    return false;
}

bool iTag::toggleBeep(NimBLEClient* client, bool beep) {
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

bool iTag::toggleBeepOnLost(NimBLEClient* client, bool beep) {
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

bool iTag::connect(NimBLEAdvertisedDevice* advertisedDevice)
{
  NimBLEClient* client;

      client = BLEDevice::createClient(appId++);
      client->setConnectTimeout(10); // 10s

  BLEAddress bleAddress(address);
  if(!client->connect(advertisedDevice)) {
    // Created a client but failed to connect, don't need to keep it as it has no data */
    NimBLEDevice::deleteClient(client);
    ESP_LOGI(TAG,"Failed to connect, deleted client\n");
    return false;
  }
  ESP_LOGI(TAG,"Connected to: %s RSSI: %d\n",
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
        remoteCharacteristic->registerForNotify(buttonNotifyCallback);
    }
  }
#endif
  toggleBeepOnLost(client, false);
  updateBattery(client);

  //toggleBeep(client, true);  // Welcome beep
  //delay(200);        //TODO remove delay and schedule this in the loop somehow
  //toggleBeep(client, false);

  {
    std::lock_guard<std::mutex> lck(mutexTags);
    // TODO clear lap data?
    participant.setCurrentLap(0,0); // TODO set race start so a late tag get full race
    participant.setTimeSinceLastSeen(0);
    active = true;
    updated = true; // will trigger GUI update later
  }

  client->disconnect(); // no need to stay connected
  return true;
}

void iTag::saveGUIObjects(lv_obj_t * ledCol, lv_obj_t * labelNam, lv_obj_t * labelDistance, lv_obj_t * labelLap, lv_obj_t * labelTim,lv_obj_t * labelConnStatus, lv_obj_t * labelBatterySym, lv_obj_t * labelBat)
{
  std::lock_guard<std::mutex> lck(mutexTags);
  ledColor = ledCol;
  labelName = labelNam;
  labelDist = labelDistance;
  labelLaps = labelLap;
  labelTime = labelTim;
  labelConnectionStatus = labelConnStatus;
  labelBatterySymbol = labelBatterySym;
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
  //getTimeFormat("%d-%H:%M:%S",&timeLastShownUp).c_str()

  if (active)
  {
    //lv_obj_set_style_opa(labelConnectionStatus, 0, 0);
    if (connected) {
      if (participant.getTimeSinceLastSeen() < 10) {
        lv_label_set_text(labelConnectionStatus, LV_SYMBOL_EYE_OPEN);
      }
      else {
        lv_label_set_text(labelConnectionStatus, LV_SYMBOL_EYE_CLOSE);
      }
    }
    else {
      lv_label_set_text(labelConnectionStatus, LV_SYMBOL_BLUETOOTH);
    }
  }
  else {
    //lv_obj_set_style_opa(labelConnectionStatus, 50, 0);
    lv_label_set_text(labelConnectionStatus, LV_SYMBOL_BLUETOOTH);
  }
  if (battery > 0) {  
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
    lv_label_set_text_fmt(labelBattery, "%3d%%",battery);
  }
  else {
    lv_label_set_text(labelBatterySymbol, "");
    lv_label_set_text(labelBattery, "");
  }
}


#define ITAG_COLOR_DARKBLUE 0x0b0b45 //Navy blue
#define ITAG_COLOR_ORANGE 0xFA8128 //Tangerine
#define ITAG_COLOR_GREEN 0xAEF359 //LIME
#define ITAG_COLOR_WHITE 0xffffff

iTag iTags[ITAG_COUNT] = {
  iTag("ff:ff:10:7e:be:67", "Zingo",   ITAG_COLOR_ORANGE,ITAG_COLOR_WHITE), //Orange
  iTag("ff:ff:10:7d:d2:08", "Stefan",  ITAG_COLOR_DARKBLUE,ITAG_COLOR_WHITE), //Dark blue
  iTag("ff:ff:10:82:ef:1e", "Johan(na)?", ITAG_COLOR_GREEN,ITAG_COLOR_GREEN)  //Light green BT4
};
static NimBLEAdvertisedDevice* advDevice;
static int32_t doConnect = -1; // -1 = none else it shows index into iTags to connect
uint32_t lastScanTime = 0;

void startRaceiTags()
{
  tm timeNow = rtc.getTimeStruct();
  time_t raceStartTime = mktime (&timeNow); //TODO raceStartTime should be something "global"

  std::lock_guard<std::mutex> lck(mutexTags);
  for(int j=0; j<ITAG_COUNT; j++)
  {
    iTags[j].updated = true;
    iTags[j].participant.clearLaps();
    iTags[j].participant.setCurrentLap(raceStartTime,0);
  }
  updateTagsNow = true;
}

void updateiTagStatus()
{
  ESP_LOGI(TAG,"----- Active tags: -----");
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

      if (timeSinceLastSeen > MINIMUM_LAP_TIME_IN_SECONDS) {
        ESP_LOGI(TAG,"%s Disconnected Time: %s delta %d", iTags[j].address.c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),iTags[j].participant.getTimeSinceLastSeen());
        iTags[j].connected = false;
      }
      iTags[j].updated = true;
    }
    if (iTags[j].updated) {
      iTags[j].updated = false;
      iTags[j].updateGUI_locked(); //TODO don't update all, only what is needed
    }

    if(iTags[j].active) {
      ESP_LOGI(TAG,"Active: %3d/%3d %s %d%% Laps: %5d | %s", iTags[j].participant.getTimeSinceLastSeen(),MINIMUM_LAP_TIME_IN_SECONDS, iTags[j].connected? "#":" ", iTags[j].battery ,iTags[j].participant.getLapCount() , iTags[j].participant.getName().c_str());
    }
  }
  ESP_LOGI(TAG,"------------------------");
}

/* Define a class to handle the callbacks when advertisements are received */
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

  void onResult(NimBLEAdvertisedDevice* advertisedDevice)
  {
    //ESP_LOGI(TAG,"Scaning iTAGs Found: %s",String(advertisedDevice->toString().c_str()).c_str());

    for(int j=0; j<ITAG_COUNT; j++)
    {
      std::lock_guard<std::mutex> lck(mutexTags);
      if(advertisedDevice->getAddress().toString() == iTags[j].address) {
        //ESP_LOGI(TAG,"Scaning iTAGs MATCH: %s",String(advertisedDevice->toString().c_str()).c_str());

        iTags[j].connected = true;
        iTags[j].participant.setTimeSinceLastSeen(0);
        tm timeNow = rtc.getTimeStruct();
        time_t newLapTime = mktime(&timeNow);
        time_t lastSeenSinceStart = iTags[j].participant.getCurrentLapStart() + iTags[j].participant.getCurrentLastSeen();
        uint32_t timeSinceLastSeen = difftime(newLapTime, lastSeenSinceStart);

        if (timeSinceLastSeen > MINIMUM_LAP_TIME_IN_SECONDS) {
          //ESP_LOGI(TAG,"%s Connected Time: %s delta %d->%d (%d,%d) NEW LAP", iTags[j].participant.getName().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(),newLapTime,timeSinceLastSeen, iTags[j].participant.getCurrentLapStart(), iTags[j].participant.getCurrentLastSeen());
          if(!iTags[j].participant.nextLap(newLapTime)) {
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
          // Set it up with iTags[j].connect() but we delay calling it until loop() loopHandlTAGs
          // Ready to connect now
          doConnect = j;
          // Save the device reference in a global for the client to use
          advDevice = advertisedDevice; // should match "j"
          // stop scan before connecting
          NimBLEDevice::getScan()->stop();
        }
      }
    }
  }
};

// Called when BT scanning has ended, start a new in non is found
void scanBTCompleteCB(NimBLEScanResults scanResults)
{
  // Only restart scanning if not connecting
  if (doConnect < 0) {
    NimBLEDevice::getScan()->start(BT_SCAN_TIME, scanBTCompleteCB);
  }
}

void initiTAGs()
{
  BLEScan* pBLEScan;
  ESP_LOGI(TAG,"BLEDevice init");
  NimBLEDevice::init("");
  pBLEScan = NimBLEDevice::getScan();

  // create a callback that gets called when advertisers are found
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());

  // Set scan interval (how often) and window (how long) in milliseconds
  pBLEScan->setInterval(97);
  pBLEScan->setWindow(67);

  // Active scan will gather scan response data from advertisers but will use more energy from both devices
  pBLEScan->setActiveScan(true);

  // Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
  pBLEScan->start(BT_SCAN_TIME, scanBTCompleteCB);

}

void loopHandlTAGs()
{
    if (doConnect >= 0) {
        /* Found a device we want to connect to, do it now */
        if (iTags[doConnect].connect(advDevice)) {
            ESP_LOGI(TAG,"Connect %d Success!, scanning for more!", doConnect);
        } else {
            ESP_LOGI(TAG,"Connect %d Failed to connect, starting scan", doConnect);
        }
        doConnect = -1;
        advDevice = NULL;
        NimBLEDevice::getScan()->start(BT_SCAN_TIME, scanBTCompleteCB);
    }
  // Show connection in log
  uint32_t now = millis();
  if (updateTagsNow) {
    updateTagsNow = false;
    lastScanTime = now;
    updateiTagStatus();
  }
  else if (now - lastScanTime > SCAN_INTERVAL) { 
    updateTagsNow = false;
    lastScanTime = now;
    updateiTagStatus();
  }
}
