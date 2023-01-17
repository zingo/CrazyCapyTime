#pragma once

#include <Arduino.h>
#include <ESP32Time.h>
#include <stdio.h>

#include "esp_log.h"

extern ESP32Time rtc;

#define RACE_DISTANCE_TOTAL (42195/6*7)
#define RACE_DISTANCE_LAP (42195/6)
//#define RACE_DISTANCE_LAP (400)
#define RACE_LAPS 7

// Max speed is 2,83min/km (or 170s/km e.g. Marathon on 2h) on the lap, this is used to not count a new lap in less time then this
#define MINIMUM_LAP_TIME_IN_SECONDS ((170*RACE_DISTANCE_LAP)/1000)

extern bool raceOngoing;

void startRace();