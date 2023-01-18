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
#include "gui.h"
#include "iTag.h"
#define TAG "Main"

//ESP32Time rtc(3600);  // offset in seconds GMT+1
ESP32Time rtc(0);  // use epoc as race start TODO use real RCT time from HW or NTP

uint32_t raceStartIn = 0;
bool raceOngoing = false;

void startRaceCountdown()
{
  rtc.setTime(0,0);  // TODO remove, for now EPOCH is sued for the countdown as RACE_COUNTDOWN-EPOCH
  startRaceiTags();
  raceStartIn = RACE_COUNTDOWN; //seconds
  raceOngoing = false;
}

void startRace()
{
  rtc.setTime(0,0);
  startRaceiTags();
  raceStartIn = 0;
  raceOngoing = true;
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

  initLVGL();
  initiTAGs();
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
  loopHandlTAGs();
  delay(5);
  //ESP_LOGI(TAG, "loop done");
}