#pragma once

#include <Arduino.h>
#include <ESP32Time.h>
#include <stdio.h>
#include <stdint.h>

#include "esp_log.h"

extern ESP32Time rtc;

#define RACE_NAME_LENGTH        30
#define PARTICIPANT_NAME_LENGTH 30

#define RACE_COUNTDOWN 15  // TODO use config setting instead of this
extern uint32_t raceStartIn;
extern bool raceOngoing;

#define TASK_BT_PRIO 20
#define TASK_RACEDB_PRIO 10
#define TASK_GUI_PRIO 5

// Stack size in words, not bytes.
#define TASK_BT_STACK (6*1024)
#define TASK_RACEDB_STACK (70*1024)
#define TASK_GUI_STACK (90*1024)

// The participant to show for goal in the graph
// 4 = ZINGO
#define DEFAULT_PARTICIPANT 4
// The participant the goal
#define DEFAULT_PARTICIPANT_GOAL (170*1000)

// If ALL_TAGS_TRIGGER_DEFAULT_PARTICIPANT is defined
// any of the registrated tags will trigger the DEFAULT_PARTICIPANT
// instead of the correct participant.
// This is a temporary fix for a one man race mode, e.g. using this as a
// your own race timetaking in a 24H race to get the special nice info from
// the graph view and ALSO give the increased lap detection of having more
// then one TAG.
// e.g. Im the only one using this and to make it less likely for fails
// or missed detection I will bring 2-3 tags.
// Plan is to make it possible to asign more TAGs per user in the future and
// remove this config from here.
#define ALL_TAGS_TRIGGER_DEFAULT_PARTICIPANT


extern TaskHandle_t xHandleBT;
extern TaskHandle_t xHandleRaceDB;
extern TaskHandle_t xHandleGUI;

void saveRace(); // Send signal to save race

void showHeapInfo(void);

// TODO move below to signals to remove access to global variables
void startRaceCountdown();
void continueRace(time_t raceStartTime);
void stopRace();


enum class HWPlatform : uint8_t
{
    Sunton_800x480,
    MakerFab_800x480,
};

extern HWPlatform HW_Platform; // Setup befoer threads are created and never changes so it can be used by all threads.