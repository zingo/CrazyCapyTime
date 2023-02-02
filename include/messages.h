#pragma once

struct msgHeader
{
  uint32_t msgType; //Must be first in all msg, used to interpertate and select rest of struct
};

struct msg_iTagDetected //TODO creat union see union msg_GFX for inspiration
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  time_t time;
  uint64_t address;
  int8_t RSSI;
  int8_t battery;
};

struct msg_AddParticipantResponse
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  uint32_t handleDB;
  uint32_t handleGFX;
  bool wasOK;
};

union msg_RaceDB
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  msg_iTagDetected iTag;
  msg_AddParticipantResponse AddedToGFX;
};

#define MSG_ITAG_DETECTED   0x1000 //msg_iTagDetected queueRaceDB
#define MSG_ITAG_CONFIG     0x1001 //msg_iTagDetected queueBTConnect
#define MSG_ITAG_CONFIGURED 0x1002 //msg_iTagDetected queueRaceDB

#define MSG_ITAG_GFX_ADD_USER_RESPONSE 0x1003 //msg_AddParticipantResponse queueRaceDB

struct msg_AddParticipant
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  uint32_t handleDB;
  uint32_t color0;
  uint32_t color1;
  char name[PARTICIPANT_NAME_LENGTH+1]; // add one for nulltermination
  bool inRace;
};

struct msg_UpdateParticipant
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  uint32_t handleGFX;
  uint32_t distance;
  uint32_t laps;
  time_t lastlaptime;
};

struct msg_UpdateParticipantStatus
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  uint32_t handleGFX;
  uint8_t connectionStatus; //0 = not connected for long time, 1 not connected for short time  If <0 Connected now value is RSSI
  uint8_t battery;
  bool inRace;
};


union msg_GFX
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  msg_AddParticipant Add;
  msg_UpdateParticipant Update;
  msg_UpdateParticipantStatus UpdateStatus;
};


#define MSG_GFX_ADD_USER_TO_RACE 0x1004 //msg_AddParticipant queueGFX
#define MSG_GFX_UPDATE_USER 0x1005 //msg_UpdateParticipant queueGFX
#define MSG_GFX_UPDATE_STATUS_USER 0x1006 //msg_UpdateParticipantStatus queueGFX

extern QueueHandle_t queueRaceDB;  // msg_RaceDB Task/Database manager is blocked reading from this
extern QueueHandle_t queueBTConnect;     // msg_iTagDetected Bluetooth task is blocked reading from this
extern QueueHandle_t queueGFX;           // msg_GFX GFX poll this (in main thread, so it's not blocket on reading)
