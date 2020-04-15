#include <Arduino.h>
#include <Streaming.h>
#include <fauxmoESP.h>
#include <ESPAsyncWebServer.h>
#include <OneButton.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <IRremote.h>
#include <rom/rtc.h>

#include "mmControlSettings.h"
#include "mmControlWebUI.h"
#include "deviceManager.h"
#include "universalUI.h"

#define VERBOSE_IRDUMP                 // for verbose ir dump output
#define DEVICENAME_VOLUME_IS "Istwert" // name of device for setting known-volume
#define DEVICENAME_VOLUME "Lautstärke" // name of device for setting target volume
#define USERINPUT_WARNBLINK_DURATION 5

// global ui instance
UniversalUI ui = UniversalUI("AlexaControl");

DeviceManager *deviceManager = new DeviceManager();
OneButton button = OneButton(PIN_BUTTON, true, true);
BlinkLed powerOnLed = BlinkLed();

// web server
static char buf[150 * DEVICECOUNT_REAL];
AsyncWebServer server(80);
fauxmoESP *fauxmo = new fauxmoESP();
byte webuiRefresh = 0;         // time interval in [seconds] for page refresh
bool webuiVerboseMode = false; // is "verbose"-Checkbox enabled?
bool webuiRefreshEnabled = true;

// NTP time
WiFiUDP ntpUDP;
NTPClient *timeClient = new NTPClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
String upSince = "";

// ir recv dump
IRrecv irrecv = IRrecv(PIN_IR_RECEIVER);
bool irRecvEnabled = false;
decode_results results;

void performDeepSleep()
{
    esp_sleep_enable_timer_wakeup(DEEPSLEEP_MAXTIME); // argument is microseconds
    esp_sleep_enable_ext0_wakeup(DEEPSLEEP_BUTTON, LOW);
    esp_deep_sleep_start();
}

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
        sprintf(buf, PSTR("<meta http-equiv=\"refresh\" content=\"%d;url=%s?r=%d#refresh\">"), webuiRefresh, uri.c_str(), webuiRefresh);
        return buf;
    }
    else
    {
        return "";
    }
}

/** @param buf pointer at character array where to append to (aka position of terminating '\0') */
static char *appendstr_P(char *buf, const __FlashStringHelper *pgmstr)
{
    const char *pstr = (char *)pgmstr;
    char b;
    while ('\0' != (b = pgm_read_byte(pstr++)))
        *buf++ = b;
    *buf = '\0';
    return buf;
}
static char *appendstr(char *buf, const char *str)
{
    while ('\0' != *str)
        *buf++ = *str++;
    *buf = '\0';
    return buf;
}

String getRefreshLink(String uri)
{
    char *abuf = buf + sprintf_P(buf, PSTR("<a href=\"%s?r=%d\">"),
                               uri.c_str(),
                               (webuiRefreshEnabled && webuiRefresh > 0) ? 0 : 1);
    if (webuiRefreshEnabled && webuiRefresh > 0)
        abuf = appendstr_P(abuf, F("Stop"));
    else
        abuf = appendstr_P(abuf, F("Start"));
    appendstr_P(abuf, F(" Refresh</a>"));
    return buf;
}

String placeholderProcessor(const String &var)
{
    if (0 == strcmp_P(var.c_str(), PSTR("APPNAME")))
        return F("AlexaControl");
    if (0 == strcmp_P(var.c_str(), PSTR("__TIMESTAMP__")))
        return F(__TIMESTAMP__);
    if (0 == strcmp_P(var.c_str(), PSTR("STATUS")))
        return ui.getStatusMessage();
    if (0 == strcmp_P(var.c_str(), PSTR("STATUSBAR")))
    {
        if (ui.hasStatusMessage())
        {
            sprintf(buf, "<p style=\"color:blue;background-color:lightgrey;text-align:center;\">Status: %s</p>", ui.getStatusMessage());
            return buf;
        }
        else
        {
            return "";
        }
    }
    if (0 == strcmp_P(var.c_str(), PSTR("VOLUME_CURRENT")))
    {
        sprintf(buf, "%d", deviceManager->getVolume());
        return buf;
    }
    if (0 == strcmp_P(var.c_str(), PSTR("BUTTONS")))
    {
        char *appendBuf = buf;
        for (byte i = 0; i < DEVICECOUNT; ++i)
        {
            if (deviceManager->isRealDevice(i))
            {
                String state = deviceManager->getDeviceState(i);
                String name = deviceManager->getDeviceName(i);
                // action->3, i->3, state->8
                // we choose this improvable variant, assuming: only ascii characters to print
                appendBuf = appendstr_P(appendBuf, F("<button onclick=\"location.href='/device/"));
                if (deviceManager->isDeviceOn(i))
                    appendBuf = appendstr_P(appendBuf, F("off"));
                else
                    appendBuf = appendstr_P(appendBuf, F("on"));
                appendBuf += sprintf_P(appendBuf, PSTR("?d=%d';\" "), (i + 1));
                if (deviceManager->isPendingActivity(i))
                    appendBuf = appendstr_P(appendBuf, F("disabled=\"disabled\""));
                appendBuf += sprintf_P(appendBuf, PSTR("class=\"button %s\">%.35s</button><br/>"), &state[0], &name[0]);
            }
        }
        return buf;
    }
    if (0 == strcmp_P(var.c_str(), PSTR("BUTTON_MAIN")))
    {
        char *abuf = appendstr_P(buf, F("<button onclick=\"location.href='';\" "));
        if (deviceManager->isPendingActivity(DeviceManager::DEVICEID_MAIN))
            abuf = appendstr_P(abuf, F("disabled=\"disabled\""));
        abuf = appendstr_P(abuf, F(" class=\"button2 "));
        abuf = appendstr(abuf, deviceManager->getDeviceState(DeviceManager::DEVICEID_MAIN).c_str());
        abuf = appendstr_P(abuf, F("\">Fernseher</button>"));
        return buf;
    }
    if (0 == strcmp_P(var.c_str(), PSTR("ACTIVE_CHANNEL")))
    {
        sprintf(buf, "%d", (deviceManager->getActiveChannelIndex() + 1));
        return buf;
    }
    if (0 == strcmp_P(var.c_str(), PSTR("SETCHANNEL_LINKS")))
    {
        char *appendBuf = buf;
        for (byte i = 0; i < DEVICECOUNT; ++i)
        {
            if (deviceManager->isRealDevice(i))
            {
                String name = deviceManager->getDeviceName(i);
                appendBuf += sprintf(appendBuf, "<a href=\"/device/set?d=%d\">%.35s</a> &nbsp; ", (i + 1), &name[0]);
            }
        }
        return buf;
    }
    if (0 == strcmp_P(var.c_str(), PSTR("RESET_REASON")))
    {
        switch (rtc_get_reset_reason(0))
        {
        case 1:
            return F("POWERON_RESET"); /**<1,  Vbat power on reset*/
        case 3:
            return F("SW_RESET"); /**<3,  Software reset digital core*/
        case 4:
            return F("OWDT_RESET"); /**<4,  Legacy watch dog reset digital core*/
        case 5:
            return F("DEEPSLEEP_RESET"); /**<5,  Deep Sleep reset digital core*/
        case 6:
            return F("SDIO_RESET"); /**<6,  Reset by SLC module, reset digital core*/
        case 7:
            return F("TG0WDT_SYS_RESET"); /**<7,  Timer Group0 Watch dog reset digital core*/
        case 8:
            return F("TG1WDT_SYS_RESET"); /**<8,  Timer Group1 Watch dog reset digital core*/
        case 9:
            return F("RTCWDT_SYS_RESET"); /**<9,  RTC Watch dog Reset digital core*/
        case 10:
            return F("INTRUSION_RESET"); /**<10, Instrusion tested to reset CPU*/
        case 11:
            return F("TGWDT_CPU_RESET"); /**<11, Time Group reset CPU*/
        case 12:
            return F("SW_CPU_RESET"); /**<12, Software reset CPU*/
        case 13:
            return F("RTCWDT_CPU_RESET"); /**<13, RTC Watch dog Reset CPU*/
        case 14:
            return F("EXT_CPU_RESET"); /**<14, for APP CPU, reseted by PRO CPU*/
        case 15:
            return F("RTCWDT_BROWN_OUT_RESET"); /**<15, Reset when the vdd voltage is not stable*/
        case 16:
            return F("RTCWDT_RTC_RESET"); /**<16, RTC Watch dog reset digital core and rtc module*/
        default:
            return F("NO_MEAN");
        }
    }
    if (0 == strcmp_P(var.c_str(), PSTR("SYSTIME")))
    {
        if (ui.isNtpTimeValid())
        {
            sprintf(buf, "%lu @ %s", millis(), timeClient->getFormattedTime().c_str());
            return buf;
        }
        else
        {
            sprintf(buf, "%lu ms", millis());
            return buf;
        }
    }
    if (0 == strcmp_P(var.c_str(), PSTR("UPSINCE")))
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
    if (0 == strcmp_P(var.c_str(), PSTR("REFRESHINDEXTAG")))
        return getRefreshTag("/index.html");
    if (0 == strcmp_P(var.c_str(), PSTR("REFRESHLOGTAG")))
        return getRefreshTag("/log.html");
    if (0 == strcmp_P(var.c_str(), PSTR("REFRESHINDEXLINK")))
        return getRefreshLink("/index.html");
    if (0 == strcmp_P(var.c_str(), PSTR("REFRESHLOGLINK")))
        return getRefreshLink("/log.html");
    if (0 == strcmp_P(var.c_str(), PSTR("USERMESSAGE")))
    {
        if (ui.hasUiError())
        {
            sprintf(buf, PSTR("<h3 style='color:red;'>%s</h3>"), &ui.getUiErrorMessage()[0]);
            return buf;
        }
        else
            return "";
    }
    if (0 == strcmp_P(var.c_str(), PSTR("LOG0")))
        return ui.getHtmlLog(0);
    if (0 == strcmp_P(var.c_str(), PSTR("LOG1")))
        return ui.getHtmlLog(1);
    if (0 == strcmp_P(var.c_str(), PSTR("MANUAL_TEXT")))
        return F(manual_txt_de);
    if (var == "CSS_STYLE")
        return F(css_style);
    if (0 == strcmp_P(var.c_str(), PSTR("IRRCV_BUTTON")))
    {
        char *abuf = appendstr_P(buf, F("<button onclick=\"location.href='/irrcv?e="));
        *abuf++ = irRecvEnabled ? '0' : '1';
        abuf = appendstr_P(abuf, F("';\" class=\"button "));
        if (irRecvEnabled)
            abuf = appendstr_P(abuf, F("active"));
        else
            abuf = appendstr_P(abuf, F("shutdown"));
        appendstr_P(abuf, F("\">IR-Dump</button>"));
        return buf;
    }
    if (0 == strcmp_P(var.c_str(), PSTR("VOICE_COMMANDS")))
    {
        char *abuf = appendstr_P(buf, F("<li>&quot;Alexa, <b>" DEVICENAME_VOLUME "</b> auf 20 [%%]&quot;<br/><i>Der Prozentwert entspricht dem Wunsch-Wert des Soundbar (0.." VOLUME_MAXVALUE_STR ")</i></li>\n"));
        abuf = appendstr_P(abuf, F("<li>&quot;Alexa, <b>" DEVICENAME_VOLUME_IS "</b> auf 16 [%%]&quot;<br/><i>der Prozentwert entspricht dem aktull angezeigten Wert im Soundbar (0.." VOLUME_MAXKNOWNVALUE_STR " zulässig)</i></li>\n"));
        abuf = appendstr_P(abuf, F("</ul><p>Kanalumschaltung &amp; Ger&auml;teaktivierung:</p><ul>\n"));
        for (byte i = 0; i < DEVICECOUNT; ++i)
        {
            abuf = appendstr_P(abuf, F("<li>&quot;Alexa, <b>"));
            abuf = appendstr(abuf, deviceManager->getDeviceName(i).c_str());
            abuf = appendstr_P(abuf, F("</b> an / aus&quot;</li>\n"));
        }
        return buf;
    }
    ui.logError() << F("DEBUG: variable not found: ") << var << endl;
    return F("???");
}

static byte convertAlexaValueToPercent(char value)
{
    const int v = 100 * value;
    return (v / 255) + (((v % 255) > 127) ? 1 : 0); // 255 : 100% = v : x -> alexa sends "24%" as value=61=255*0.24
}

static void setDeviceState(AsyncWebServerRequest *request, const bool newState)
{
    if (request->hasParam(PARAM_DEVICEID))
    {
        const String deviceId = request->getParam(PARAM_DEVICEID)->value();
        ui.logDebug() << F("setting device ") << deviceId << F(" state to ") << newState << endl;
        deviceManager->setDeviceStatus(deviceId, newState);
        request->redirect(F("/index.html"));
    }
    else
    {
        ui.logError() << F("Aufruf /device/on ohne deviceId") << endl;
        request->send(400, F("text/html"), F("Error"));
    }
}
static void setChannel(AsyncWebServerRequest *request)
{
    if (request->hasParam("d"))
    {
        const String deviceId = request->getParam("d")->value();
        ui.logDebug() << F("setting active channel to ") << deviceId << endl;
        deviceManager->setActiveChannel(deviceId);
        request->redirect(F("/index.html"));
    }
    else
    {
        ui.logError(F("Aufruf /device/set ohne deviceId"));
        request->send(400, "text/html", "Error");
    }
}
static void selectChannel(AsyncWebServerRequest *request, const int direction)
{
    ui.logDebug() << F("shifting channel by ") << direction << endl;
    deviceManager->selectChannel(direction);
    request->redirect(F("/index.html"));
}
static void adaptVolume(AsyncWebServerRequest *request, const int direction)
{
    ui.logDebug() << F("adapting volume by ") << direction << endl;
    deviceManager->setVolumeAbsolute(deviceManager->getVolume() + direction);
    request->redirect(F("/index.html"));
}

static void setIrReceive(AsyncWebServerRequest *request)
{
    if (request->hasParam("e"))
    {
        const String enabled = request->getParam("e")->value();
        ui.logDebug() << F("setting irrcv mode to ") << enabled << endl;
        irRecvEnabled = (enabled == "1");
        if (irRecvEnabled)
        {
            irrecv.enableIRIn();
            request->redirect(F("/log.html"));
        }
        else
        {
            irrecv.disableIRIn();
            // old solution
            //request->redirect("/irRcvOff.html");
            // reset to disable IR receive (timers)
            //ESP.restart();
            request->redirect(F("/log.html"));
        }
    }
    else
    {
        ui.logError(F("Aufruf /irrcv ohne enabled"));
        request->send(400, "text/html", "Error");
    }
}

void serverSetup()
{
    // webUI control page
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        adjustWebuiParameters(request);
        if (request->hasParam("knownVolume"))
        {
            String volume = request->getParam("knownVolume")->value();
            int v = volume.toInt();
            if (0 != v)
            {
                deviceManager->setKnownVolume(v);
            }
            else
            {
                ui.logInfo() << "invalid value for volume, got " << volume << endl;
                ui.reportUiError("invalid value for volume, must be a number between 0 and 45", 3);
            }
        }
        request->send_P(200, "text/html", index_html, placeholderProcessor);
    });
    server.on("/log.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        adjustWebuiParameters(request);
        request->send_P(200, "text/html", log_html, placeholderProcessor);
    });
    server.on("/manual.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", manual_html, placeholderProcessor);
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
    server.on("/volume/up", HTTP_GET, [](AsyncWebServerRequest *request) {
        adaptVolume(request, +1);
    });
    server.on("/volume/down", HTTP_GET, [](AsyncWebServerRequest *request) {
        adaptVolume(request, -1);
    });
    server.on("/device/set", HTTP_GET, [](AsyncWebServerRequest *request) {
        setChannel(request);
    });
    server.on("/device/main/off", HTTP_GET, [](AsyncWebServerRequest *request) {
        ui.logDebug() << F("setting active channel status to OFF\n");
        deviceManager->setActiveChannel("off");
        request->redirect(F("/index.html"));
    });
    server.on("/device/main/on", HTTP_GET, [](AsyncWebServerRequest *request) {
        ui.logDebug() << "setting active channel status to ON\n";
        deviceManager->setActiveChannel("on");
        request->redirect(F("/index.html"));
    });
    server.on("/irrcv", HTTP_GET, [](AsyncWebServerRequest *request) {
        setIrReceive(request);
    });
    server.on("/log/memory", HTTP_GET, [](AsyncWebServerRequest *request) {
        ui.logInfo() << F("free heap = ") << ESP.getFreeHeap() << endl;
        request->redirect(F("/log.html"));
    });
    server.on("/sleep/now", HTTP_GET, [](AsyncWebServerRequest *request) {
        webuiRefresh = 0;
        ui.logWarn() << F("on manual request: going to SLEEP") << endl;
        ui.statusActive("going to sleep immediately");
        request->redirect(F("/log.html"));
        yield();
        performDeepSleep();
    });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, F("image/x-icon"), favicon_ico_gzip, favicon_ico_len);
        response->addHeader(F("Content-Encoding"), F("gzip"));
        request->send(response);
    });
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect(F("/index.html"));
    });

    // These two callbacks are required for gen1 and gen3 compatibility
    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (fauxmo->process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data)))
            return;
        // Handle any other body request here...
        ui.logInfo() << F("unknown request, uri=") << request->url() << endl;
    });
    server.onNotFound([](AsyncWebServerRequest *request) {
        String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
        if (fauxmo->process(request->client(), request->method() == HTTP_GET, request->url(), body))
            return;
        // Handle not found request here...
        ui.logInfo() << F("unknown uri=") << request->url() << endl;
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
        p << F("Unknown encoding: ");
    }
    else if (results->decode_type == NEC)
    {
        p << F("Decoded NEC: ");
    }
    else if (results->decode_type == SONY)
    {
        p << F("Decoded SONY: ");
    }
    else if (results->decode_type == RC5)
    {
        p << F("Decoded RC5: ");
    }
    else if (results->decode_type == RC6)
    {
        p << F("Decoded RC6: ");
    }
    else if (results->decode_type == PANASONIC)
    {
        p << F("Decoded PANASONIC - Address: ");
        p << _HEX(results->address);
        p << F(" Value: ");
    }
    else if (results->decode_type == LG)
    {
        p << F("Decoded LG: ");
    }
    else if (results->decode_type == JVC)
    {
        p << F("Decoded JVC: ");
    }
    else if (results->decode_type == AIWA_RC_T501)
    {
        p << F("Decoded AIWA RC T501: ");
    }
    else if (results->decode_type == WHYNTER)
    {
        p << F("Decoded Whynter: ");
    }

    p << _HEX(results->value);
    p << F(" (") << _DEC(results->bits) << F(" bits)") << endl;
#ifdef VERBOSE_IRDUMP
    p = ui.logInfo();
    const int count = results->rawlen;
    p << F("Raw (") << _DEC(count) << F("): ");

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
    if (deviceManager->isDeviceOn(DeviceManager::DEVICEID_MAIN))
    {
        ui.logInfo() << F("manual channel switch with button, active was ") << _lastActiveChannel << endl;
        deviceManager->selectChannel(+1);
    }
    else if (deviceManager->isPendingActivity(DeviceManager::DEVICEID_MAIN))
    {
        ui.logWarn() << F("ignored button press while main device pending activity") << endl;
    }
    else
    {
        ui.logInfo() << F("manually switched ON main device using channel ") << _lastActiveChannel << endl;
        deviceManager->setDeviceStatus(_lastActiveChannel, true);
    }
}

void printWakeupReason()
{
    switch (esp_sleep_get_wakeup_cause())
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        ui.logInfo() << F("Wakeup caused by external signal using RTC_IO") << endl;
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        ui.logInfo() << F("Wakeup caused by external signal using RTC_CNTL") << endl;
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        ui.logInfo() << F("Wakeup caused by timer") << endl;
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        ui.logInfo() << F("Wakeup caused by touchpad") << endl;
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        ui.logInfo() << F("Wakeup caused by ULP program") << endl;
        break;
    default:
        ui.logInfo() << F("another Wakeup reason (") << esp_sleep_get_wakeup_cause() << F("): ") << placeholderProcessor("RESET_REASON") << endl;
        break;
    }
}

unsigned long lastSleepCheckMs = 0;
void checkForSleep()
{
    const unsigned long now = millis();
    if (deviceManager->isAnyActivity() || !ui.isNtpTimeValid())
    {
        lastSleepCheckMs = now;
        webuiRefreshEnabled = true;
        return;
    }

    if (now - lastSleepCheckMs > DEEPSLEEP_INACTIVITY) // 1min passed, go to sleep
    {
        // check for allowed time frame
        const int nowHour = timeClient->getHours();
        if (DEEPSLEEP_DISALLOW_HOURS)
        {
            lastSleepCheckMs = now;
            return;
        }

        // preparing deep sleep
        if (webuiRefreshEnabled)
        {
            ui.logInfo() << F("going to sleep") << endl;
            ui.statusActive("going to sleep"); // TODO overload function for use with F()
            ui.setBlink(50, 250);
            webuiRefreshEnabled = false;
        }
        if (now - lastSleepCheckMs > DEEPSLEEP_WAITPERIOD)
        {
            performDeepSleep();
        }
    }
}

void setup()
{
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    powerOnLed.init(PIN_LED_POWER);
    powerOnLed.setBlink(-1, 0);
    powerOnLed.update();

    ui.setNtpClient(timeClient);
    ui.init(PIN_LED_STATUS, F(__FILE__), F(__TIMESTAMP__));

    printWakeupReason();
    serverSetup();
    fauxmo->createServer(false); // not needed, this is the default value
    fauxmo->setPort(80);         // This is required for gen3 devices
    deviceManager->addDevices(fauxmo);
    fauxmo->addDevice(DEVICENAME_VOLUME_IS);
    fauxmo->addDevice(DEVICENAME_VOLUME);
    deviceManager->init();
    fauxmo->enable(true);

    fauxmo->onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value) {
        // Callback when a command from Alexa is received.
        if (0 == strcmp_P(device_name, PSTR(DEVICENAME_VOLUME)))
        {
            ui.logInfo() << F("alexa: setting volume to ") << value << endl;
            value = convertAlexaValueToPercent(value);
            if (value <= VOLUME_MAXVALUE)
                deviceManager->setVolume(value);
            else
            {
                ui.logWarn() << F("invalid value: ") << value << endl;
                ui.reportUiError("invalid value", USERINPUT_WARNBLINK_DURATION);
            }
        }
        else if (0 == strcmp_P(device_name, PSTR(DEVICENAME_VOLUME_IS)))
        {
            ui.logInfo() << F("alexa: setting known volume to ") << value << endl;
            deviceManager->setKnownVolume(convertAlexaValueToPercent(value));
        }
        else
        {
            ui.logInfo() << F("alexa command: #") << device_id << F(" '") << device_name << F("' to state=") << state << F(", value=") << value << endl;
            deviceManager->setDeviceStatus(device_id, state);
        }
    });

    button.attachClick(buttonClick);
    powerOnLed.setBlink(50, 2950);

    if (ui.isNtpTimeValid())
    {
        upSince = timeClient->getFormattedTime();
        ui.logInfo() << PSTR("set upSince to ") << upSince << endl;
    }
}

void loop()
{
    if (ui.handle())
    {
        fauxmo->handle();
        deviceManager->handle();
        button.tick();
        powerOnLed.update();

        if (irRecvEnabled && irrecv.decode(&results))
        {
            printIrResults(&results);
            irrecv.decode(&results);
            irrecv.resume(); // Receive the next value
        }

        checkForSleep();

        // output free heap once a minute, as a cheap way to detect memory leaks
        if (0 == (millis() % 60000))
        {
            Serial.printf(PSTR("[MAIN] Free heap: %d bytes\n"), ESP.getFreeHeap());
            delay(1);
        }
    }
}
