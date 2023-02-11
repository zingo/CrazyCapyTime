# CrazyCapyTime
Cheap race timer and lap counter based on ESP32 and iTag BT tags.

The idea is to be fairly ok time, but cheap the use case is small Cannonball style races 3-20 participants with selfserving depo that you pass each lap and where people take the time themself. Usually people run them more for fun then to be fast.

The selected HW is not good enough to detect crossing the exact finish line but should be good enough to detect each lap. A "Im finish" button could be added to solve the finish line. A button could be used by the ones that are ok to spend some seconds to walk up to it and press it, the fast ones can still take the tame themself.

But initially this project just aims to count the laps and show some stats.

If anyone else likes this and wants something similar please reuse what you like. Maybe we could share the software or even the hardware setup going forward.

## Usage
Currently all used tags are hardcoded in iTag.cpp in a table. As I plan to use a limited amount of tags 15-20ish this might be good enough for a while. But it would be nice to be able to autodetect new tags and configure them in the UI and save some list in the filesystem or on a SD card. But anyway right now you need to set ITAG_COUNT to the number of tags and edit the iTags[] with all the info, sorry about that.

## Future improvement ideas

Personal time taking on other races. One plan is to also use this
on time based races like 24h/6 days to count my own laps in parallel with the official and collect/calculate my own distance and have a personal screen where I can see how I compare to my own goals easily. Like a personal information screen. One idea is so use a smaller 


## Hardware
1 x Sunton ESP32-8048S043 4.3 inch 800×480 IPS RGB Display 
https://www.aliexpress.com/item/1005004788147691.html
(About €31)
Any ESP32 with a screen would work, this one has quite a good big screen so lots of into can be shown. And it is quite cheap.

I also found https://www.makerfabs.com/esp32-s3-parallel-tft-with-touch-4-3-inch.html (About $37)
that seem to be even better and has an RTC onboard that will make it possible to have power glitched during races, like for switching batteries.

Many x iTags BT5.0 version ITAG Anti-Lost Device
https://www.aliexpress.com/item/1005004558844093.html
(About €3.5 each, you need one per person to track)

Extra (good to have):

A USB power bank, anyone big enough to power the device during the intended race.

Some weatherproof transparent box for protection to put it in together with the powerbank during the race. Probably a cheap food box would be fine.

Future hardware add ons:

I will probably add a battery backup RTC to ensure timing if restarted during the race. I have the one below at home so I'll probably use it.

RTC (Not added yet)
AZDelivery RTC DS3231 I2C
https://www.amazon.se/dp/B076GP5B94
(About €7)
