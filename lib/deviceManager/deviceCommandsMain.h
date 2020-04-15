#include <Arduino.h>
#include "IRremote.h"
#include "mmControlSettings.h"
#include "universalUIglobal.h"

#ifndef DEVICE_COMMANDS_MAIN_H
#define DEVICE_COMMANDS_MAIN_H

static IRsend irsend = IRsend(PIN_IR_TRANSMITTER);

/** send RC5 code with 12 bits */
static int irRc5(int code, const __FlashStringHelper *cmd)
{
    ui.logTrace() << F("sending RC5-IR command: ") << cmd << endl;
    const word start = micros();
    irsend.sendRC5(code, 12);
    // time check: allow 1 mark tolerance = (4+12*2)*889µsec = 24892
    if ((micros() - start) > 25000)
    {
        ui.logError() << F("ir-send failed: ") << cmd << endl;
        return -1;
    }
    return 114;
}
/** send RC6 code with 20 bits */
static int irRc6(int code, const byte bits, const __FlashStringHelper *cmd)
{
    irsend.sendRC6(code, bits);
    ui.logTrace() << F("sending RC6-IR command: ") << cmd << endl;
    return 20;
}

namespace DeviceTvMedion
{
static int onOff()
{
    return irRc5(0x4C, F("MedionTV on/off"));
}
static int up()
{
    return irRc5(0x54, F("up"));
}
static int down()
{
    return irRc5(0x53, F("down"));
}
static int av()
{
    return irRc5(0x78, F("av"));
}
static int ok()
{
    return irRc5(0x75, F("ok"));
}
} // namespace DeviceTvMedion

namespace DeviceSoundbarPhilips
{
static int onOff() { return irRc6(0x100C, 12, F("Soundbar on/off")); }
static int volumeUp() { return irRc6(0x1010, 13, F("Soundbar Volume+")); }
static int volumeDown() { return irRc6(0x8808, 13, F("Soundbar Volume-")); }
}; // namespace DeviceSoundbarPhilips

#endif
