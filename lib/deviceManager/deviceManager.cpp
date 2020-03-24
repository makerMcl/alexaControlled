#include <Arduino.h>
#ifndef DISABLE_FAUXMO
#include <fauxmoESP.h>
#endif
#include <Streaming.h>
#include "universalUIglobal.h"
#include "deviceManager.h"
#include "deviceCommandsMain.h"

void DeviceManager::init()
{
    Devices::init();
}

bool DeviceManager::isRealDevice(byte deviceId)
{
    return !_devices[deviceId].isAlias();
}

String DeviceManager::getDeviceState(const byte deviceId)
{
    return deviceId == DEVICEID_MAIN ? _main.getStateName() : _devices[deviceId].getStateName();
}
bool DeviceManager::isDeviceOn(byte deviceId)
{
    return deviceId == DEVICEID_MAIN ? _main.isOn() : _devices[deviceId].isOn();
}
bool DeviceManager::isPendingActivity(const byte deviceId)
{
    return deviceId == DEVICEID_MAIN ? _main.isPendingActivity() : _devices[deviceId].isPendingActivity();
}
bool DeviceManager::isAnyActivity()
{
    for (int i = 0; i < DEVICECOUNT; ++i)
    {
        if (!_devices[i].isOff())
        {
            return true;
        }
    }
    return !_main.isOff() && !_pendingCommands.isEmpty();
}

String DeviceManager::getDeviceName(const byte deviceId)
{
    return _devices[deviceId].getName();
}

bool DeviceManager::isValidDeviceId(const int id)
{
    if (id < 0)
    {
        ui.logError("no<0");
        return false;
    }
    else if (id >= DEVICECOUNT)
    {
        ui.logError("no>=DEVICECOUNT");
        return false;
    }
    return true;
}

void DeviceManager::setDeviceStatus(const String deviceNumber, const bool stateIsOn)
{
    const int n = deviceNumber.toInt();
    if (isValidDeviceId(n - 1))
    {
        setDeviceStatus(n - 1, stateIsOn);
    }
}

void DeviceManager::setActiveChannel(String deviceNumber)
{
    const int n = deviceNumber.toInt();
    if (isValidDeviceId(n - 1) && isRealDevice(n - 1))
    {
        _activeChannel = n - 1;
        _lastActiveChannel = n - 1;
    }
}

/**
 * @param deviceNumber must be >=0
 */
void DeviceManager::setDeviceStatus(const byte deviceId, const bool targetStateIsOn)
{
    if (isValidDeviceId(deviceId))
    {
        if (_devices[deviceId].isAlias())
        {
            setDeviceStatus(_devices[deviceId].getAliasedDevice(), targetStateIsOn);
            ui.logDebug() << "setDeviceStatus() for aliased device " << deviceId << " -> " << _devices[deviceId].getAliasedDevice() << endl;
        }
        else
        {
            _devices[deviceId].setState(targetStateIsOn, _pendingCommands);
            if (targetStateIsOn)
            {
                _targetChannel = deviceId;
            }
        }
        ui.logDebug() << "setDeviceStatus(): deviceId=" << deviceId << "set to " << targetStateIsOn
                      << ", reached state=" << _devices[deviceId].getStateName()
                      << ", targetChannel=" << _targetChannel
                      << endl;
    }
}

/*
 * Implements state machine to switch on/off and switch channel to a specific device.
 * 
 * 1. switch on TV if it is off
 * 2. switch on device on if it is off
 * 3. switch off active device (with delay for shutdown)
 * 5. wait for tv, then determine channel delta and call selectChannel()
 * 
 * Device-specific actions:
 * SWITCH_TO_ON: send ir code for ON && state=WAIT_FOR_ON
 * WAIT_FOR_ON: after time elapsed: state=ON
 * WAIT_TO_OFF: after time elapsed: send IR code for OFF
 */
void DeviceManager::handle()
{
    _pendingCommands.handle();
    _main.handle(_pendingCommands);
    // check for any device to be switched on
    bool anythingOn = false;
    for (int i = 0; i < DEVICECOUNT; ++i)
    {
        _devices[i].handle(_pendingCommands);
        anythingOn |= _devices[i].isActive();
    }

    // on/off-handling of main devices (include sound bar!)
    if (anythingOn && !_main.isActive())
    {
        _main.setState(true, _pendingCommands);
        ui.logDebug() << "set main device to ON" << endl;
    }
    if (_main.isOn())
    {
        if (!anythingOn)
        {
            _main.setState(false, _pendingCommands);
            ui.logDebug() << "set main device to OFF" << endl;
        }
        else if (_targetChannel != _activeChannel)
        {
            // switch channel of main device if ON and not at target channel
            selectChannel(_targetChannel - _activeChannel);
        }
    }
}

void DeviceManager::selectChannel(int deltaChannelNumber)
{
    ui.startActivity();
    // generate IR sequence for given delta
    ui.logDebug() << "generating IR sequence for channel-shift by " << deltaChannelNumber << endl;
    _pendingCommands.addCommand(DeviceTvMedion::av);
    if (deltaChannelNumber > 0)
    {
        for (int i = 0; i < deltaChannelNumber; ++i)
            _pendingCommands.addCommand(DeviceTvMedion::down);
    }
    else if (deltaChannelNumber < 0)
    {
        for (int i = 0; deltaChannelNumber < i; --i)
            _pendingCommands.addCommand(DeviceTvMedion::up);
    }
    _pendingCommands.addCommand(DeviceTvMedion::ok);
    // update _lastActiveChannel after IR command has been issued
    _activeChannel += deltaChannelNumber;
    _targetChannel = _activeChannel;
    _lastActiveChannel = _activeChannel;
    ui.finishActivity();
}
