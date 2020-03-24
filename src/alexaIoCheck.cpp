#include <Arduino.h>
#include <Streaming.h>
#include <OneButton.h>
#include "deviceCommandsMain.h"
#include "deviceCommandsDevices.h"
#include "universalUI.h"

#define RELAIS_PORT 26
#define PIN_BUTTON 4
#define PIN_LED 13
UniversalUI ui = UniversalUI("Alexa IO check");
OneButton button = OneButton(PIN_BUTTON, 1, true);

void onClick()
{
    digitalWrite(PIN_LED, HIGH);
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
    digitalWrite(PIN_LED, LOW);
}

void setup()
{
    ui.init(2);
    Devices::init();
    button.attachClick(onClick);
    pinMode(PIN_LED, OUTPUT);
    ui.logDebug() << "Echt tolles"
                  << " debugging" << endl;
}

void loop()
{
    ui.handle();
    button.tick();
}
