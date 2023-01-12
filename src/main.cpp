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
#include "common.h"
#include "gui.h"
#include "iTag.h"

#define TAG "Main"

ESP32Time rtc(3600);  // offset in seconds GMT+1

void setup()
{
  //delay(1000);
  Serial.begin(115200);
  ESP_LOGI(TAG, "Crazy Capy Time setup");

  initLVGL();
  initiTAGs();
  ESP_LOGI(TAG, "Setup done switching to running loop");

}


void loop()
{
  //ESP_LOGI(TAG,"Time: %s\n",rtc.getTime("%Y-%m-%d %H:%M:%S").c_str()); // format options see https://cplusplus.com/reference/ctime/strftime/
  loopHandlLVGL();
  loopHandlTAGs();
  delay(5);
  //ESP_LOGI(TAG, "loop done");
}