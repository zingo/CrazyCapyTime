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

struct msg_UpdateParticipantRaceStatus
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  uint32_t handleDB;
  uint32_t handleGFX;
  bool inRace;
};

union msg_RaceDB
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  msg_iTagDetected iTag;
  msg_AddParticipantResponse AddedToGFX;
  msg_UpdateParticipantRaceStatus UpdateParticipantRaceStatus;
};

#define MSG_ITAG_CONFIG     0x1000 //msg_iTagDetected queueBTConnect


#define MSG_ITAG_DETECTED   0x2000 //msg_iTagDetected queueRaceDB
#define MSG_ITAG_CONFIGURED 0x2001 //msg_iTagDetected queueRaceDB

#define MSG_ITAG_GFX_ADD_USER_RESPONSE 0x2002 //msg_AddParticipantResponse queueRaceDB
#define MSG_ITAG_UPDATE_USER_RACE_STATUS 0x2003 //msg_UpdateParticipantRaceStatus queueRaceDB

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
  int8_t connectionStatus; //0 = not connected for long time, 1 not connected for short time  If <0 Connected now value is RSSI
};

struct msg_UpdateParticipantStatus
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  uint32_t handleGFX;
  int8_t connectionStatus; //0 = not connected for long time, 1 not connected for short time  If <0 Connected now value is RSSI
  int8_t battery;
  bool inRace;
};

union msg_GFX
{
  msgHeader header; //Must be first in all msg, used to interpertate and select rest of struct 
  msg_AddParticipant Add;
  msg_UpdateParticipant Update;
  msg_UpdateParticipantStatus UpdateStatus;
};


#define MSG_GFX_ADD_USER_TO_RACE 0x3004 //msg_AddParticipant queueGFX
#define MSG_GFX_UPDATE_USER 0x3005 //msg_UpdateParticipant queueGFX
#define MSG_GFX_UPDATE_STATUS_USER 0x3006 //msg_UpdateParticipantStatus queueGFX

extern QueueHandle_t queueRaceDB;  // msg_RaceDB Task/Database manager is blocked reading from this
extern QueueHandle_t queueBTConnect;     // msg_iTagDetected Bluetooth task is blocked reading from this
extern QueueHandle_t queueGFX;           // msg_GFX GFX poll this (in main thread, so it's not blocket on reading)
