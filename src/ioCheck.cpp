#include <Arduino.h>
#include <Streaming.h>
#include <OneButton.h>
#include "deviceCommandsMain.h"
#include "deviceCommandsDevices.h"
#include "universalUI.h"
#include "mmControlWebUI.h"

#define PIN_LED 13
UniversalUI ui = UniversalUI("mmControl IO check");
OneButton button = OneButton(PIN_BUTTON, true, true);

void onClick()
{
    digitalWrite(PIN_LED_STATUS, HIGH);
    ui.statusActive("sending IR command");
    delay(DeviceTvMedion::av());
    ui.statusLedOff();
    ui.logDebug("sent av");
    delay(DeviceTvMedion::down());
    ui.logDebug("sent down");
    ui.statusLedOn();
    DeviceTvMedion::ok();
    ui.logDebug("sent ok");
    ui.statusOk();
    digitalWrite(PIN_LED_STATUS, LOW);
}

void setup()
{
    ui.init(2);
    Devices::init();
    button.attachClick(onClick);
    pinMode(PIN_LED_STATUS, OUTPUT);
    ui.logDebug() << "Echt tolles"
                  << " debugging" << endl;
}

void loop()
{
    ui.handle();
    button.tick();
}
