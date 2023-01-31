#pragma once

struct msg_iTagDetected {
  uint32_t msgType;
  time_t time;
  uint64_t address;
  int8_t RSSI;
  int8_t battery;
};

#define MSG_ITAG_DETECTED   0x1000 //msg_iTagDetected queueiTagDetected
#define MSG_ITAG_CONFIG     0x1001 //msg_iTagDetected queueBTConnect
#define MSG_ITAG_CONFIGURED 0x1002 //msg_iTagDetected queueiTagDetected

struct msg_Participant {
  uint32_t msgType;
  uint32_t color0;
  uint32_t color1;
  char name[PARTICIPANT_NAME_LENGTH+1]; // add one for nulltermination
  uint32_t distance;
  uint32_t laps;
  time_t lastlaptime;
};

#define MSG_GFX_ADD_USER_TO_RACE 0x1003 //msg_Participant queueGFX

extern QueueHandle_t queueiTagDetected;  // msg_iTagDetected Task/Database manager is reading from this
extern QueueHandle_t queueBTConnect;     // msg_iTagDetected Bluetooth task is reading from this
extern QueueHandle_t queueGFX;           // msg_Participant GFX read from this (in main thread, so it's not blocket on reading)
