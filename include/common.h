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


#define MINIMUM_LAP_TIME_IN_SECONDS 1*60

extern bool raceOngoing;

void startRace();