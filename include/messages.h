#pragma once

struct msgHeader
{
  uint32_t msgType; //Must be first in all msg, used to interpertate and select rest of struct
};


// ##################### Send to more the one queue e.g. Broadcast type of messages

// Sent before race start and before loading a race
struct msg_RaceClear
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
};

// Sent when race start
struct msg_RaceStart
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  time_t startTime;
};


// Used during setup, load or editing a race
struct msg_RaceConfig
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  char fileName[RACE_NAME_LENGTH+1]; // add one for nulltermination
  char name[RACE_NAME_LENGTH+1]; // add one for nulltermination
  bool timeBasedRace;
  time_t maxTime;  // in hours
  uint32_t distance; //timeBasedRace=false: race distance, timeBasedRace=true: This is lap distance
  uint32_t laps; //timeBasedRace=false: laps in race -> calulate lap distance, timeBasedRace=true: NA
  time_t blockNewLapTime;
  time_t updateCloserTime;
  time_t raceStartInTime; // Race Start Countdown time in seconds
};

union msg_BroadcastMessages
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  msg_RaceClear RaceClear;
  msg_RaceStart RaceStart;
  msg_RaceConfig RaceConfig;
};

#define MSG_RACE_CLEAR        0xffff0000 //msg_RaceClear queueRaceDB, queueGFX
#define MSG_RACE_START        0xffff0001 //msg_RaceStart queueRaceDB, queueGFX
#define MSG_RACE_CONFIG       0xffff0002 //msg_RaceConfig queueRaceDB, queueGFX


// ##################### Send to queueBTConnect

struct msg_iTagDetected //TODO creat union see union msg_GFX for inspiration
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  time_t time;
  uint64_t address;
  int8_t RSSI;
  int8_t battery;
};

#define MSG_ITAG_CONFIG                  0x1000 //msg_iTagDetected queueBTConnect


// ##################### Send to queueRaceDB

struct msg_AddParticipantResponse
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  uint32_t handleDB;
  uint32_t handleGFX;
  bool wasOK;
};

// Used when editing a user
struct msg_UpdateParticipantInDB
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  uint32_t handleDB;
  uint32_t handleGFX;
  uint32_t color0;
  uint32_t color1;
  char name[PARTICIPANT_NAME_LENGTH+1]; // add one for nulltermination
  bool inRace; // Use inRace to move participant in/out of race table in GUI
};

struct msg_UpdateParticipantRaceStatus
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  uint32_t handleDB;
  uint32_t handleGFX;
  bool inRace;         // Is the user in a race or not
};

struct msg_UpdateParticipantLapCount
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  uint32_t handleDB;
  uint32_t handleGFX;
  int32_t lapDiff;  //Value is added to lap negative values will result in a subtraction.
};

struct msg_LoadSaveRace
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  // TODO Maybe we want a filename here later :)
};



// Send whenever a timer expiered
struct msg_Timer
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
};

union msg_RaceDB
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  msg_BroadcastMessages Broadcast;
  msg_iTagDetected iTag;
  msg_AddParticipantResponse AddedToGFX;
  msg_UpdateParticipantInDB UpdateParticipant;
  msg_UpdateParticipantRaceStatus UpdateParticipantRaceStatus;
  msg_UpdateParticipantLapCount UpdateParticipantLapCount;
  msg_LoadSaveRace LoadRace;
  msg_LoadSaveRace SaveRace;
  msg_Timer Timer;
};

// TODO this chould be class enum to avoid typo misstakes
#define MSG_ITAG_DETECTED                0x2000 //msg_iTagDetected queueRaceDB
#define MSG_ITAG_CONFIGURED              0x2001 //msg_iTagDetected queueRaceDB
#define MSG_ITAG_GFX_ADD_USER_RESPONSE   0x2002 //msg_AddParticipantResponse queueRaceDB
#define MSG_ITAG_UPDATE_USER             0x2003 //msg_UpdateParticipantInDB queueRaceDB
#define MSG_ITAG_UPDATE_USER_RACE_STATUS 0x2004 //msg_UpdateParticipantRaceStatus queueRaceDB
#define MSG_ITAG_UPDATE_USER_LAP_COUNT   0x2005 //msg_UpdateParticipantRaceStatus queueRaceDB
#define MSG_ITAG_LOAD_RACE               0x2006 //msg_LoadSaveRace queueRaceDB
#define MSG_ITAG_SAVE_RACE               0x2007 //msg_LoadSaveRace queueRaceDB
// "internal" update GUI timer tick
#define MSG_ITAG_TIMER_2000              0x2100 //msg_Timer queueRaceDB


// ##################### Send to queueGFX


// Startup setup of all participants in GUI will respond with MSG_ITAG_GFX_ADD_USER_RESPONSE containing the handleGFX that is later used
struct msg_AddParticipant
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  uint32_t handleDB;
  uint32_t color0;
  uint32_t color1;
  char name[PARTICIPANT_NAME_LENGTH+1]; // add one for nulltermination
  bool inRace;
};

// Used when loading or reconfig a user
struct msg_UpdateParticipant
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  uint32_t handleGFX;
  uint32_t color0;
  uint32_t color1;
  char name[PARTICIPANT_NAME_LENGTH+1]; // add one for nulltermination
  bool inRace; // Use inRace to move participant in/out of race table in GUI
};

// Sent now and then during a Race to update GUI
// Special combination 
// If laps is one more then last sent -> new lap is setup in GUI
// LastSeenTime will be updated in graph if connectionStatus is <0 (connected)
struct msg_UpdateParticipantData
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  uint32_t handleGFX;
  uint32_t distance;
  uint32_t laps;
  time_t lastLapTime;  //seconds since start of Race, This is used with laps ONLY for newLap registration
  time_t lastSeenTime; //seconds since start of Race, If this and laps is present this updates lastSeenTime values in graph
  int8_t connectionStatus; //0 = not connected for long time, 1 not connected for short time  If <0 Connected now value is RSSI
  bool inRace;   // Is participand in a race of not
};

// Sent when non race thing needs to update GUI
struct msg_UpdateParticipantStatus
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  uint32_t handleGFX;
  int8_t connectionStatus; //0 = not connected for long time, 1 = not connected for short time  If <0 Connected now value is RSSI
  int8_t battery; // 0-100%
  bool inRace; // Use inRace to move participant in/out of race table in GUI
};

union msg_GFX
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct
  msg_BroadcastMessages Broadcast;
  msg_AddParticipant AddUser;
  msg_UpdateParticipant UpdateUser;
  msg_UpdateParticipantData UpdateUserData;
  msg_UpdateParticipantStatus UpdateStatus;
  msg_Timer Timer;
};

#define MSG_GFX_ADD_USER           0x3000 //msg_AddParticipant queueGFX
#define MSG_GFX_UPDATE_USER        0x3001 //msg_UpdateParticipant queueGFX
#define MSG_GFX_UPDATE_USER_DATA   0x3002 //msg_UpdateParticipantData queueGFX
#define MSG_GFX_UPDATE_USER_STATUS 0x3003 //msg_UpdateParticipantStatus queueGFX
// "internal" update GUI timer tick
#define MSG_GFX_TIMER              0x3100 //msg_Timer queueGFX

extern QueueHandle_t queueRaceDB;  // msg_RaceDB Task/Database manager is blocked reading from this
extern QueueHandle_t queueBTConnect;     // msg_iTagDetected Bluetooth task is blocked reading from this
extern QueueHandle_t queueGFX;           // msg_GFX GFX poll this (in main thread, so it's not blocket on reading)
