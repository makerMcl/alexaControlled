#include <Arduino.h>
#include "IRremote.h"
#include "alexaControlSettings.h"
#include "universalUIglobal.h"

#ifndef DEVICE_COMMANDS_MAIN_H
#define DEVICE_COMMANDS_MAIN_H

static IRsend irsend = IRsend(PIN_IR_TRANSMITTER);

/** send RC5 code with 12 bits */
static word irRc5(int code, String cmd)
{
    irsend.sendRC5(code, 12);
    ui.logTrace() << "sent RC5-IR command: " << cmd << endl;
    return 114;
}
/** send RC6 code with 20 bits */
static word irRc6(int code, String cmd)
{
    irsend.sendRC6(code, 20);
    ui.logTrace() << "sent RC6-IR command: " << cmd << endl;
    return 20;
}

namespace DeviceTvMedion
{
static word onOff()
{
    return irRc5(0x4C, "on/off");
}
static word up()
{
    return irRc5(0x54, "up");
}
static word down()
{
    return irRc5(0x53, "down");
}
static word av()
{
    return irRc5(0x78, "av");
}
static word ok()
{
    return irRc5(0x75, "ok");
}
} // namespace DeviceTvMedion

namespace DeviceSoundbarPhilips
{
static word onOff()
{
    return irRc6(0x100C, "onOff");
}
}; // namespace DeviceSoundbarPhilips

#endif
