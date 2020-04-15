/**
 * Settings to be defined/coordinated among multiple components of mmControl.
 */
#ifndef MM_CONTROL_SETTINGS_H
#define MM_CONTROL_SETTINGS_H

#define COPY_TO_SERIAL      // if defined, logged messages should be immediately printed on Serial
#define LOGBUF_LENGTH 51200 // log buffer size; default defined by logBuffer.h for testing is 16

// timing settings
#define DEEPSLEEP_INACTIVITY 15 * 60 * 1000                     // [millis] - 15min of inactivity before going to sleep
#define DEEPSLEEP_WAITPERIOD 20 * 60 * 1000                     // [millis] - wait 5min before performing deep sleep, must include DEEPSLEEP_INACTIVITY
#define DEEPSLEEP_MAXTIME (unsigned long)60 * 60 * 1000 * 1000  // in [microseconds], max value is ~70min
#define DEEPSLEEP_DISALLOW_HOURS (nowHour < 23 && nowHour >= 6) // sleep allowed from 23:00 to 6:00
//#define DEEPSLEEP_DISALLOW_HOURS (timeClient.getMinutes() % 2 == 0) // for testing: going to sleep every odd minute

// physical pin definitions
#define DEEPSLEEP_BUTTON GPIO_NUM_39 // should be same as PIN_BUTTON
#define PIN_BUTTON 39                // GPIO36 aka RTS_GPIO03
#define PIN_LED_POWER 2              // GPIO2 for internal LED
#define PIN_LED_STATUS 15            // GPIO13 for (external) status LED
#define PIN_IR_RECEIVER 16           // GPIO16 for IR receiver
#define PIN_IR_TRANSMITTER 17        // GPIO17 for IR transmitter (with 5V-driving circuit)

// use adjacent pins for relais control
#define PIN_RELAIS_USB1 26
#define PIN_RELAIS_USB2 27
#define PIN_RELAIS_GRID1 32
#define PIN_RELAIS_GRID2 25

#endif
