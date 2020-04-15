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
    return !_devices[deviceId]->isAlias();
}

String DeviceManager::getDeviceState(const byte deviceId)
{
    return deviceId == DEVICEID_MAIN ? _main->getStateName() : _devices[deviceId]->getStateName();
}
bool DeviceManager::isDeviceOn(byte deviceId)
{
    return deviceId == DEVICEID_MAIN ? _main->isOn() : _devices[deviceId]->isOn();
}
bool DeviceManager::isPendingActivity(const byte deviceId)
{
    return deviceId == DEVICEID_MAIN ? _main->isPendingActivity() : _devices[deviceId]->isPendingActivity();
}
bool DeviceManager::isAnyActivity()
{
    for (int i = 0; i < DEVICECOUNT; ++i)
    {
        if (!_devices[i]->isOff())
        {
            return true;
        }
    }
    return !_main->isOff() && !_pendingCommands->isEmpty();
}

String DeviceManager::getDeviceName(const byte deviceId)
{
    return _devices[deviceId]->getName();
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

void DeviceManager::setActiveChannel(String deviceNumber)
{
    const int n = deviceNumber.toInt();
    if (0 == strcmp_P(deviceNumber.c_str(), PSTR("off")))
    {
        _main->forceState(false);
    }
    else if (0 == strcmp_P(deviceNumber.c_str(), PSTR("on")))
    {
        _main->forceState(true);
        if (isRealDevice(_activeChannel) && _devices[_activeChannel]->isOff())
        {
            // TODO if sending toggling onOff commands to device, its state has to be defined some way
            _devices[n - 1]->setStateOn(_pendingCommands);
        }
    }
    else if (isValidDeviceId(n - 1) && isRealDevice(n - 1))
    {
        _activeChannel = n - 1;
        _targetChannel = n - 1;
        _lastActiveChannel = n - 1;
        if (_devices[n - 1]->isOff())
        {
            _devices[n - 1]->forceState(true);
            _main->forceState(true);
        }
    }
    else
    {
        ui.logError() << F("invalid value for setActiveChannel: ") << deviceNumber << endl;
    }
}

void DeviceManager::setDeviceStatus(const String deviceNumber, const bool stateIsOn)
{
    const int n = deviceNumber.toInt();
    if (isValidDeviceId(n - 1))
    {
        setDeviceStatus(n - 1, stateIsOn);
    }
}

/**
 * @param deviceNumber must be >=0
 */
void DeviceManager::setDeviceStatus(const byte deviceId, const bool targetStateIsOn)
{
    if (isValidDeviceId(deviceId))
    {
        if (_devices[deviceId]->isAlias())
        {
            setDeviceStatus(_devices[deviceId]->getAliasedDevice(), targetStateIsOn);
            ui.logDebug() << F("setDeviceStatus() for aliased device ") << deviceId << F(" -> ") << _devices[deviceId]->getAliasedDevice() << endl;
        }
        else
        {
            _devices[deviceId]->setState(targetStateIsOn, _pendingCommands);
            if (targetStateIsOn)
            {
                _devices[_activeChannel]->setStateOff(_pendingCommands); // switch previous active device off
                _targetChannel = deviceId;
            }
            ui.logDebug() << F("setDeviceStatus(): deviceId=") << deviceId << F("set to ") << targetStateIsOn
                          << F(", reached state=") << _devices[deviceId]->getStateName()
                          << F(", targetChannel=") << _targetChannel
                          << endl;
        }
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
    _pendingCommands->handle();
    _main->handle(_pendingCommands);
    // check for any device to be switched on
    bool anythingOn = false;
    for (int i = 0; i < DEVICECOUNT; ++i)
    {
        _devices[i]->handle(_pendingCommands);
        anythingOn |= _devices[i]->isActive();
    }

    // on/off-handling of main devices (include sound bar!)
    if (anythingOn && !_main->isActive())
    {
        _main->setState(true, _pendingCommands);
    }
    if (_main->isOn())
    {
        if (!anythingOn)
        {
            _main->setState(false, _pendingCommands);
        }
        else if (_targetChannel != _activeChannel)
        {
            // switch channel of main device if ON and not at target channel
            selectChannel(_targetChannel - _activeChannel);
        }

        // adapt volume if necessary
        if (_knownVolume >= 0 && _knownVolume != _targetVolume && _knownVolume <= VOLUME_MAXVALUE)
        {
            ui.startActivity();
            int volumeDelta = _targetVolume - _knownVolume;
            ui.logDebug() << F("generating IR sequence for volume change by ") << volumeDelta << endl;
            if (volumeDelta > 0)
            {
                for (int i = 0; i < volumeDelta; ++i)
                    _pendingCommands->addCommand(DeviceSoundbarPhilips::volumeUp);
            }
            else if (volumeDelta < 0)
            {
                for (int i = 0; volumeDelta < i; --i)
                    _pendingCommands->addCommand(DeviceSoundbarPhilips::volumeDown);
            }
            _knownVolume = _targetVolume;
            ui.finishActivity();
        }
    }
}

void DeviceManager::selectChannel(int deltaChannelNumber)
{
    ui.startActivity();
    // generate IR sequence for given delta
    ui.logDebug() << F("generating IR sequence for channel-shift by ") << deltaChannelNumber << endl;
    _pendingCommands->addCommand(DeviceTvMedion::av);
    if (deltaChannelNumber > 0)
    {
        for (int i = 0; i < deltaChannelNumber; ++i)
            _pendingCommands->addCommand(DeviceTvMedion::down);
    }
    else if (deltaChannelNumber < 0)
    {
        for (int i = 0; deltaChannelNumber < i; --i)
            _pendingCommands->addCommand(DeviceTvMedion::up);
    }
    _pendingCommands->addCommand(DeviceTvMedion::ok);
    // update _lastActiveChannel after IR command has been issued
    const int reachedChannel = _activeChannel + deltaChannelNumber;
    if (reachedChannel < 0)
    {
        _activeChannel = DEVICECOUNT_REAL + reachedChannel;
    }
    else if (reachedChannel >= DEVICECOUNT_REAL)
    {
        _activeChannel = reachedChannel - DEVICECOUNT_REAL;
    }
    else
    {
        _activeChannel = reachedChannel;
    }
    _targetChannel = _activeChannel;
    _lastActiveChannel = _activeChannel;
    ui.finishActivity();
}

byte DeviceManager::getVolume()
{
    return _targetVolume;
}

void DeviceManager::setKnownVolume(const byte volumeIs)
{
    if (volumeIs <= VOLUME_MAXKNOWNVALUE)
    {
        ui.logDebug() << F("knownValue set to ") << volumeIs << endl;
        _knownVolume = volumeIs;
        _targetVolume = volumeIs;
        ui.clearUiError();
    }
    else
    {
        ui.logError() << F("knownValue out of range: ") << volumeIs << endl;
        ui.reportUiError("Wert für Istwert darf maximal " VOLUME_MAXKNOWNVALUE_STR " [%] sein, den angezeigten Wert als Prozentzahl setzen", 5);
    }
}
void DeviceManager::setVolume(const byte targetVolumePercent)
{
    // absolute scale of soundbar is 0..VOLUME_MAXVALUE
    setVolumeAbsolute(targetVolumePercent * VOLUME_MAXVALUE / 100);
}
void DeviceManager::setVolumeAbsolute(const int targetVolume)
{
    if (_knownVolume < 0)
    {
        ui.reportUiError("no known volume, please set it via webUI or with 'alexa, ist-Stand auf <Wert in Soundbar-Anzeige>'", 5);
    }
    if (targetVolume < 0)
    {
        ui.reportUiError("Already muted", 5);
    }
    else if (targetVolume > VOLUME_MAXVALUE)
    {
        ui.reportUiError("Maximalwert erreicht", 5);
    }
    else
    {
        _targetVolume = targetVolume;
        ui.clearUiError();
    }
}
