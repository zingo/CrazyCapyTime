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

#ifdef TESTCODE

#include <ESP32Time.h>
#include <NimBLEDevice.h>
#include "common.h"
#include "messages.h"

#define TAG "TestEndToEnd"

static TaskHandle_t xHandleTestEndToEnd = nullptr;

enum class EndToEndTest : uint32_t
{
    Test24HFast,
    Test24HLive,
    StopTest
};

static void Test24H(enum class EndToEndTest testEndToEnd);


enum class EndToEndTest EndToEndTestString2Enum (std::string const& inString) {
    if (inString == "Test24HFast") return EndToEndTest::Test24HFast;
    if (inString == "Test24HLive") return EndToEndTest::Test24HLive;
    if (inString == "StopTest") return EndToEndTest::StopTest;
    return EndToEndTest::StopTest;
}

std::string EndToEndTestEnum2String (enum class EndToEndTest enu) {
    if (enu == EndToEndTest::Test24HFast) return std::string("Test24HFast");
    if (enu == EndToEndTest::Test24HLive) return std::string("Test24HLive");
    if (enu == EndToEndTest::StopTest) return std::string("StopTest");
    return std::string("StopTest");
}

void executeEndToEndTest(enum class EndToEndTest enu) {
    if (enu == EndToEndTest::Test24HFast) return Test24H(enu);
    if (enu == EndToEndTest::Test24HLive) return Test24H(enu);
    if (enu == EndToEndTest::StopTest) return vTaskDelete( NULL );
    return;
}


static void Test24H(enum class EndToEndTest testEndToEnd)
{

  std::string testTag("ff:ff:10:7e:be:67"); // Zingo
  NimBLEAddress bleAddress(testTag);
  time_t start = rtc.getEpoch();
  uint32_t startIn = 15;
  uint32_t lapDist = 821; //meter
  // Max speed is 2,83min/km (or 170s/km e.g. Marathon on 2h) on the lap, this is used to not count a new lap in less time then this
  time_t blockNewLapTime = ((170*lapDist)/1000); //seconds


  // 24000m on 24h -> laps=(240000.0/lapDist) AvgLapTime=24*60*60/laps
  uint32_t wantedDist=180*1000;
  uint32_t wantedLaps=(wantedDist/lapDist)+1;
  time_t AvgLapTime = 24*60*60/wantedLaps; //blockNewLapTime+2;

  ESP_LOGI(TAG,"EndToEnd Test: %s > START 24H Race, LapDist:%d wantedDist:%d wantedLaps:%d -> AvgLapTime:%d (blockedTime:%d)\n", EndToEndTestEnum2String(testEndToEnd).c_str(),lapDist,wantedDist,wantedLaps,AvgLapTime,blockNewLapTime);


  // Setup a iTag
  // Fake a "ping"
  ESP_LOGI(TAG,"EndToEnd Test: %s > SETUP TAG\n", EndToEndTestEnum2String(testEndToEnd).c_str());

  {
    msg_RaceDB msg;
    msg.iTag.header.msgType = MSG_ITAG_DETECTED;
    msg.iTag.time = start;
    msg.iTag.address = static_cast<uint64_t>(bleAddress);
    msg.iTag.RSSI = INT8_MIN;
    msg.iTag.battery = 78;
    BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 0 )); //try without wait
    if (!xReturned)
    {
      ESP_LOGE(TAG,"ERROR iTAG detected queue is full: %s RETRY for 1s",String(bleAddress.toString().c_str()).c_str());
      xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 1000 )); //just wait a short while
      if (!xReturned)
      {
        ESP_LOGE(TAG,"ERROR ERROR iTAG detected queue is still full: %s trow a way detected",String(bleAddress.toString().c_str()).c_str());
        //TODO do something clever ??? Collect how many?
      }
    }
  }
  
  // Fake a "Setup done"
  {
    // Send response/activate iTag
    msg_RaceDB msgReponse;
    msgReponse.iTag.header.msgType = MSG_ITAG_CONFIGURED;
    msgReponse.iTag.address = static_cast<uint64_t>(bleAddress);
    msgReponse.iTag.battery = 78;
    msgReponse.iTag.RSSI = -57;
    msgReponse.iTag.time = start;

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

  // Setup a race
  ESP_LOGI(TAG,"EndToEnd Test: %s > SETUP RACE\n", EndToEndTestEnum2String(testEndToEnd).c_str());
  {
    std::string configRaceFileName(EndToEndTestEnum2String(testEndToEnd).c_str());
    std::string configRaceName(EndToEndTestEnum2String(testEndToEnd).c_str());

    msg_RaceDB msg;
    msg.Broadcast.RaceConfig.header.msgType = MSG_RACE_CONFIG;
    size_t len = configRaceFileName.copy(msg.Broadcast.RaceConfig.fileName, PARTICIPANT_NAME_LENGTH);
    msg.Broadcast.RaceConfig.fileName[len] = '\0';
    len = configRaceName.copy(msg.Broadcast.RaceConfig.name, PARTICIPANT_NAME_LENGTH);
    msg.Broadcast.RaceConfig.name[len] = '\0';
    msg.Broadcast.RaceConfig.timeBasedRace = true;
    msg.Broadcast.RaceConfig.maxTime = 24;
    msg.Broadcast.RaceConfig.distance = 821;
    msg.Broadcast.RaceConfig.laps = 200; //NA when timeBasedRace = true 
    msg.Broadcast.RaceConfig.blockNewLapTime = blockNewLapTime;
    msg.Broadcast.RaceConfig.updateCloserTime = 30;
    msg.Broadcast.RaceConfig.raceStartInTime = startIn;

    ESP_LOGI(TAG,"Send: MSG_RACE_CONFIG MSG:0x%x filename:%s name:%d distace:%d laps:%d blockNewLapTime:%d updateCloserTime:%d, raceStartInTime:%d",
          msg.Broadcast.RaceConfig.header.msgType, msg.Broadcast.RaceConfig.fileName, msg.Broadcast.RaceConfig.name,msg.Broadcast.RaceConfig.distance, msg.Broadcast.RaceConfig.laps, 
          msg.Broadcast.RaceConfig.blockNewLapTime, msg.Broadcast.RaceConfig.updateCloserTime, msg.Broadcast.RaceConfig.raceStartInTime);

    BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 2000 )); // TODO add resend ?
    if (!xReturned) {
      // it it fails let the user click again
      ESP_LOGW(TAG,"WARNING: Send: MSG_RACE_CONFIG MSG:0x%x could not be sent in 2000ms. USER need to retry", msg.Broadcast.RaceConfig.header.msgType);
    }
  }

  // Start race
  ESP_LOGI(TAG,"EndToEnd Test: %s > START RACE\n", EndToEndTestEnum2String(testEndToEnd).c_str());
  startRaceCountdown();

  ESP_LOGI(TAG,"EndToEnd Test: %s > WAIT FOR RACE COUTNDOWN +2s: %d\n", EndToEndTestEnum2String(testEndToEnd).c_str(),startIn+2);
  delay((startIn+2)*1000); 

  ESP_LOGI(TAG,"EndToEnd Test: %s > RACE STARTED\n", EndToEndTestEnum2String(testEndToEnd).c_str());
  
  for(;;)
  {
    ESP_LOGI(TAG,"EndToEnd Test: %s > WAIT FOR LAP BLOCK +2s: %d\n", EndToEndTestEnum2String(testEndToEnd).c_str(),blockNewLapTime+2);

    if (testEndToEnd == EndToEndTest::Test24HFast) {
      // Fake time jump by adding time to RTC
      rtc.setTime(rtc.getEpoch()+10,0);
      delay((2)*1000);
      rtc.setTime(rtc.getEpoch()+AvgLapTime-12,0);
      delay((2)*1000);
    }
    else {
      delay((AvgLapTime)*1000);
    }
  
    ESP_LOGI(TAG,"EndToEnd Test: %s > TAG\n", EndToEndTestEnum2String(testEndToEnd).c_str());
    {
      msg_RaceDB msg;
      msg.iTag.header.msgType = MSG_ITAG_DETECTED;
      msg.iTag.time = rtc.getEpoch();
      msg.iTag.address = static_cast<uint64_t>(bleAddress);
      msg.iTag.RSSI = INT8_MIN;
      msg.iTag.battery = 78;
      BaseType_t xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 0 )); //try without wait
      if (!xReturned)
      {
        ESP_LOGE(TAG,"ERROR iTAG detected queue is full: %s RETRY for 1s",String(bleAddress.toString().c_str()).c_str());
        xReturned = xQueueSend(queueRaceDB, (void*)&msg, (TickType_t)pdMS_TO_TICKS( 1000 )); //just wait a short while
        if (!xReturned)
        {
          ESP_LOGE(TAG,"ERROR ERROR iTAG detected queue is still full: %s trow a way detected",String(bleAddress.toString().c_str()).c_str());
          //TODO do something clever ??? Collect how many?
        }
      }
    }
  }
}



void vTaskTestEndToEnd( void *pvParameters )
{
  ESP_LOGI(TAG,"EndToEnd Test: Start!");
  /* The parameter value is expected to be 2 as 2 is passed in the
     pvParameters value in the call to xTaskCreate() below. 
  */
  //configASSERT( ( ( uint32_t ) pvParameters ) == 2 );
  uint32_t data = ( uint32_t ) pvParameters;
  enum class EndToEndTest testEndToEnd = (enum class EndToEndTest) data;
  ESP_LOGI(TAG,"EndToEnd Test: %s > Started!\n", EndToEndTestEnum2String(testEndToEnd).c_str());

  executeEndToEndTest(testEndToEnd);


  vTaskDelete( NULL );
}


void stopTestEndToEnd()
{
  ESP_LOGI(TAG,"stopTestEndToEnd");

  if (xHandleTestEndToEnd != nullptr)
  {
    // Kill ongoing test
    ESP_LOGI(TAG,"stopTestEndToEnd: Test was ongoing kill it!");
    vTaskDelete( xHandleTestEndToEnd );
    xHandleTestEndToEnd = nullptr;
  }
}

void startTestEndToEnd(std::string testname)
{
  ESP_LOGI(TAG,"startTestEndToEnd: %s was selected\n", testname.c_str());
  // Make sure to kill a ongoing test before starting a new, if one is running
  stopTestEndToEnd();

  ESP_LOGI(TAG,"startTestEndToEnd: Start a task for: %s\n", testname.c_str());
  enum class EndToEndTest pvParameters = EndToEndTestString2Enum(testname);
  ESP_LOGI(TAG,"startTestEndToEnd: Enum used: %s\n", EndToEndTestEnum2String(pvParameters).c_str());
  if (pvParameters == EndToEndTest::StopTest)
  {
    return;
  }
  uint32_t data = ( uint32_t ) pvParameters;
  // Start Task
  BaseType_t xReturned;
  /* Create the task, storing the handle. */
  xReturned = xTaskCreate(
                  vTaskTestEndToEnd,       /* Function that implements the task. */
                  "TestEndToEnd",          /* Text name for the task. */
                  4*1024,                 /* Stack size in words, not bytes. */
                  (void *)data,               /* Parameter passed into the task. */
                  0,                      /* Priority  0-(configMAX_PRIORITIES-1)   idle = 0 = tskIDLE_PRIORITY*/
                  &xHandleTestEndToEnd );         /* Used to pass out the created task's handle. */

  if( xReturned != pdPASS )
  {
    ESP_LOGE(TAG,"FATAL ERROR: xTaskCreate(vTaskTestEndToEnd, TestEndToEnd,..) Failed --- Do nothing");
  }
}



#endif // #ifdef TESTCODE
