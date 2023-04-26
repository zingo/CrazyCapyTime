/**
 *
 *
 * The MIT License (MIT)
 * Copyright © 2023 <Zingo Andersen>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the “Software”), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <ESP32Time.h>
#include "FS.h"
#include <LittleFS.h>
#include <Wire.h>
#include "common.h"
#include "messages.h"
#include "gui.h"
#include "iTag.h"
#include "bluetooth.h"
#define TAG "Main"
#include "RTClib.h"

//ESP32Time rtc(3600);  // offset in seconds GMT+1
ESP32Time rtc(0);  // use epoc as race start TODO use real RCT time from HW or NTP
RTC_PCF8563 rtcHW;

static unsigned long raceStartInEpoch = 0;
uint32_t raceStartIn = 0;
bool raceOngoing = false;


QueueHandle_t queueBTConnect = NULL;
QueueHandle_t queueRaceDB = NULL;
QueueHandle_t queueGFX = NULL; 

TaskHandle_t xHandleBT = NULL;
TaskHandle_t xHandleRaceDB = NULL;
TaskHandle_t xHandleGUI = NULL;

HWPlatform HW_Platform = HWPlatform::MakerFab_800x480;


void initRTC()
{
  if (HW_Platform == HWPlatform::MakerFab_800x480 ) {

    if (! rtcHW.begin()) {
      Serial.println("Couldn't find RTC");
      Serial.flush();
      while (1) delay(10);
    }

    if (rtcHW.lostPower()) {
      ESP_LOGW(TAG,"RTC is NOT initialized, let's set the time!");
      // When time needs to be set on a new device, or after a power loss, the
      // following line sets the RTC to the date & time this sketch was compiled
      rtcHW.adjust(DateTime(F(__DATE__), F(__TIME__)));
      // This line sets the RTC with an explicit date & time, for example to set
      // January 21, 2014 at 3am you would call:
      // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
      //
      // Note: allow 2 seconds after inserting battery or applying external power
      // without battery before calling adjust(). This gives the PCF8523's
      // crystal oscillator time to stabilize. If you call adjust() very quickly
      // after the RTC is powered, lostPower() may still return true.
    }
    // When time needs to be re-set on a previously configured device, the
    // following line sets the RTC to the date & time this sketch was compiled
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));

    // When the RTC was stopped and stays connected to the battery, it has
    // to be restarted by clearing the STOP bit. Let's do this to ensure
    // the RTC is running.
    rtcHW.start();
    DateTime now = rtcHW.now();
    rtc.setTime(now.unixtime());
    
  }
  ESP_LOGE(TAG,"Time of boot: %s\n", rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());
}


void initMessageQueues()
{
  // Well we have a lot of memory, so why not allow it, e.g. ITAG_COUNT  :)
  queueRaceDB = xQueueCreate(ITAG_COUNT, sizeof(msg_RaceDB));  // ITAG_COUNT x msg_RaceDB  
  if (queueRaceDB == 0){
    ESP_LOGE(TAG,"Failed to create queueRaceDB = %p\n", queueRaceDB);
    // TODO Something more clever here?
  }

  // lets just make the queue big enough for all (it should work to make it smaller)
  queueBTConnect = xQueueCreate(ITAG_COUNT, sizeof(msg_iTagDetected));  // ITAG_COUNT x msg_iTagDetected
  if (queueBTConnect == 0){
    ESP_LOGE(TAG,"Failed to create queueBTConnect = %p\n", queueBTConnect);
    // TODO Something more clever here?
  }

  // lets just make the queue big enough for all (it should work to make it smaller)
  queueGFX = xQueueCreate(ITAG_COUNT, sizeof(msg_GFX));  // ITAG_COUNT x msg_GFX
  if (queueGFX == 0){
    ESP_LOGE(TAG,"Failed to create queueGFX = %p\n", queueGFX);
    // TODO Something more clever here?
  }
}

// Don't touch data, just send messages, can be used from any context
static void BroadcastRaceClear()
{
  msg_RaceDB msg;
  msg.Broadcast.RaceStart.header.msgType = MSG_RACE_CLEAR;  // We send this to "Clear data" before countdown, this would be what a user expect
  ESP_LOGI(TAG,"Send: MSG_RACE_CLEAR MSG:0x%x",msg.Broadcast.RaceStart.header.msgType);
  xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress

  msg_GFX msgGFX;
  msgGFX.Broadcast.RaceStart.header.msgType = MSG_RACE_CLEAR;  // We send this to "Clear data" before countdown, this would be what a user expect
  ESP_LOGI(TAG,"Send: MSG_RACE_CLEAR MSG:0x%x",msgGFX.Broadcast.RaceStart.header.msgType);
  xQueueSend(queueGFX, (void*)&msgGFX, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress
}

// Don't touch data, just send messages, can be used from any context
static void BroadcastRaceStart(time_t raceStartTime)
{
  msg_RaceDB msg;
  msg.Broadcast.RaceStart.header.msgType = MSG_RACE_START;  // We send this to "Clear data" before countdown, this would be what a user expect
  msg.Broadcast.RaceStart.startTime = raceStartTime;
  ESP_LOGI(TAG,"Send: MSG_RACE_START MSG:0x%x startTime:%d",msg.Broadcast.RaceStart.header.msgType,msg.Broadcast.RaceStart.startTime);
  xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress


  msg_GFX msgGFX;
  msgGFX.Broadcast.RaceStart.header.msgType = MSG_RACE_START;  // We send this to "Clear data" before countdown, this would be what a user expect
  msgGFX.Broadcast.RaceStart.startTime = raceStartTime;
  ESP_LOGI(TAG,"Send: MSG_RACE_START MSG:0x%x startTime:%d",msgGFX.Broadcast.RaceStart.header.msgType,msgGFX.Broadcast.RaceStart.startTime);
  xQueueSend(queueGFX, (void*)&msgGFX, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress
}

// Don't touch data, just send messages, can be used from any context
static void BroadcastRaceStop()
{
  msg_RaceDB msg;
  msg.Broadcast.RaceStop.header.msgType = MSG_RACE_STOP;  // We send this to stop a ongoing race
  ESP_LOGI(TAG,"Send: MSG_RACE_STOP MSG:0x%x",msg.Broadcast.RaceStop.header.msgType);
  xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress

  msg_GFX msgGFX;
  msgGFX.Broadcast.RaceStop.header.msgType = MSG_RACE_STOP;  // We send this to stop a ongoing race
  ESP_LOGI(TAG,"Send: MSG_RACE_STOP MSG:0x%x",msgGFX.Broadcast.RaceStop.header.msgType);
  xQueueSend(queueGFX, (void*)&msgGFX, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress
}

void startRaceCountdown()
{
  ESP_LOGI(TAG,"================== startRaceCountdown() ================== ");
  unsigned long now = rtc.getEpoch();
  ESP_LOGI(TAG,"startRaceCountdown()");
  raceStartIn = RACE_COUNTDOWN; //seconds
  raceStartInEpoch = now + raceStartIn;
  raceOngoing = false;

  BroadcastRaceClear();
}

static void startRace()
{
  ESP_LOGI(TAG,"================== startRace() ================== ");
  raceStartInEpoch = rtc.getEpoch();
  tm timeNow = rtc.getTimeStruct();
  time_t raceStartTime = mktime(&timeNow);
  raceStartIn = 0;
  raceOngoing = true;

  //BroadcastRaceClear();
  BroadcastRaceStart(raceStartTime);
}

void stopRace()
{
  ESP_LOGI(TAG,"================== stopRace() ================== ");
  raceStartIn = 0;
  raceOngoing = false;
  BroadcastRaceStop();
}

//TODO add signal/message CONTINUE_RACE and move code below, called from from DBloadRace() if race is ongoinf when it was saved
void continueRace(time_t raceStartTime)
{
  ESP_LOGI(TAG,"================== continueRace() ================== ");
  raceStartIn = 0;
  raceOngoing = true;

  msg_GFX msgGFX;
  msgGFX.Broadcast.RaceStart.header.msgType = MSG_RACE_START;  // We send this to "Clear data" before countdown, this would be what a user expect
  msgGFX.Broadcast.RaceStart.startTime = raceStartTime;
  //ESP_LOGI(TAG,"Send: MSG_RACE_START MSG:0x%x startTime:%d",msgGFX.Broadcast.RaceStart.header.msgType,msgGFX.Broadcast.RaceStart.startTime);
  xQueueSend(queueGFX, (void*)&msgGFX, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress
}

void saveRace()
{
  msg_RaceDB msg;
  msg.SaveRace.header.msgType = MSG_ITAG_SAVE_RACE;
  //ESP_LOGI(TAG,"Send: MSG_ITAG_SAVE_RACE MSG:0x%x handleDB:0x%08x", msg.SaveRace.header.msgType);
  BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  
}

void showHeapInfo()
{
  ESP_LOGI(TAG, "       freeHeap:      %9d bytes", xPortGetFreeHeapSize());
  ESP_LOGI(TAG, "MALLOC_CAP_EXEC:      %9d bytes",heap_caps_get_free_size(MALLOC_CAP_EXEC));
  //ESP_LOGI(TAG, "MALLOC_CAP_32BIT:     %9d bytes",heap_caps_get_free_size(MALLOC_CAP_32BIT)); same as MALLOC_CAP_DEFAULT
  //ESP_LOGI(TAG, "MALLOC_CAP_8BIT:      %9d bytes",heap_caps_get_free_size(MALLOC_CAP_8BIT));  same as MALLOC_CAP_DEFAULT
  ESP_LOGI(TAG, "MALLOC_CAP_DMA:       %9d bytes",heap_caps_get_free_size(MALLOC_CAP_DMA));
/* seem to always be 0
  ESP_LOGI(TAG, "MALLOC_CAP_PID2:      %9d bytes",heap_caps_get_free_size(MALLOC_CAP_PID2));
  ESP_LOGI(TAG, "MALLOC_CAP_PID3:      %9d bytes",heap_caps_get_free_size(MALLOC_CAP_PID3));
  ESP_LOGI(TAG, "MALLOC_CAP_PID4:      %9d bytes",heap_caps_get_free_size(MALLOC_CAP_PID4));
  ESP_LOGI(TAG, "MALLOC_CAP_PID5:      %9d bytes",heap_caps_get_free_size(MALLOC_CAP_PID5));
  ESP_LOGI(TAG, "MALLOC_CAP_PID6:      %9d bytes",heap_caps_get_free_size(MALLOC_CAP_PID6));
  ESP_LOGI(TAG, "MALLOC_CAP_PID7:      %9d bytes",heap_caps_get_free_size(MALLOC_CAP_PID7));  */
  ESP_LOGI(TAG, "MALLOC_CAP_SPIRAM:    %9d bytes",heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  ESP_LOGI(TAG, "MALLOC_CAP_INTERNAL:  %9d bytes",heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  ESP_LOGI(TAG, "MALLOC_CAP_DEFAULT:   %9d bytes",heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
  //ESP_LOGI(TAG, "MALLOC_CAP_IRAM_8BIT: %9d bytes",heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT)); seem to always be 0
  //ESP_LOGI(TAG, "MALLOC_CAP_RETENTION: %9d bytes",heap_caps_get_free_size(MALLOC_CAP_RETENTION)); same as MALLOC_CAP_EXEC
  //ESP_LOGI(TAG, "MALLOC_CAP_RTCRAM:    %9d bytes",heap_caps_get_free_size(MALLOC_CAP_RTCRAM)); uniteresting for now
}

void initLittleFS() 
{
  if (!LittleFS.begin(true)) { // true = formatOnFail
    ESP_LOGE(TAG, "ERROR: Cannot start LittleFS");
    return;
  }
  ESP_LOGI(TAG, "LittleFS started");
}

#if 0
void i2cscan(int sda, int scl)
{
  byte error, address;
  int nDevices;
 
  Wire.begin(sda, scl);

  ESP_LOGI(TAG, "I2C Scanning (%d,%d)...",sda,scl); 
  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0) {
      ESP_LOGI(TAG, "I2C device found at address  0x%02x <--",address);  
      nDevices++;
    }
    else if (error==4) {
      ESP_LOGI(TAG, "I2C Unknown error at address 0x%02x",address);  
    }    
  }
  if (nDevices == 0) {
    ESP_LOGI(TAG, "No I2C devices found."); 
  }
  else {
    ESP_LOGI(TAG, "done");
  }
  Wire.end();
}
#endif

HWPlatform autoDetectHW()
{
//  i2cscan(19,20); //SUNTON_800x480
//  i2cscan(17,18); //MAKERFAB_800x480

  //SUNTON_800x480
  byte error;
  Wire.begin(19, 20);
  Wire.beginTransmission(0x5d); //GT911 Touch controller
  error = Wire.endTransmission();
  Wire.end();

  if (error == 0) {
    ESP_LOGI(TAG, "I2C GT911 Touch device found on pin 19,20 -> SUNTON_800x480"); 
    return HWPlatform::Sunton_800x480;
  }

  //MAKERFAB_800x480
  Wire.begin(17, 18);
  Wire.beginTransmission(0x5d); //GT911 Touch controller
  error = Wire.endTransmission();
  Wire.end();

  if (error == 0) {
    ESP_LOGI(TAG, "I2C GT911 Touch device found on pin 17,18 -> MAKERFAB_800x480"); 
    return HWPlatform::MakerFab_800x480;
  }

  ESP_LOGI(TAG, "No I2C GT911 Touch device found. Fallback to MAKERFAB_800x480"); 
  return HWPlatform::MakerFab_800x480; // Fall back in case of errors
}

void setup()
{
  raceStartIn = 0;
  raceOngoing = false;
  //delay(1000);
  Serial.begin(115200);
  ESP_LOGI(TAG, "Crazy Capy Time setup");

  HW_Platform = autoDetectHW();

  initMessageQueues(); // Must be called before starting all tasks as they might use the messages queues

  initLVGL();
  initBluetooth();
  initLittleFS();

  initRTC(); // After initLVGL as it setups wire-I2C
  delay(100); //TODO do we need this? Ideas is to see if autoloaded race is correct in graph
  initRaceDB();
  ESP_LOGI(TAG, "Setup done switching to running loop");

}



void loop()
{
  unsigned long now = rtc.getEpoch();

  if(raceStartIn > 0) {
    if(now>=raceStartInEpoch) {
      //Countdown 0 -> Start Race!!!!
      raceStartInEpoch = now;
      raceStartIn = 0;
      startRace();
    }
     else {
      // Update countdown
      raceStartIn = raceStartInEpoch - now;
    }
  }


  static unsigned long lastTimeUpdate = 0;
  if ((lastTimeUpdate+5*60) <= now) { //once per 5 min
    lastTimeUpdate = now;
    ESP_LOGI(TAG,"------------------------ Time: %s\n",rtc.getTime("%Y-%m-%d %H:%M:%S").c_str()); // format options see https://cplusplus.com/reference/ctime/strftime/
    showHeapInfo(); //Monitor heap to see if memory leaks
    ESP_LOGI(TAG,"Main   used stack: %d",uxTaskGetStackHighWaterMark(nullptr));
    ESP_LOGI(TAG,"BT     used stack: %d / %d",uxTaskGetStackHighWaterMark(xHandleBT),TASK_BT_STACK);
//#if NIMBLE_CFG_CONTROLLER
//    ESP_LOGI(TAG,"BT ll  used stack: %d",nimble_port_freertos_get_ll_hwm());
//#endif
//    ESP_LOGI(TAG,"BT hs  used stack: %d",nimble_port_freertos_get_hs_hwm());
    ESP_LOGI(TAG,"RaceDB used stack: %d / %d",uxTaskGetStackHighWaterMark(xHandleRaceDB),TASK_RACEDB_STACK);
    ESP_LOGI(TAG,"GUI    used stack: %d / %d",uxTaskGetStackHighWaterMark(xHandleGUI),TASK_GUI_STACK);
  }

  //ESP_LOGI(TAG,"Time: %s\n",rtc.getTime("%Y-%m-%d %H:%M:%S").c_str()); // format options see https://cplusplus.com/reference/ctime/strftime/
  delay(100);
}