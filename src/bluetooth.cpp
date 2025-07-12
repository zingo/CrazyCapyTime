#include <NimBLEDevice.h>
#include "bluetooth.h"

#include <string>

#include "common.h"
#include "messages.h"

#define TAG "BT"

#define BT_SCAN_TIME 5000 // in seconds
//static uint16_t appId = 1;

//std::string convertBLEAddressToString(uint64_t bleAddress64)
//{
//  NimBLEAddress bleAddress(bleAddress64);
//  return bleAddress.toString();
//}

std::string convertBLEAddressToString(uint64_t bleAddress64)
{
    char str[18];
    uint8_t *addr = (uint8_t *)&bleAddress64;
    // BLE addresses are usually little-endian in memory, so print in reverse
    snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X",
        addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    return std::string(str);
}


#if 0

//When the BLE Server sends a new button reading with the notify property
static void BTiTagButtonPressed(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                        uint8_t* pData, size_t length, bool isNotify) {
  ESP_LOGI(TAG,"Button PRESSED");
}

#endif

static bool BTupdateBattery( NimBLEClient* client, msg_iTagDetected &msg_iTag) {
    // Battery READ, NOTIFY
    static const BLEUUID batteryServiceUUID("0000180f-0000-1000-8000-00805f9b34fb");
    static const BLEUUID batteryCharacteristicUUID("00002a19-0000-1000-8000-00805f9b34fb");

    NimBLERemoteService* remoteService = client->getService(batteryServiceUUID);
    if (remoteService) {
      NimBLERemoteCharacteristic* remoteCharacteristic = remoteService->getCharacteristic(batteryCharacteristicUUID);
      if (remoteCharacteristic) {
        // Read the value of the characteristic.
        if(remoteCharacteristic->canRead()) {
          uint8_t bat = remoteCharacteristic->readValue<uint8_t>();
          ESP_LOGI(TAG,"Read battery value: 0x%x %d",bat,bat);

          msg_iTag.battery = static_cast<int8_t>(bat);
        }
  //          if(remoteCharacteristic->canNotify()) {
  //            remoteCharacteristic->registerForNotify(batteryNotifyCallback);
  //          }
        return true;
      }
    }
    return false;
}

static bool BTtoggleBeep(NimBLEClient* client, bool beep) {
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

static bool BTtoggleBeepOnLost(NimBLEClient* client, bool beep) {
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

static bool BTconnect(msg_iTagDetected &msg_iTag)
{
  NimBLEClient* client;
  NimBLEAddress bleAddress(convertBLEAddressToString(msg_iTag.address).c_str(),BLE_ADDR_PUBLIC);
  ESP_LOGI(TAG,"BT Connect %s", bleAddress.toString().c_str());

  client = BLEDevice::createClient();
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
  BTupdateBattery(client,msg_iTag);

  //BTtoggleBeep(client, true);  // Welcome/setup beep
  //delay(200);
  //BTtoggleBeep(client, false);

  client->disconnect(); // no need to stay connected
  return true;
}

bool doBTScan = true;

/* Define a class to handle the callbacks when advertisements are received */
class BLEScanCallbacks : public NimBLEScanCallbacks {

  void onResult(const NimBLEAdvertisedDevice* advertisedDevice)
  {
    //ESP_LOGI(TAG,"Scaning iTAGs Found: %s",String(advertisedDevice->toString().c_str()).c_str());

    size_t isiTag = advertisedDevice->getName().find("iTAG");
    if (isiTag != std::string::npos && isiTag == 0) {
      //ESP_LOGI(TAG,"Scaning iTAGs MATCH: %s",String(advertisedDevice->toString().c_str()).c_str());
      msg_RaceDB msg;
      msg.iTag.header.msgType = MSG_ITAG_DETECTED;
      msg.iTag.time = rtc.getEpoch();
      msg.iTag.address = static_cast<uint64_t>(advertisedDevice->getAddress());
      msg.iTag.RSSI = advertisedDevice->getRSSI();
      msg.iTag.battery = INT8_MIN;
      BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 0 )); //try without wait
      if (!xReturned)
      {
        ESP_LOGE(TAG,"ERROR iTAG detected queue is full: %s RETRY for 1s",std::string(advertisedDevice->toString().c_str()).c_str());
        xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 1000 )); //just wait a short while
        if (!xReturned)
        {
          ESP_LOGE(TAG,"ERROR ERROR iTAG detected queue is still full: %s trow a way detected",std::string(advertisedDevice->toString().c_str()).c_str());
          //TODO do something clever ??? Collect how many?
        }
      }
    }
  }
  void onScanEnd(const NimBLEScanResults & 	scanResults, int 	reason )
  {
    ESP_LOGI(TAG,"BT SCAN Scaning iTAGs Ended reason: %d", reason);

    // Only restart scanning if not connecting
    if (doBTScan) {
      //ESP_LOGI(TAG,"BT SCAN restart %u ms", BT_SCAN_TIME);
      NimBLEDevice::getScan()->start(BT_SCAN_TIME, true, true);
    }
    else {
      ESP_LOGI(TAG,"BT SCAN NOT restarted, waiting for restart");
    }
  }
};

static void vTaskBTConnect( void *pvParameters )
{
  /* The parameter value is expected to be 1 as 1 is passed in the
     pvParameters value in the call to xTaskCreate() below. 
  */
  //configASSERT( ( ( uint32_t ) pvParameters ) == 1 );

  BLEScan* pBLEScan;
  ESP_LOGI(TAG,"BLEDevice init");
  NimBLEDevice::init("");

  pBLEScan = NimBLEDevice::getScan();

  // create a callback that gets called when advertisers are found
  pBLEScan->setScanCallbacks(new BLEScanCallbacks(), true);

  // Set scan interval (how often) and window (how long) in milliseconds
  pBLEScan->setInterval(97);
  pBLEScan->setWindow(67);

  // Active scan will gather scan response data from advertisers but will use more energy from both devices
  pBLEScan->setActiveScan(true);

  // Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
  doBTScan = true;  // used by the callback to autorestart BT scan
  ESP_LOGI(TAG,"BT SCAN start %u ms", BT_SCAN_TIME);
  pBLEScan->start(BT_SCAN_TIME, true, true);

  for( ;; )
  {
    ESP_LOGI(TAG,"Wait for BT Connect");
    msg_iTagDetected msg_iTag;
    if( xQueueReceive(queueBTConnect, &(msg_iTag), (TickType_t)portMAX_DELAY) == pdPASS)
    {
      switch(msg_iTag.header.msgType) {
        case MSG_ITAG_CONFIG:
        {
          ESP_LOGI(TAG,"received: MSG_ITAG_CONFIG");
          ESP_LOGI(TAG,"BT Connect SCAN Stop");

          doBTScan = false;
          NimBLEDevice::getScan()->stop();

          BTconnect(msg_iTag); //Will update battery
          
          ESP_LOGI(TAG,"BT Connect SCAN Start");
          doBTScan = true;
          NimBLEDevice::getScan()->start(BT_SCAN_TIME, true, true);

          // Send response/activate iTag
          msg_RaceDB msgReponse;
          msgReponse.iTag.header.msgType = MSG_ITAG_CONFIGURED;
          msgReponse.iTag.address = msg_iTag.address;
          msgReponse.iTag.battery = msg_iTag.battery;
          msgReponse.iTag.RSSI = msg_iTag.RSSI;
          msgReponse.iTag.time = msg_iTag.time;

          ESP_LOGI(TAG,"send: MSG_ITAG_CONFIGURED");
          BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msgReponse, (TickType_t)pdMS_TO_TICKS( 0 )); //try without wait
          if (!xReturned)
          {
            ESP_LOGE(TAG,"ERROR iTAG detected/configured queue is full RETRY for 1s");
            xReturned = xQueueSend(queueRaceDB, (void*)&msgReponse, (TickType_t)pdMS_TO_TICKS( 1000 )); //just wait a short while
            if (!xReturned)
            {
            ESP_LOGE(TAG,"ERROR iTAG detected/configured queue is full IGNORE");
              //TODO do something clever ??? Collect how many?
            }
          }
        }
        break;
        default:
          ESP_LOGE(TAG,"ERROR received bad msg: 0x%" PRIx32 "",msg_iTag.header.msgType);
          break;
      }
    }
  }
    vTaskDelete( NULL ); // Should never be reached
}

void initBluetooth()
{
  // Start BT Task (scan and inital connect&config)
  BaseType_t xReturned;
  /* Create the task, storing the handle. */
  xReturned = xTaskCreate(
                  vTaskBTConnect,       /* Function that implements the task. */
                  "BTConnect",          /* Text name for the task. */
                  TASK_BT_STACK,        /* Stack size in words, not bytes. */
                  NULL,                 /* Parameter passed into the task. */
                  TASK_BT_PRIO,         /* Priority  0-(configMAX_PRIORITIES-1)   idle = 0 = tskIDLE_PRIORITY*/
                  &xHandleBT );         /* Used to pass out the created task's handle. */

  if( xReturned != pdPASS )
  {
    ESP_LOGE(TAG,"FATAL ERROR: xTaskCreate(vTaskBTConnect, BTConnect,..) Failed");
    ESP_LOGE(TAG,"----- esp_restart() -----");
    esp_restart();
  }
}

