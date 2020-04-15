#include <Arduino.h>
#include "mmControlSettings.h"
#include "universalUIglobal.h"

#define RELAIS_ACTIVE LOW

namespace DeviceUsb1
{
static int on()
{
    digitalWrite(PIN_RELAIS_USB1, RELAIS_ACTIVE);
    ui.logTrace() << F("switched USB1 = on") << endl;
    return 1;
}
static int off()
{
    digitalWrite(PIN_RELAIS_USB1, !RELAIS_ACTIVE);
    ui.logTrace() << F("switched USB1 = off") << endl;
    return 1;
}
}; // namespace DeviceUsb1

namespace DeviceGrid1
{
static byte _usedMarker = 0;
static int switchDevice(const byte pin, const byte on, const byte indicatorBit)
{
    if (on)
    {
        _usedMarker |= indicatorBit;
    }
    else
    {
        _usedMarker &= ~indicatorBit;
    }
    ui.logTrace() << F("switching grid1(") << indicatorBit << F(") = ") << (on ? F("on") : F("off")) << F(", marker=") << _usedMarker << endl;
    digitalWrite(pin, _usedMarker > 0 ? RELAIS_ACTIVE : !RELAIS_ACTIVE);
    return 1;
}
static int device1on() { return switchDevice(PIN_RELAIS_GRID1, true, 1); }
static int device1off() { return switchDevice(PIN_RELAIS_GRID1, false, 1); }
static int device2on() { return switchDevice(PIN_RELAIS_GRID1, true, 2); }
static int device2off() { return switchDevice(PIN_RELAIS_GRID1, false, 2); }
}; // namespace DeviceGrid1

namespace DeviceGrid2
{
static int on()
{
    digitalWrite(PIN_RELAIS_GRID2, RELAIS_ACTIVE);
    ui.logTrace() << F("switched grid2 = on") << endl;
    return 1;
}
static int off()
{
    digitalWrite(PIN_RELAIS_GRID2, !RELAIS_ACTIVE);
    ui.logTrace() << F("switched grid2 = off") << endl;
    return 1;
}
}; // namespace DeviceGrid2

namespace Devices
{
static void init()
{
    pinMode(PIN_RELAIS_USB1, OUTPUT);
    digitalWrite(PIN_RELAIS_USB1, !RELAIS_ACTIVE);
    pinMode(PIN_RELAIS_USB2, OUTPUT);
    digitalWrite(PIN_RELAIS_USB2, !RELAIS_ACTIVE);
    pinMode(PIN_RELAIS_GRID1, OUTPUT);
    digitalWrite(PIN_RELAIS_GRID1, !RELAIS_ACTIVE);
    pinMode(PIN_RELAIS_GRID2, OUTPUT);
    digitalWrite(PIN_RELAIS_GRID2, !RELAIS_ACTIVE);
    ui.logDebug() << F("devices initialized") << endl;
}
} // namespace Devices
