#include <Arduino.h>
#include "alexaControlSettings.h"
#include "universalUI.h"
#include "deviceCommandsMain.h"
#include "deviceCommandsDevices.h"
#include "commandQueue.h"
#include "device.h"

#define DEVICECOUNT 6
#define DEVICECOUNT_REAL 5
#define DEFAULT_DEVICE 1 // that is "KabelTV"

static RTC_DATA_ATTR byte _lastActiveChannel = DEFAULT_DEVICE; // save this in RTC memory, to live trough deep sleep!

/*
 * State managing machine for alexaControl project.
 * 
 * It knows the devices internally, external interface is for activation control and state.
 */
class DeviceManager
{
private:
    /** 0-based number of input channel of TV */
    Device _devices[DEVICECOUNT] = {
        Device("DVD", DeviceGrid1::on, DeviceGrid1::off),         // id: 0
        Device("KabelTV", nullptr, nullptr),                      // id: 1
        Device("Netflix", DeviceUsb1::on, DeviceUsb1::off),       // id: 2
        Device("Switch", DeviceGrid1::on, DeviceGrid1::off),      // id: 3
        Device("Playstation", DeviceGrid1::on, DeviceGrid1::off), // id: 4
        Device("TV", 1)};                                         // id: 5
    class MainDevice : public Device
    {
    protected:
        void commandOn(CommandQueue queue)
        {
            queue.addCommand(DeviceTvMedion::onOff);
            queue.addCommand(DeviceSoundbarPhilips::onOff);
        }
        void commandOff(CommandQueue queue)
        {
            commandOn(queue);
        }

    public:
        MainDevice() : Device("main", nullptr, nullptr, 20000, 20000) {}
    } _main = MainDevice();

    CommandQueue _pendingCommands = CommandQueue();
    byte _activeChannel = _lastActiveChannel;
    byte _targetChannel = DEFAULT_DEVICE;
    bool isValidDeviceId(const int id);

public:
    static const byte DEVICEID_MAIN = 255;

    DeviceManager() {}
    void setDeviceStatus(byte deviceId, bool state);
    void setDeviceStatus(String deviceNumber, bool state);
    void setActiveChannel(String deviceNumber);
    /** send IR sequence for changing given number of channels */
    void selectChannel(int deltaChannelNumber);

    /** performs initialization. to be called in setup(). */
    void init();
    /**
     * Performs device activation, blocking only for the duration of successive IR commands.
     */
    void handle();

    bool isDeviceOn(const byte deviceId);
    bool isPendingActivity(const byte deviceId);
    bool isRealDevice(const byte deviceId);
    bool isAnyActivity();
    String getDeviceState(const byte deviceId);
    String getDeviceName(const byte deviceId);

#ifndef DISABLE_FAUXMO
    void addDevices(fauxmoESP fauxmo)
    {
        for (int i = 0; i < DEVICECOUNT; ++i)
        {
            fauxmo.addDevice(_devices[i].getName());
        }
    }
#endif
};
