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

#define TASK_BT_PRIO 20
#define TASK_RACEDB_PRIO 10
#define TASK_GUI_PRIO 5

// Stack size in words, not bytes.
#define TASK_BT_STACK (6*1024)
#define TASK_RACEDB_STACK (70*1024)
#define TASK_GUI_STACK (70*1024)

extern TaskHandle_t xHandleBT;
extern TaskHandle_t xHandleRaceDB;
extern TaskHandle_t xHandleGUI;

void saveRace(); // Send signal to save race

void showHeapInfo(void);

// TODO move below to signals to remove access to global variables
void startRaceCountdown();
void continueRace(time_t raceStartTime);



enum class HWPlatform : uint8_t
{
    Sunton_800x480,
    MakerFab_800x480,
};

extern HWPlatform HW_Platform; // Setup befoer threads are created and never changes so it can be used by all threads.