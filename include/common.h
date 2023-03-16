#pragma once

#include <Arduino.h>
#include <ESP32Time.h>
#include <stdio.h>
#include <stdint.h>

#include "esp_log.h"

extern ESP32Time rtc;

#define RACE_LAPS 6
#define RACE_DISTANCE_TOTAL (42195)

#define RACE_NAME_LENGTH        30
#define PARTICIPANT_NAME_LENGTH 30

#define DRAW_MAX_LAPS_IN_CHART (RACE_LAPS+10) //10 extra to allow some extra laps in case of errors or wrong config

#define RACE_COUNTDOWN 15  // TODO use config setting instead of this
extern uint32_t raceStartIn;
extern bool raceOngoing;

// Broadcast...() - Don't touch data, just send messages, can be used from any context
void BroadcastRaceClear();
void BroadcastRaceStart(time_t raceStartTime);

void startRaceCountdown();
