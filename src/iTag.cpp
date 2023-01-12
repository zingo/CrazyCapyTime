/*
  Handle iTag BT devices
  
  Based on some info found from the nice guides of Fernando K Tecnologia https://www.instructables.com/Multiple-ITags-With-ESP32/ and
  https://www.youtube.com/watch?v=uNGMq_U3ydw

*/
#include <NimBLEDevice.h>
#include "common.h"
#include "iTag.h"

#define TAG "iTAG"

#define SCAN_INTERVAL 2000
#define BT_SCAN_TIME 2 // in seconds


static uint16_t appId = 3;

//When the BLE Server sends a new button reading with the notify property
static void buttonNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                        uint8_t* pData, size_t length, bool isNotify) {
  ESP_LOGI(TAG,"Button PRESSED");
}

class ClientCallbacks : public NimBLEClientCallbacks
{
    void onConnect(NimBLEClient* pClient) {
    for(int j=0; j<ITAG_COUNT; j++)
    {
        if(pClient->getPeerAddress().toString() == iTags[j].address) {
          // TODO add disconnect/reconnect detection with minimum time and stuff
          ESP_LOGI(TAG,"%s Connected Time: %s", pClient->getPeerAddress().toString().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());
          iTags[j].connected = true;
          if (iTags[j].active) {
            iTags[j].laps++;
            iTags[j].updateGUI();
          }
        }
    }
    };

    void onDisconnect(NimBLEClient* pClient) {
    for(int j=0; j<ITAG_COUNT; j++)
    {
        if(pClient->getPeerAddress().toString() == iTags[j].address) {
        ESP_LOGI(TAG,"%s Disconnected Time: %s", pClient->getPeerAddress().toString().c_str(),rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());
        iTags[j].connected = false;
        iTags[j].updateGUI();
        }
    }
    };
};

iTag::iTag(std::string inAddress,std::string inName, uint32_t inColor0, uint32_t inColor1)
{
    address = inAddress;
    name = inName;
    color0 = inColor0;
    color1 = inColor1;
    pClient = NULL;
    battery = 0;
    active = false;
    connected = false;
    laps = 0;
}

bool iTag::updateBattery() {
    // Battery READ, NOTIFY
    static const BLEUUID batteryServiceUUID("0000180f-0000-1000-8000-00805f9b34fb");
    static const BLEUUID batteryCharacteristicUUID("00002a19-0000-1000-8000-00805f9b34fb");

    NimBLERemoteService* remoteService = pClient->getService(batteryServiceUUID);
    if (remoteService) {
    NimBLERemoteCharacteristic* remoteCharacteristic = remoteService->getCharacteristic(batteryCharacteristicUUID);
    if (remoteCharacteristic) {
        // Read the value of the characteristic.
        if(remoteCharacteristic->canRead()) {
        battery = remoteCharacteristic->readUInt8();
        ESP_LOGI(TAG,"Read battery value: 0x%x %d",battery,battery);
        }
//          if(remoteCharacteristic->canNotify()) {
//            remoteCharacteristic->registerForNotify(batteryNotifyCallback);
//          }
        return true;
    }
    }
    return false;
}


bool iTag::toggleBeep(bool beep) {
    // Alert WRITE, WRITE_WITHOUT_RESPONSE, NOTIFY 0x00-NoAlert 0x01-MildAlert 0x02-HighAlert
    static const BLEUUID alertServiceUUID("00001802-0000-1000-8000-00805f9b34fb");
    static const BLEUUID alertCharacteristicUUID("00002a06-0000-1000-8000-00805f9b34fb");
    NimBLERemoteService* remoteService = pClient->getService(alertServiceUUID);
    if (remoteService) {
    NimBLERemoteCharacteristic* remoteCharacteristic = remoteService->getCharacteristic(alertCharacteristicUUID);
    if (remoteCharacteristic) {
        uint8_t value = remoteCharacteristic->readUInt8();  // Read the value of the characteristic.
        ESP_LOGI(TAG,"Read alert value: 0x%x %d",value,value);

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

bool iTag::toggleBeepOnLost(bool beep) {
    // Alert when disconnect WRITE,  0x00-NoAlert 0x01-Alert
    static const BLEUUID alertServiceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
    static const BLEUUID alertCharacteristicUUID("0000ffe2-0000-1000-8000-00805f9b34fb");
    NimBLERemoteService* remoteService = pClient->getService(alertServiceUUID);
    if (remoteService) {
    NimBLERemoteCharacteristic* remoteCharacteristic = remoteService->getCharacteristic(alertCharacteristicUUID);
    if (remoteCharacteristic) {
        uint8_t value = remoteCharacteristic->readUInt8();  // Read the value of the characteristic.
        ESP_LOGI(TAG,"Read alert value: 0x%x %d",value,value);

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
    if(pClient == NULL) {
    pClient = BLEDevice::createClient(appId++);
    pClient->setClientCallbacks(new ClientCallbacks, false);
    pClient->setConnectTimeout(10); // 10s
    }

    BLEAddress bleAddress(address);
    if(!pClient->connect(advertisedDevice)) {
    // Created a client but failed to connect, don't need to keep it as it has no data */
    NimBLEDevice::deleteClient(pClient);
    pClient = NULL;
    ESP_LOGI(TAG,"Failed to connect, deleted client\n");
    return false;
    }
    active = true;
    ESP_LOGI(TAG,"Connected to: %s RSSI: %d\n",
            pClient->getPeerAddress().toString().c_str(),
            pClient->getRssi());

    // Button READ, NOTIFY
    static const BLEUUID buttonServiceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
    static const BLEUUID buttonCharacteristicUUID("0000ffe1-0000-1000-8000-00805f9b34fb");
    NimBLERemoteService* remoteService = pClient->getService(buttonServiceUUID);
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
    toggleBeepOnLost(false);
    updateBattery();

    updateGUI();

    toggleBeep(true);  // Welcome beep
    delay(300);        //TODO remove delay and schedule this in the loop somehow
    toggleBeep(false);
    return true;
}

void iTag::saveGUIObjects(lv_obj_t * ledCol, lv_obj_t * labelNam, lv_obj_t * labelLap, lv_obj_t * labelConnStatus, lv_obj_t * labelBatterySym, lv_obj_t * labelBat)
{
  ledColor = ledCol;
  labelName = labelNam;
  labelLaps = labelLap;
  labelConnectionStatus = labelConnStatus;
  labelBatterySymbol = labelBatterySym;
  labelBattery = labelBat;
}

void iTag::updateGUI(void)
{
  lv_led_set_color(ledColor, lv_color_hex(color0));
  lv_label_set_text(labelName, name.c_str());
  lv_label_set_text(labelLaps, std::to_string(laps).c_str());

  if (active)
  {
    //lv_obj_set_style_opa(labelConnectionStatus, 0, 0);
    if (connected) {
      lv_label_set_text(labelConnectionStatus, LV_SYMBOL_EYE_OPEN);
    }
    else {
      lv_label_set_text(labelConnectionStatus, LV_SYMBOL_BLUETOOTH);
    }
  }
  else {
    //lv_obj_set_style_opa(labelConnectionStatus, 50, 0);
    lv_label_set_text(labelConnectionStatus, LV_SYMBOL_BLUETOOTH);
  }

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
  lv_label_set_text(labelBattery, std::to_string(battery).c_str());
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


void showiTagStatus()
{
  ESP_LOGI(TAG,"----- Active tags: -----");
  for(int j=0; j<ITAG_COUNT; j++)
  {
    if(iTags[j].active) {
      ESP_LOGI(TAG,"Active: %s %d%% Laps: %5d | %s", iTags[j].connected? "#":" ", iTags[j].battery ,iTags[j].laps , iTags[j].name.c_str());
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
      if(advertisedDevice->getAddress().toString() == iTags[j].address) {
        ESP_LOGI(TAG,"Scaning iTAGs MATCH: %s",String(advertisedDevice->toString().c_str()).c_str());

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
};

// Called when BT scanning has ended, start a new in non is found
void scanBTCompleteCB(NimBLEScanResults scanResults)
{
  // Only restart scanning if not connecting
  if (doConnect<0) {
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
    if (doConnect>=0) {
        /* Found a device we want to connect to, do it now */
        if (iTags[doConnect].connect(advDevice)) {
            ESP_LOGI(TAG,"Connect %d Success!, scanning for more!",doConnect);
        } else {
            ESP_LOGI(TAG,"Connect %d Failed to connect, starting scan",doConnect);
        }

        doConnect = -1;
        NimBLEDevice::getScan()->start(BT_SCAN_TIME, scanBTCompleteCB);
    }
  // Show connection in log
  uint32_t now = millis();
  if(now - lastScanTime > SCAN_INTERVAL) { 
    lastScanTime = now;
    showiTagStatus();
  }
}
