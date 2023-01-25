#pragma once

#define MSG_ITAG_DETECTED   0x1000 //msg_iTagDetected queueiTagDetected
#define MSG_ITAG_CONFIG     0x1001 //msg_iTagDetected queueBTConnect
#define MSG_ITAG_CONFIGURED 0x1002 //msg_iTagDetected queueiTagDetected

struct msg_iTagDetected {
  uint32_t msgType;   
  time_t time;
  uint64_t address;
  int8_t RSSI;
  int8_t battery;
};

extern QueueHandle_t queueiTagDetected;  // msg_iTagDetected Task/Database manager is reading from this
extern QueueHandle_t queueBTConnect;     // msg_iTagDetected Bluetooth task is reading from this