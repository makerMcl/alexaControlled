#include <Arduino.h>
#include "mmControlSettings.h"
#include "universalUI.h"
#include "deviceCommandsMain.h"
#include "deviceCommandsDevices.h"
#include "commandQueue.h"
#include "device.h"

#define DEVICECOUNT 8
#define DEVICECOUNT_REAL 5
#define DEFAULT_DEVICE 1 // that is "KabelTV"
#define VOLUME_MAXVALUE 45
#define VOLUME_MAXVALUE_STR "45"
#define VOLUME_MAXKNOWNVALUE 25
#define VOLUME_MAXKNOWNVALUE_STR "25"

static RTC_DATA_ATTR byte _lastActiveChannel = DEFAULT_DEVICE; // save this in RTC memory, to live trough deep sleep!
static RTC_DATA_ATTR int _knownVolume = -1;                    // saved in RTC memory

/*
 * State managing machine for mmControl project.
 * 
 * It knows the devices internally, external interface is for activation control and state.
 */
class DeviceManager
{
private:
    /** 0-based number of input channel of TV */
    Device* _devices[DEVICECOUNT] = {
        new Device("DVD", DeviceGrid2::on, DeviceGrid2::off),                       // id: 0, grid2
        new Device("Switch", DeviceGrid1::device1on, DeviceGrid1::device1off),      // id: 3, grid1, source=hdmi1
        new Device("KabelTV", nullptr, nullptr),                                    // id: 1
        new Device("Netflix", DeviceUsb1::on, DeviceUsb1::off),                     // id: 2, usb1, source=hdmi3
        new Device("Playstation", DeviceGrid1::device2on, DeviceGrid1::device2off), // id: 4, grid1, source=hdmi4
        new Device("TV", 2),
        new Device("Fernseher", 2),
        new Device("Retropi", 1)};
    class MainDevice : public Device{
        protected :
            virtual void commandOn(CommandQueue * queue){
                queue->addCommand(DeviceTvMedion::onOff);
    queue->addCommand(DeviceSoundbarPhilips::onOff);
} virtual void commandOff(CommandQueue *queue)
{
    commandOn(queue);
}

public:
MainDevice() : Device("main", nullptr, nullptr, 20000, 20000) {}
}
*_main = new MainDevice();

CommandQueue *_pendingCommands = new CommandQueue();
byte _activeChannel = _lastActiveChannel;
byte _targetChannel = DEFAULT_DEVICE;
byte _targetVolume = -1;
bool isValidDeviceId(const int id);

public:
static const byte DEVICEID_MAIN = 255;

DeviceManager() {}
void setDeviceStatus(byte deviceId, bool state);
void setDeviceStatus(String deviceNumber, bool state);
void setActiveChannel(String deviceNumber);
byte getActiveChannelIndex()
{
    return _activeChannel;
}
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
byte getVolume();
void setKnownVolume(const byte volumeIsAbsolute);
void setVolume(const byte targetVolumePercent);
void setVolumeAbsolute(const int targetVolume);

#ifndef DISABLE_FAUXMO
void addDevices(fauxmoESP *fauxmo)
{
    for (int i = 0; i < DEVICECOUNT; ++i)
    {
        fauxmo->addDevice(_devices[i]->getName());
        ui.logDebug() << "added device id=" << i << " '" << _devices[i]->getName() << "'" << endl;
    }
}
#endif
}
;
