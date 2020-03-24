#include <Arduino.h>
#include <Streaming.h>
#include <fauxmoESP.h>
#include <ESPAsyncWebServer.h>
#include <OneButton.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <IRremote.h>
#include <rom/rtc.h>

#include "alexaControlSettings.h"
#include "alexaControlWebUI.h"
#include "deviceManager.h"
#include "universalUI.h"

//#define VERBOSE_IRDUMP        // for verbose ir dump output

// global ui instance
UniversalUI ui = UniversalUI("AlexaControl");

DeviceManager deviceManager = DeviceManager();
OneButton button = OneButton(PIN_BUTTON, true, true);
BlinkLed powerOnLed = BlinkLed();

// web server
static char buf[150 * DEVICECOUNT_REAL];
AsyncWebServer server(80);
fauxmoESP fauxmo;
byte webuiRefresh = 0;         // time interval in [seconds] for page refresh
bool webuiVerboseMode = false; // is "verbose"-Checkbox enabled?
bool webuiRefreshEnabled = true;

// NTP time
WiFiUDP ntpUDP;
NTPClient timeClient = NTPClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
String upSince = "";

// ir recv dump
IRrecv irrecv = IRrecv(PIN_IR_RECEIVER);
bool irRecvEnabled = false;
decode_results results;

void adjustWebuiParameters(AsyncWebServerRequest *request)
{
    if (request->hasParam(PARAM_REFRESH))
    {
        const String r = request->getParam(PARAM_REFRESH)->value();
        webuiRefresh = r.toInt();
    }
    webuiVerboseMode = request->hasParam("v");
}
String getRefreshTag(String uri)
{
    if (webuiRefreshEnabled && webuiRefresh > 0)
    {
        sprintf(buf, "<meta http-equiv=\"refresh\" content=\"%d;url=%s?r=%d#refresh\">", webuiRefresh, &uri[0], webuiRefresh);
        return buf;
    }
    else
    {
        return "";
    }
}
String getRefreshLink(String uri)
{
    sprintf(buf, "<a href=\"%s?r=%d\">%s Refresh</a>",
            &uri[0],
            (webuiRefreshEnabled && webuiRefresh > 0) ? 0 : 1,
            (webuiRefreshEnabled && webuiRefresh > 0) ? "Stop" : "Start");
    return buf;
}

String placeholderProcessor(const String &var)
{
    if (var == "VAR")
        return F("Hello world!");
    if (var == "APPNAME")
        return F("AlexaControl");
    if (var == "__TIMESTAMP__")
        return __TIMESTAMP__;
    if (var == "STATUS")
        return ui.getStatusMessage();
    if (var == "STATUSBAR")
    {
        if (strlen(ui.getStatusMessage()) > 0)
        {
            sprintf(buf, "<p style=\"color:blue;background-color:lightgrey;text-align:center;\">Status: %s</p>", ui.getStatusMessage());
            return buf;
        }
        else
        {
            return "";
        }
    }
    if (var == "BUTTONS")
    {
        char *appendBuf = buf;
        for (byte i = 0; i < DEVICECOUNT; ++i)
        {
            if (deviceManager.isRealDevice(i))
            {
                String state = deviceManager.getDeviceState(i);
                String name = deviceManager.getDeviceName(i);
                // action->3, i->3, state->8
                // we choose this improvable variant, assuming: only ascii characters to print
                appendBuf +=
                    sprintf(appendBuf, "<button onclick=\"location.href='/device/%s?d=%d';\" %s class=\"button %s\">%.35s</button><br/>",
                            (deviceManager.isDeviceOn(i) ? "off" : "on"),
                            (i + 1),
                            deviceManager.isPendingActivity(i) ? "disabled=\"disabled\"" : "",
                            &state[0], &name[0]);
            }
        }
        return buf;
    }
    if (var == "BUTTON_MAIN")
    {
        sprintf(buf, "<button onclick=\"location.href='';'\" %s class=\"button %s\">Fernseher</button><br/>",
                deviceManager.isPendingActivity(deviceManager.DEVICEID_MAIN) ? "disabled=\"disabled\"" : "",
                &deviceManager.getDeviceState(deviceManager.DEVICEID_MAIN)[0]);
        return buf;
    }
    if (var == "SETCHANNEL_LINKS")
    {
        char *appendBuf = buf;
        for (byte i = 0; i < DEVICECOUNT; ++i)
        {
            if (deviceManager.isRealDevice(i))
            {
                String name = deviceManager.getDeviceName(i);
                appendBuf += sprintf(appendBuf, "<a href=\"/device/set?d=%d\">%.35s</a> &nbsp; ", (i + 1), &name[0]);
            }
        }
        return buf;
    }
    if (var == "RESET_REASON")
    {
        switch (rtc_get_reset_reason(0))
        {
        case 1:
            return "POWERON_RESET"; /**<1,  Vbat power on reset*/
        case 3:
            return "SW_RESET"; /**<3,  Software reset digital core*/
        case 4:
            return "OWDT_RESET"; /**<4,  Legacy watch dog reset digital core*/
        case 5:
            return "DEEPSLEEP_RESET"; /**<5,  Deep Sleep reset digital core*/
        case 6:
            return "SDIO_RESET"; /**<6,  Reset by SLC module, reset digital core*/
        case 7:
            return "TG0WDT_SYS_RESET"; /**<7,  Timer Group0 Watch dog reset digital core*/
        case 8:
            return "TG1WDT_SYS_RESET"; /**<8,  Timer Group1 Watch dog reset digital core*/
        case 9:
            return "RTCWDT_SYS_RESET"; /**<9,  RTC Watch dog Reset digital core*/
        case 10:
            return "INTRUSION_RESET"; /**<10, Instrusion tested to reset CPU*/
        case 11:
            return "TGWDT_CPU_RESET"; /**<11, Time Group reset CPU*/
        case 12:
            return "SW_CPU_RESET"; /**<12, Software reset CPU*/
        case 13:
            return "RTCWDT_CPU_RESET"; /**<13, RTC Watch dog Reset CPU*/
        case 14:
            return "EXT_CPU_RESET"; /**<14, for APP CPU, reseted by PRO CPU*/
        case 15:
            return "RTCWDT_BROWN_OUT_RESET"; /**<15, Reset when the vdd voltage is not stable*/
        case 16:
            return "RTCWDT_RTC_RESET"; /**<16, RTC Watch dog reset digital core and rtc module*/
        default:
            return "NO_MEAN";
        }
    }
    if (var == "SYSTIME")
    {
        if (ui.isNtpTimeValid())
        {
            sprintf(buf, "%lu @ %s", millis(), &timeClient.getFormattedTime()[0]);
            return buf;
        }
        else
        {
            sprintf(buf, "%lu ms", millis());
            return buf;
        }
    }
    if (var == "UPSINCE")
    {
        if (ui.isNtpTimeValid())
        {
            return upSince;
        }
        else
        {
            ui.printTimeInterval(buf, millis());
            return buf;
        }
    }
    if (var == "REFRESHINDEXTAG")
        return getRefreshTag("/index.html");
    if (var == "REFRESHLOGTAG")
        return getRefreshTag("/log.html");
    if (var == "REFRESHINDEXLINK")
        return getRefreshLink("/index.html");
    if (var == "REFRESHLOGLINK")
        return getRefreshLink("/log.html");
    if (var == "LOG0")
    {
        return ui.getHtmlLog(0);
    }
    if (var == "LOG1")
    {
        return ui.getHtmlLog(1);
    }
    if (var == "IRRCV_BUTTON")
    {
        sprintf(buf, "<button onclick=\"location.href='/irrcv?e=%d';\" class=\"button %s\">IR-Dump</button>",
                irRecvEnabled ? 0 : 1,
                irRecvEnabled ? "active" : "shutdown");
        return buf;
    }
    Serial << "DEBUG: variable not found: " << var << endl;
    ui.logError(var);
    return "???";
}

static void setDeviceState(AsyncWebServerRequest *request, const bool newState)
{
    if (request->hasParam(PARAM_DEVICEID))
    {
        const String deviceId = request->getParam(PARAM_DEVICEID)->value();
        ui.logDebug() << "setting device " << deviceId << " state to " << newState << endl;
        deviceManager.setDeviceStatus(deviceId, newState);
        request->redirect("/index.html");
    }
    else
    {
        ui.logError("Aufruf /device/on ohne deviceId");
        request->send(400, "text/html", "Error");
    }
}
static void setChannel(AsyncWebServerRequest *request)
{
    if (request->hasParam("d"))
    {
        const String deviceId = request->getParam("d")->value();
        ui.logDebug() << "setting active channel to " << deviceId << endl;
        deviceManager.setActiveChannel(deviceId);
        request->redirect("/index.html");
    }
    else
    {
        ui.logError("Aufruf /device/set ohne deviceId");
        request->send(400, "text/html", "Error");
    }
}
static void selectChannel(AsyncWebServerRequest *request, const int direction)
{
    ui.logDebug() << "shifting channel by " << direction << endl;
    deviceManager.selectChannel(direction);
    request->redirect("/index.html");
}

static void setIrReceive(AsyncWebServerRequest *request)
{
    if (request->hasParam("e"))
    {
        const String enabled = request->getParam("e")->value();
        ui.logDebug() << "setting irrcv mode to " << enabled << endl;
        irRecvEnabled = (enabled == "1");
        if (irRecvEnabled)
        {
            irrecv.enableIRIn();
            request->redirect("/log.html");
        }
        else
        {
            request->redirect("/irRcvOff.html");
            // reset to disable IR receive (timers)
            ESP.restart();
        }
    }
    else
    {
        ui.logError("Aufruf /irrcv ohne enabled");
        request->send(400, "text/html", "Error");
    }
}

void serverSetup()
{
    // webUI control page
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        adjustWebuiParameters(request);
        request->send_P(200, "text/html", index_html, placeholderProcessor);
    });
    server.on("/log.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        adjustWebuiParameters(request);
        request->send_P(200, "text/html", log_html, placeholderProcessor);
    });
    server.on("/irRcvOff.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", irRcvOff_html, placeholderProcessor);
    });

    server.on("/device/on", HTTP_GET, [](AsyncWebServerRequest *request) {
        setDeviceState(request, true);
    });
    server.on("/device/off", HTTP_GET, [](AsyncWebServerRequest *request) {
        setDeviceState(request, false);
    });
    server.on("/channel/up", HTTP_GET, [](AsyncWebServerRequest *request) {
        selectChannel(request, -1);
    });
    server.on("/channel/down", HTTP_GET, [](AsyncWebServerRequest *request) {
        selectChannel(request, +1);
    });
    server.on("/device/set", HTTP_GET, [](AsyncWebServerRequest *request) {
        setChannel(request);
    });
    server.on("/irrcv", HTTP_GET, [](AsyncWebServerRequest *request) {
        setIrReceive(request);
    });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon_ico_gzip, favicon_ico_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/index.html");
    });

    // These two callbacks are required for gen1 and gen3 compatibility
    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data)))
            return;
        // Handle any other body request here...
    });
    server.onNotFound([](AsyncWebServerRequest *request) {
        String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
        if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), body))
            return;
        // Handle not found request here...
    });

    // Start the server
    server.begin();
}

void printIrResults(decode_results *results)
{
#ifndef VERBOSE_IRDUMP
    if (results->decode_type == UNKNOWN)
        return;
#endif
    Print &p = ui.logInfo();
    if (results->decode_type == UNKNOWN)
    {
        p << "Unknown encoding: ";
    }
    else if (results->decode_type == NEC)
    {
        p << "Decoded NEC: ";
    }
    else if (results->decode_type == SONY)
    {
        p << "Decoded SONY: ";
    }
    else if (results->decode_type == RC5)
    {
        p << "Decoded RC5: ";
    }
    else if (results->decode_type == RC6)
    {
        p << "Decoded RC6: ";
    }
    else if (results->decode_type == PANASONIC)
    {
        p << "Decoded PANASONIC - Address: ";
        p << _HEX(results->address);
        p << " Value: ";
    }
    else if (results->decode_type == LG)
    {
        p << "Decoded LG: ";
    }
    else if (results->decode_type == JVC)
    {
        p << "Decoded JVC: ";
    }
    else if (results->decode_type == AIWA_RC_T501)
    {
        p << "Decoded AIWA RC T501: ";
    }
    else if (results->decode_type == WHYNTER)
    {
        p << "Decoded Whynter: ";
    }

    p << _HEX(results->value);
    p << " (" << _DEC(results->bits) << " bits)" << endl;
#ifdef VERBOSE_IRDUMP
    p = ui.logInfo();
    const int count = results->rawlen;
    p << "Raw (" << _DEC(count) << "): ";

    for (int i = 1; i < count; ++i)
    {
        if (i & 1)
        {
            p << _DEC(results->rawbuf[i] * USECPERTICK);
        }
        else
        {
            p << '-' << _DEC((unsigned long)results->rawbuf[i] * USECPERTICK);
        }
        p << " ";
    }
    p << endl;
#endif
}

void buttonClick()
{
    // if main device is on, select next channel, otherwise switch on with last active device
    if (deviceManager.isDeviceOn(deviceManager.DEVICEID_MAIN))
    {
        ui.logInfo() << "manual channel switch with button, active was " << _lastActiveChannel << endl;
        deviceManager.selectChannel(+1);
    }
    else if (deviceManager.isPendingActivity(deviceManager.DEVICEID_MAIN))
    {
        ui.logWarn() << "ignored button press while main device pending activity" << endl;
    }
    else
    {
        ui.logInfo() << "manually switched ON main device using channel " << _lastActiveChannel << endl;
        deviceManager.setDeviceStatus(_lastActiveChannel, true);
    }
}

void printWakeupReason()
{
    switch (esp_sleep_get_wakeup_cause())
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        ui.logInfo() << "Wakeup caused by external signal using RTC_IO" << endl;
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        ui.logInfo() << "Wakeup caused by external signal using RTC_CNTL" << endl;
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        ui.logInfo() << "Wakeup caused by timer" << endl;
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        ui.logInfo() << "Wakeup caused by touchpad" << endl;
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        ui.logInfo() << "Wakeup caused by ULP program" << endl;
        break;
    default:
        ui.logInfo() << "Wakeup was not caused by deep sleep: " << esp_sleep_get_wakeup_cause() << endl;
        break;
    }
}

unsigned long lastSleepCheckMs = 0;
void checkForSleep()
{
    const unsigned long now = millis();
    if (deviceManager.isAnyActivity() || !ui.isNtpTimeValid())
    {
        lastSleepCheckMs = now;
        webuiRefreshEnabled = true;
        return;
    }

    if (now - lastSleepCheckMs > DEEPSLEEP_INACTIVITY) // 1min passed, go to sleep
    {
        // check for allowed time frame
        const int nowHour = timeClient.getHours();
        if (DEEPSLEEP_DISALLOW_HOURS)
        {
            lastSleepCheckMs = now;
            return;
        }

        // preparing deep sleep
        if (webuiRefreshEnabled)
        {
            ui.logInfo() << "going to sleep" << endl;
            ui.statusActive("going to sleep");
            ui.setBlink(50, 250);
            webuiRefreshEnabled = false;
        }
        if (now - lastSleepCheckMs > DEEPSLEEP_WAITPERIOD)
        {
            esp_sleep_enable_timer_wakeup(DEEPSLEEP_MAXTIME); // argument is microseconds
            esp_sleep_enable_ext0_wakeup(DEEPSLEEP_BUTTON, LOW);
            esp_deep_sleep_start();
        }
    }
}

void setup()
{
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    powerOnLed.init(PIN_LED_POWER);
    powerOnLed.setBlink(-1, 0);
    powerOnLed.update();
    Devices::init();
    ui.setNtpClient(&timeClient);
    ui.init(PIN_LED_STATUS);
    printWakeupReason();
    serverSetup();
    fauxmo.createServer(false); // not needed, this is the default value
    fauxmo.setPort(80);         // This is required for gen3 devices
    deviceManager.addDevices(fauxmo);
    deviceManager.init();
    fauxmo.enable(true);

    fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value) {
        // Callback when a command from Alexa is received.
        sprintf(buf, "alexa command: #%d to %s", device_id, state ? "ON" : "OFF");
        ui.logInfo(buf);
        deviceManager.setDeviceStatus(device_id, state);
    });

    button.attachClick(buttonClick);
    powerOnLed.setBlink(50, 2950);

    if (ui.isNtpTimeValid())
    {
        upSince = timeClient.getFormattedTime();
        ui.logInfo() << "set upSince to " << upSince << endl;
    }
}

void loop()
{
    if (ui.handle())
    {
        fauxmo.handle();
        deviceManager.handle();
        button.tick();
        powerOnLed.update();

        if (irRecvEnabled && irrecv.decode(&results))
        {
            printIrResults(&results);
            irrecv.decode(&results);
            irrecv.resume(); // Receive the next value
        }

        checkForSleep();

        // output free heap every 5 seconds, as a cheap way to detect memory leaks
        if ((millis() % 5000) == 0)
        {
            Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
            delay(1);
        }
    }
}
