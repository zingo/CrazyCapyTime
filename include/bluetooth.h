#pragma once

#include <string>
#include <stdint.h>

/*
 initBluetooth() will start a Task that handle BT scanning and BT Connecton

 The task will start a BT scan and if a device with the name "iTAG*" shows up it
 will send a MSG_ITAG_DETECTED message to queueiTagDetected (RaceDB task) with
 the info.

    RaceDB will read and act on the message, if it is the first time since
    power on it sees a iTag it will send a MSG_ITAG_CONFIG back via queueBTConnect 

 If we recievs a MSG_ITAG_CONFIG message on queueBTConnect The BT scanning will
 temporary be stop while we connect to that iTag and configure it to not beep when
 out of range and read the battery level from it. We will then send a 
 MSG_ITAG_CONFIGURED to queueiTagDetected (RaceDB task) with battery level
 filled in.
*/

void initBluetooth();


/* Small conversion util so other files don't need to include Nimble headers */
std::string convertBLEAddressToString(uint64_t);