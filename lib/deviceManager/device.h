#include <Arduino.h>
#include "commandQueue.h"
#include "universalUIglobal.h"
#pragma once

/**
 * Manages the state of a single device.
 * It does not know about physical actions, only implements the state machine and issues commands.
 */
class Device
{
private:
    const char *_name;
    const CommandFunction _cmdOn = nullptr;
    const CommandFunction _cmdOff = nullptr;
    word _waitForOnMs = 10000;     // time to wait till device can receive next command after got ON-command
    word _waitForOffMs = 100000;   // time to wait till device can receive next command after got OFF-command
    word _waitBeforeOffMs = 30000; // time to wait before switching device off - 30 sek
    /** OFF  [user]>  SWITCH_TO_ON  [ir-cmd]>  WAIT_FOR_ON  [time]>  ON
     *   [user]>  SWITCH_TO_OFF  [time]>  WAIT_TO_OFF  [ir-cmd]>  OFF
     */
    enum DeviceState
    {
        SWITCH_TO_ON = 2, // handle() should switch device on
        WAIT_FOR_ON = 3,  // waiting for device powering on
        ON = 1,
        SWITCH_TO_OFF = 4, // handle() should switch device off
        WAIT_FOR_OFF = 5,  // waiting for device powering off
        OFF = 6,
        ALIAS = 99 // nextActionMs is index of another device
    } _state = OFF;
    word _lastActionAtMs = 0;
    word _waitInterval = 0;

    static bool isActiveState(DeviceState state)
    {
        return state < WAIT_FOR_OFF;
    }
    static bool isPendingState(const DeviceState state)
    {
        return state == WAIT_FOR_ON || state == WAIT_FOR_OFF;
    }
    /* implement handling of millis()-rollover as described in https://playground.arduino.cc/Code/TimingRollover/ */
    bool isWaitTimeReached()
    {
        return ((millis() - _lastActionAtMs) >= _waitInterval);
    }

protected:
    void commandOn(CommandQueue queue)
    {
        if (nullptr != _cmdOn)
        {
            if (!queue.addCommand(_cmdOn))
            {
                ui.logError() << "dropped cmd: " << _name << " ON";
            }
        }
    }
    void commandOff(CommandQueue queue)
    {
        if (nullptr != _cmdOff)
        {
            if (!queue.addCommand(_cmdOff))
            {
                ui.logError() << "dropped cmd: " << _name << " OFF";
            }
        }
    }

public:
    /** alias device */
    Device(const char *name, const byte aliasDeviceId) : _name(name), _state(ALIAS), _waitInterval(aliasDeviceId) {}
    /** device with default timings */
    Device(const char *name, const CommandFunction cmdOn, const CommandFunction cmdOff) : _name(name), _cmdOn(cmdOn), _cmdOff(cmdOff) {}
    /** device with specific timings */
    Device(const char *name, const CommandFunction cmdOn, const CommandFunction cmdOff, const word delayOn, const word delayOff) : Device(name, cmdOn, cmdOff)
    {
        _waitForOnMs = delayOn;
        _waitForOffMs = delayOff;
    }

    const char *getName()
    {
        return _name;
    }

    bool isAlias()
    {
        return _state == ALIAS;
    }
    byte getAliasedDevice()
    {
        return isAlias() ? _waitInterval : -1;
    }

    bool isOff()
    {
        return _state == OFF || _state == ALIAS;
    }
    bool isOn()
    {
        return _state == ON;
    }
    bool isActive()
    {
        return isActiveState(_state);
    }
    bool isPendingActivity()
    {
        return isPendingState(_state);
    }

    String getStateName()
    {
        switch (_state)
        {
        case OFF:
            return "off";
            break;
        case ON:
            return "active";
            break;
        case SWITCH_TO_OFF:
        case WAIT_FOR_OFF:
            return "shutdown";
            break;
        case SWITCH_TO_ON:
        case WAIT_FOR_ON:
            return "startup";
            break;
        default:
            return "unknown";
        }
    }

    /**
     * Define the new target state the device should switch to.
     * Immediatly accepts the target state, but switching to it is delayed.
     */
    void setState(const bool targetStateIsOn, const CommandQueue queue);

    /**
     * Performs device activation, blocking only for the duration of successive IR commands.
     * @return true if an activity was initiated
     */
    bool handle(const CommandQueue queue);
};
