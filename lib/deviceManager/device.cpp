#include <Arduino.h>
#include "universalUIglobal.h"
#include "device.h"
#include "commandQueue.h"

void Device::setState(const bool targetStateIsOn, const CommandQueue queue)
{
    if (targetStateIsOn) // switch to ON
    {
        switch (_state)
        {
        case OFF:
            _state = SWITCH_TO_ON;
            ui.logDebug() << _name << ": OFF -> SWITCH_TO_ON" << endl;
            break;
        case SWITCH_TO_OFF:
            // abort switching off
            _state = ON;
            ui.logDebug() << _name << ": aborted switching off" << endl;
            break;
        case ON:
            ui.logWarn("device already active");
            break;
        case WAIT_FOR_OFF:
            _state = SWITCH_TO_ON;
            // _nextActionAtMs is already set
            ui.logDebug() << _name << ": WAIT_FOR_OFF -> SWITCH_TO_ON with delay=" << _waitInterval << endl;
            break;
        case ALIAS:
        case WAIT_FOR_ON:
        case SWITCH_TO_ON:
            // do nothing
            ui.logDebug() << _name << ": doing nothing for state=" << _state << endl;
            break;
        }
    }
    else // switch to OFF
    {
        switch (_state)
        {
        case WAIT_FOR_ON:
            // we assume that _waitForOffMs>_waitForOnMs, so fall trough
        case ON:
            _state = SWITCH_TO_OFF;
            _lastActionAtMs = millis();
            _waitInterval = _waitBeforeOffMs;
            ui.logDebug() << _name << ": ON -> WAIT_TO_OFF" << endl;
            break;
        case SWITCH_TO_ON: // abort switch on
            _state = OFF;
            ui.logDebug() << _name << ": aborting switching on" << endl;
            break;
        case OFF:
        case ALIAS:
        case SWITCH_TO_OFF:
        case WAIT_FOR_OFF:
            // do nothing
            ui.logDebug() << _name << ": doing nothing for state=" << _state << endl;
        }
    }
}

bool Device::handle(const CommandQueue queue)
{
    switch (_state)
    {
        // most possible cases first
    case ON:
    case OFF:
        // do nothing
        return false;
    case SWITCH_TO_ON:
        ui.startActivity();
        ui.logDebug() << _name << ": sending ON-command to device" << endl;
        commandOn(queue);
        _state = WAIT_FOR_ON;
        _lastActionAtMs = millis();
        _waitInterval = _waitForOnMs;
        return true;
    case WAIT_FOR_ON:
        if (isWaitTimeReached())
        {
            _state = ON;
            _waitInterval = 0;
            ui.finishActivity();
            return true;
        }
        break;
    case SWITCH_TO_OFF:
        if (isWaitTimeReached())
        {
            ui.startActivity();
            ui.logDebug() << _name << ": sending OFF-command to device" << endl;
            commandOff(queue);
            _state = WAIT_FOR_OFF;
            _lastActionAtMs = millis();
            _waitInterval = _waitForOffMs;
            return true;
        }
        break;
    case WAIT_FOR_OFF:
        if (isWaitTimeReached())
        {
            ui.logDebug() << _name << ": reaching off-state" << endl;
            _state = OFF;
            _lastActionAtMs = 0;
            _waitInterval = 0;
            ui.finishActivity();
            return true;
        }
    case ALIAS:; // unexpected, fall trough
    }
    return false;
}
