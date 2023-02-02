#pragma once

#include <Arduino.h>
#include <ESP32Time.h>
#include <stdio.h>
#include <stdint.h>

#include "esp_log.h"

extern ESP32Time rtc;

#define RACE_LAPS 6
#define RACE_DISTANCE_TOTAL (42195/6*RACE_LAPS)
#define RACE_DISTANCE_LAP (42195/6)
//#define RACE_DISTANCE_LAP (400)

#define PARTICIPANT_NAME_LENGTH 10

#define DRAW_MAX_LAPS_IN_CHART (RACE_LAPS+10) //10 extra to allow some extra laps in case of errors or wrong config

// Max speed is 2,83min/km (or 170s/km e.g. Marathon on 2h) on the lap, this is used to not count a new lap in less time then this
//#define MINIMUM_LAP_TIME_IN_SECONDS ((170*RACE_DISTANCE_LAP)/1000)

#define MINIMUM_LAP_TIME_IN_SECONDS 60


#define RACE_COUNTDOWN 15
extern uint32_t raceStartIn;
extern bool raceOngoing;

void startRaceCountdown();
void loadRace();
void saveRace();
