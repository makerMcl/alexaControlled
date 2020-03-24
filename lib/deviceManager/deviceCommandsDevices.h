#include <Arduino.h>
#include "alexaControlSettings.h"
#include "universalUIglobal.h"

namespace DeviceUsb1
{
static word on()
{
    digitalWrite(PIN_RELAIS_USB1, LOW);
    ui.logTrace() << "switched USB1 = on" << endl;
    return 1;
}
static word off()
{
    digitalWrite(PIN_RELAIS_USB1, HIGH);
    ui.logTrace() << "switched USB1 = off" << endl;
    return 1;
}
}; // namespace DeviceUsb1

namespace DeviceGrid1
{
static word on()
{
    digitalWrite(PIN_RELAIS_GRID1, LOW);
    ui.logTrace() << "switched grid1 = on" << endl;
    return 1;
}
static word off()
{
    digitalWrite(PIN_RELAIS_GRID1, HIGH);
    ui.logTrace() << "switched grid1 = off" << endl;
    return 1;
}
}; // namespace DeviceGrid1

namespace Devices
{
static void init()
{
    pinMode(PIN_RELAIS_USB1, OUTPUT);
    DeviceUsb1::off();
    pinMode(PIN_RELAIS_USB2, OUTPUT);
    //DeviceUsb2::off();
    pinMode(PIN_RELAIS_GRID1, OUTPUT);
    DeviceGrid1::off();
    pinMode(PIN_RELAIS_GRID2, OUTPUT);
    //DeviceGrid2::off();
}
} // namespace Devices
