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
#include "common.h"
#include "messages.h"
#include "gui.h"
#include "iTag.h"
#include "bluetooth.h"
#define TAG "Main"

//ESP32Time rtc(3600);  // offset in seconds GMT+1
ESP32Time rtc(0);  // use epoc as race start TODO use real RCT time from HW or NTP

uint32_t raceStartIn = 0;
bool raceOngoing = false;


QueueHandle_t queueRaceDB;
QueueHandle_t queueBTConnect;
QueueHandle_t queueGFX; 

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
void BroadcastRaceClear()
{
  msg_RaceDB msg;
  msg.Broadcast.RaceStart.header.msgType = MSG_RACE_CLEAR;  // We send this to "Clear data" before countdown, this would be what a user expect
  //ESP_LOGI(TAG,"Send: MSG_RACE_CLEAR MSG:0x%x",msg.Broadcast.RaceStart.header.msgType);
  xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress

  msg_GFX msgGFX;
  msgGFX.Broadcast.RaceStart.header.msgType = MSG_RACE_CLEAR;  // We send this to "Clear data" before countdown, this would be what a user expect
  //ESP_LOGI(TAG,"Send: MSG_RACE_CLEAR MSG:0x%x",msgGFX.Broadcast.RaceStart.header.msgType);
  xQueueSend(queueGFX, (void*)&msgGFX, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress
}

// Don't touch data, just send messages, can be used from any context
void BroadcastRaceStart(time_t raceStartTime)
{
  msg_RaceDB msg;
  msg.Broadcast.RaceStart.header.msgType = MSG_RACE_START;  // We send this to "Clear data" before countdown, this would be what a user expect
  msg.Broadcast.RaceStart.startTime = raceStartTime;
  //ESP_LOGI(TAG,"Send: MSG_RACE_START MSG:0x%x startTime:%d",msg.Broadcast.RaceStart.header.msgType,msg.Broadcast.RaceStart.startTime);
  xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress


  msg_GFX msgGFX;
  msgGFX.Broadcast.RaceStart.header.msgType = MSG_RACE_START;  // We send this to "Clear data" before countdown, this would be what a user expect
  msgGFX.Broadcast.RaceStart.startTime = raceStartTime;
  //ESP_LOGI(TAG,"Send: MSG_RACE_START MSG:0x%x startTime:%d",msgGFX.Broadcast.RaceStart.header.msgType,msgGFX.Broadcast.RaceStart.startTime);
  xQueueSend(queueGFX, (void*)&msgGFX, (TickType_t)pdMS_TO_TICKS( 2000 ));  //No check for error, user will see problem in UI and repress
  
}


void startRaceCountdown()
{
  rtc.setTime(0,0);  // TODO remove, for now EPOCH is used for the countdown as RACE_COUNTDOWN-EPOCH
  //time_t raceStartTime = 0; // TODO remove, for now EPOCH is used for the countdown as RACE_COUNTDOWN-EPOCH
  raceStartIn = RACE_COUNTDOWN; //seconds
  raceOngoing = false;

  BroadcastRaceClear();
  //BroadcastRaceStart(raceStartTime);
}

void startRace()
{
  rtc.setTime(0,0);  // TODO remove when we have RTC HW  and save reace start instead
  time_t raceStartTime = 0; //TODO remove when we have RTC HW  and save reace start instead
  raceStartIn = 0;
  raceOngoing = true;

  //BroadcastRaceClear();
  BroadcastRaceStart(raceStartTime);
}

void showHeapInfo()
{
  ESP_LOGI(TAG, "------------------------------------------ freeHeap: %9d bytes", xPortGetFreeHeapSize());
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

void setup()
{
  raceStartIn = 0;
  raceOngoing = false;
  //delay(1000);
  Serial.begin(115200);
  ESP_LOGI(TAG, "Crazy Capy Time setup");

  initMessageQueues(); // Must be called before starting all tasks as theu might use the messages queues
  initLVGL();
  initBluetooth();
  initRaceDB();
  initLittleFS();
  ESP_LOGI(TAG, "Setup done switching to running loop");

}

void loop()
{
  if(raceStartIn > 0) {
    int newRaceStartIn = RACE_COUNTDOWN - rtc.getEpoch();
    if(newRaceStartIn<=0) {
      //Countdown 0
      raceStartIn = 0;
      startRace();
    }
     else {
      raceStartIn = newRaceStartIn;
    }
  }

  //ESP_LOGI(TAG,"Time: %s\n",rtc.getTime("%Y-%m-%d %H:%M:%S").c_str()); // format options see https://cplusplus.com/reference/ctime/strftime/
  loopHandlLVGL();
  delay(5);

  static unsigned long lastTimeUpdate = 0;
  unsigned long now = rtc.getEpoch();
  if ((lastTimeUpdate+5*60) <= now) { //one per 5 min
    lastTimeUpdate = now;
    showHeapInfo(); //Monitor heap to see if memory leaks
  }
}