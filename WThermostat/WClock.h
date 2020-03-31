
/*********************************************************************************************\
*  TimeZone Handling based on
*        Time by Michael Margolis and Paul Stoffregen (https://github.com/PaulStoffregen/Time)
*        Timezone by Jack Christensen (https://github.com/JChristensen/Timezone)
*        Tasmota by Theo Arends (https://github.com/arendst/Tasmota)
\*********************************************************************************************/

#ifndef __WCLOCK_H__
#define __WCLOCK_H__

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <Time.h>
#include <TimeLib.h>
#include <WiFiUdp.h>

#include "../lib/WAdapter/Wadapter/WDevice.h"
#include "../lib/WAdapter/Wadapter/WNetwork.h"
#include "Arduino.h"

const static char* DEFAULT_NTP_SERVER = "pool.ntp.org";
const static char* DEFAULT_TIME_ZONE = "99";
const static char* DEFAULT_TIME_DST = "0,3,0,2,120";
const static char* DEFAULT_TIME_STD = "0,10,0,3,60";

const static char HTTP_TEXT_CLOCK_HOWTO[] PROGMEM = R"=====(
<div style="max-width: 400px">
See <a href="https://github.com/fashberg/WThermostatBeca/blob/master/Configuration.md#4-configure-clock-settings"
target="_blank">https://github.com/fashberg/WThermostatBeca/blob/master/Configuration.md#4-configure-clock-settings</a> 
for Howto set Timezone and Daylight-Savings
</div>
)=====";

typedef struct {
    bool initialized;
    uint16_t week;   // bits 1 - 3   = 0=Last week of the month, 1=First, 2=Second, 3=Third, 4=Fourth
    uint16_t month;  // bits 4 - 7   = 1=Jan, 2=Feb, ... 12=Dec
    uint16_t dow;    // bits 8 - 10  = day of week, 1=Sun, 2=Mon, ... 7=Sat
    uint16_t hour;   // bits 11 - 15 = 0-23
    int16_t offset;
} TimeRule;

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day_of_week;  // sunday is day 1
    uint8_t day_of_month;
    uint8_t month;
    char name_of_month[4];
    uint16_t day_of_year;
    uint16_t year;
    unsigned long days;
    unsigned long valid;
} TIME_T;

typedef struct {
    uint32_t utc_time = 0;
    uint32_t local_time = 0;
    uint32_t daylight_saving_time = 0;
    uint32_t standard_time = 0;
    uint32_t ntp_time = 0;
    uint32_t midnight = 0;
    uint32_t restart_time = 0;
    int32_t time_timezone = 0;
    uint8_t ntp_sync_minute = 0;
    bool midnight_now = false;
    bool user_time_entry = false;               // Override NTP by user setting
} RTC;

#define LEAP_YEAR(Y) ((( Y) > 0) && !((Y) % 4) && (((Y) % 100) || !((Y) % 400)))

static const uint8_t kDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};  // API starts months from 1, this array starts from 0
static const char kMonthNames[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
const uint32_t START_VALID_TIME = 1451602800;  // Time is synced and after 2016-01-01

class WClock : public WDevice {
   public:
    typedef std::function<void(void)> THandlerFunction;
    typedef std::function<void(const char*)> TErrorHandlerFunction;

    //WClock(bool debug, WNetwork *network) {
    WClock(WNetwork* network, String applicationName)
        : WDevice(network, "clock", "clock", network->getIdx(), DEVICE_TYPE_TEXT_DISPLAY) {
        this->mainDevice = false;
        this->visibility = MQTT;
        this->ntpServer = network->getSettings()->setString("ntpServer", 32, DEFAULT_NTP_SERVER);
        this->ntpServer->setReadOnly(true);
        this->ntpServer->setVisibility(MQTT);
        this->addProperty(ntpServer);
        this->timeZoneConfig = network->getSettings()->setString("timeZone", 6, DEFAULT_TIME_ZONE);
        // upgrade hack
        String tzval=(String)this->timeZoneConfig->c_str();
        if (tzval.startsWith("http://")){
            network->log()->warning(PSTR("Changing old http Timezone to default"));
            this->timeZoneConfig->setString(DEFAULT_TIME_ZONE);
        }
        this->timeZoneConfig->setReadOnly(true);
        this->timeZoneConfig->setVisibility(MQTT);
        this->addProperty(timeZoneConfig);
        this->timeDST = network->getSettings()->setString("timeDST", 14, DEFAULT_TIME_DST);
        this->timeDST->setReadOnly(true);
        this->timeDST->setVisibility(MQTT);
        this->addProperty(timeDST);
        this->timeSTD = network->getSettings()->setString("timeSTD", 14, DEFAULT_TIME_STD);
        this->timeSTD->setReadOnly(true);
        this->timeSTD->setVisibility(MQTT);
        this->addProperty(timeSTD);

        parseTimeZone(this->timeZoneConfig->c_str(), &this->timezone_hours, &this->timezone_minutes);
        if (this->timezone_hours == 99) {
            parseTimeRule(this->timeDST->c_str(), &this->timeRuleDst);
            parseTimeRule(this->timeSTD->c_str(), &this->timeRuleStd);
        } else {
            resetTimeRule(&this->timeRuleStd);
            resetTimeRule(&this->timeRuleDst);
        }

        network->log()->notice(PSTR("TimeZoneAndRules: TZ:%s, DST:%s, STD:%s"),
                               getTimeZoneIntegerToString(this->timezone_hours, this->timezone_minutes).c_str(),
                               getTimeRuleToString(this->timeRuleDst).c_str(),
                               getTimeRuleToString(this->timeRuleStd).c_str()
                               );


        Rtc.utc_time = 0;
        BreakTime(Rtc.utc_time, RtcTime);



        this->epochTime = new WLongProperty("epochTime");
        this->epochTime->setReadOnly(true);
        this->epochTime->setOnValueRequest([this](WProperty* p) {
            p->setLong(getEpochTime());
        });
        this->addProperty(epochTime);
        this->epochTimeLocalFormatted = new WStringProperty("epochTimeLocalFormatted", "epochTimeLocalFormatted", 19);
        this->epochTimeLocalFormatted->setReadOnly(true);
        this->epochTimeLocalFormatted->setOnValueRequest([this](WProperty* p) { updateLocalFormattedTime(); });
        this->addProperty(epochTimeLocalFormatted);
        this->validTime = new WOnOffProperty("validTime", "validTime");
        this->validTime->setBoolean(false);
        this->validTime->setReadOnly(true);
        this->addProperty(validTime);
        this->offset = new WIntegerProperty("offset", "Offset");
        this->offset->setInteger(0);
        this->offset->setReadOnly(true);
        this->addProperty(offset);
        lastTry = lastNtpSync = ntpTime =  lastRun = 0;
    }

    void loop(unsigned long now) {
        TIME_T tmpTime;
        const int ntpRetryMinutes = 1;
        const int ntpResyncMinutes = 30;
        const int ntpInvalidateHours = 30;
        bool notify=false;
        //Invalid after 3 hours
        if (isValidTime() && !(now - lastNtpSync < (ntpInvalidateHours * 60 * 60 * 1000) )){
            network->log()->error(F("No valid NTP Time since %d hours -> invalidating"), ntpInvalidateHours);
        }
        validTime->setBoolean((lastNtpSync > 0) && (now - lastNtpSync < (ntpInvalidateHours * 60 * 60 * 1000)));

        Rtc.utc_time=getEpochTime();

        if (((!isValidTime()) && ((lastTry == 0) 
            || ( (now - lastNtpSync > ntpResyncMinutes * 60 * 1000) || (now - lastTry > ntpRetryMinutes * 60 * 1000) ) ))
            && (WiFi.status() == WL_CONNECTED)) {
            network->log()->trace(F("Time via NTP server '%s'"), ntpServer->c_str());
            WiFiUDP ntpUDP;
            NTPClient ntpClient(ntpUDP, ntpServer->c_str());
            //ntpClient.begin();
            //delay(500);
            lastTry = now;
            if (ntpClient.update()) {
                ntpTime = ntpClient.getEpochTime();
                //ntpTime = 1603587544; // DST-Test
                //ntpTime = 1585555170; // Full Hour Test
                if (ntpTime > START_VALID_TIME) {
                    uint32_t oldutc = getEpochTime();
                    Rtc.utc_time = ntpTime;
                    BreakTime(ntpTime, tmpTime);
                    RtcTime.year = tmpTime.year;
                    Rtc.daylight_saving_time = RuleToTime(this->timeRuleDst, RtcTime.year);
                    Rtc.standard_time = RuleToTime(this->timeRuleStd, RtcTime.year);
                    network->log()->notice(F("NTP time synced: (%04d-%02d-%02d %02d:%02d:%02d, Weekday: %d, Epoch: %d, Dst: %d, Std: %d, Diff: %d)"),
                            tmpTime.year, tmpTime.month, tmpTime.day_of_month, tmpTime.hour, tmpTime.minute, tmpTime.second, tmpTime.day_of_week,
                            ntpTime, Rtc.daylight_saving_time, Rtc.standard_time, ntpTime - oldutc );

                    validTime->setBoolean(true);
                    notify=true;
                    lastNtpSync = now;
                } else {
                    network->log()->error(F("NTP reports invalid time. (%d)"), ntpTime);
                }
            } else {
                network->log()->warning(F("NTP sync failed. "));
            }
                 
        }

        // -------------
        if (!notify && lastRun > millis()-1000) return;
        lastRun=millis();

        network->log()->verbose(F("Clock %d / %d / %d:%d"), getEpochTime(),  Rtc.utc_time,  this->timezone_hours, this->timezone_minutes);

        Rtc.local_time = Rtc.utc_time + Rtc.time_timezone;
        BreakTime(Rtc.local_time, RtcTime);

        if (notify || (RtcTime.minute==0 && RtcTime.second==0)){
            if (Rtc.local_time > START_VALID_TIME) {  // 2016-01-01
                network->log()->trace(F("Clock Recalc Timezone %d"),  Rtc.local_time);
                int32_t newTz=Rtc.time_timezone;
                if (this->timezone_hours!= 99) {
                    newTz = (this->timezone_hours * SECS_PER_HOUR) + (this->timezone_minutes * SECS_PER_MIN * (this->timezone_hours < 0 ? -1 : 1));

                } else {
                    int32_t dstoffset = this->timeRuleDst.offset * SECS_PER_MIN;
                    int32_t stdoffset = this->timeRuleStd.offset * SECS_PER_MIN;
                    if (Rtc.standard_time > Rtc.daylight_saving_time) { 
                        // Southern hemisphere
                        if ((Rtc.utc_time >= (Rtc.standard_time - dstoffset)) && (Rtc.utc_time < (Rtc.daylight_saving_time - stdoffset))) {
                            newTz = stdoffset;  // Standard Time
                        } else {
                            newTz = dstoffset;  // Daylight Saving Time
                        }
                    } else {
                        // Northern hemisphere
                        if ((Rtc.utc_time >= (Rtc.daylight_saving_time - stdoffset)) && (Rtc.utc_time < (Rtc.standard_time - dstoffset))) {
                            newTz = dstoffset;  // Daylight Saving Time
                        } else {
                            newTz = stdoffset;  // Standard Time
                        }
                    }
                }
                if (Rtc.time_timezone!=newTz){
                    network->log()->notice(F("TZ Change %d -> %d"), Rtc.time_timezone, newTz);
                    Rtc.time_timezone = newTz;
                    notify=true;
                    Rtc.local_time = Rtc.utc_time + Rtc.time_timezone;
                    BreakTime(Rtc.local_time, RtcTime);
                } 
            }
        }

        /* 
        if (RtcTime.valid) {
            if (!Rtc.midnight) {
                Rtc.midnight = Rtc.local_time - (RtcTime.hour * 3600) - (RtcTime.minute * 60) - RtcTime.second;
            }
            if (!RtcTime.hour && !RtcTime.minute && !RtcTime.second) {
                Rtc.midnight = Rtc.local_time;
                Rtc.midnight_now = true;
            }
        }
        */

        if (notify){
            this->offset->setReadOnly(false);
            this->offset->setInteger(Rtc.time_timezone);
            this->offset->setReadOnly(true);
            network->log()->notice(F("NotifyNewDateTime"));
            notifyOnTimeUpdate();
        }
    }

    void setOnTimeUpdate(THandlerFunction onTimeUpdate) {
        this->onTimeUpdate = onTimeUpdate;
    }

    void setOnError(TErrorHandlerFunction onError) {
        this->onError = onError;
    }

    unsigned long getEpochTime() {
        return (lastNtpSync > 0 ? ntpTime + ((millis() - lastNtpSync) / 1000) : 0);
    }

    unsigned long getEpochTimeLocal() {
        return (lastNtpSync > 0 ? ntpTime + getOffset() + ((millis() - lastNtpSync) / 1000) : 0);
    }

    byte getWeekDay() {
        return RtcTime.day_of_week -1;
    }

    byte getWeekDay(unsigned long epochTime) {
        //weekday from 0 to 6, 0 is Sunday
        TIME_T tmpTime;
        BreakTime(epochTime, tmpTime);
        return tmpTime.day_of_week -1;
    }

    byte getHours() {
        return RtcTime.hour;
    }

    byte getHours(unsigned long epochTime) {
        TIME_T tmpTime;
        BreakTime(epochTime, tmpTime);
        return tmpTime.hour;
    }

    byte getMinutes() {
        return RtcTime.minute;
    }

    byte getMinutes(unsigned long epochTime) {
        TIME_T tmpTime;
        BreakTime(epochTime, tmpTime);
        return tmpTime.minute;
    }

    byte getSeconds() {
        return RtcTime.second;
    }

    byte getSeconds(unsigned long epochTime) {
        TIME_T tmpTime;
        BreakTime(epochTime, tmpTime);
        return tmpTime.second;
    }

    int getYear() {
        return RtcTime.year;
    }

    int getYear(unsigned long epochTime) {
        TIME_T tmpTime;
        BreakTime(epochTime, tmpTime);
        return tmpTime.year;
    }

    byte getMonth() {
        return RtcTime.month;
    }

    byte getMonth(unsigned long epochTime) {
        TIME_T tmpTime;
        BreakTime(epochTime, tmpTime);
        //month from 1 to 12
        return tmpTime.month;
    }

    byte getDay() {
        return RtcTime.day_of_month;
    }

    byte getDay(unsigned long epochTime) {
        TIME_T tmpTime;
        BreakTime(epochTime, tmpTime);
        //day from 1 to 31
        return tmpTime.day_of_month;
    }

    bool isTimeLaterThan(byte hours, byte minutes) {
        return ((getHours() > hours) || ((getHours() == hours) && (getMinutes() >= minutes)));
    }

    bool isTimeEarlierThan(byte hours, byte minutes) {
        return ((getHours() < hours) || ((getHours() == hours) && (getMinutes() < minutes)));
    }

    bool isTimeBetween(byte hours1, byte minutes1, byte hours2, byte minutes2) {
        return ((isTimeLaterThan(hours1, minutes1)) && (isTimeEarlierThan(hours2, minutes2)));
    }

    void updateLocalFormattedTime() {
        updateFormattedTimeImpl(getEpochTimeLocal());
    }

    void updateFormattedTimeImpl(unsigned long rawTime) {
        WStringStream* stream = new WStringStream(19);
        char buffer[5];
        //year
        int _year = year(rawTime);
        itoa(_year, buffer, 10);
        stream->print(buffer);
        stream->print("-");
        //month
        uint8_t _month = month(rawTime);
        if (_month < 10) stream->print("0");
        itoa(_month, buffer, 10);
        stream->print(buffer);
        stream->print("-");
        //month
        uint8_t _day = day(rawTime);
        if (_day < 10) stream->print("0");
        itoa(_day, buffer, 10);
        stream->print(buffer);
        stream->print(" ");
        //hours
        unsigned long _hours = (rawTime % 86400L) / 3600;
        if (_hours < 10) stream->print("0");
        itoa(_hours, buffer, 10);
        stream->print(buffer);
        stream->print(":");
        //minutes
        unsigned long _minutes = (rawTime % 3600) / 60;
        if (_minutes < 10) stream->print("0");
        itoa(_minutes, buffer, 10);
        stream->print(buffer);
        stream->print(":");
        //seconds
        unsigned long _seconds = rawTime % 60;
        if (_seconds < 10) stream->print("0");
        itoa(_seconds, buffer, 10);
        stream->print(buffer);

        epochTimeLocalFormatted->setString(stream->c_str());
        delete stream;
    }

    bool isValidTime() {
        return validTime->getBoolean();
    }

    bool isClockSynced() {
        return (lastNtpSync > 0);
    }

    long getOffset() {
        return offset->getInteger();
    }

    void printConfigPage(WStringStream* page) {
        network->log()->notice(F("Clock config page"));
        page->printAndReplace(FPSTR(HTTP_CONFIG_PAGE_BEGIN), getId());
        page->printAndReplace(FPSTR(HTTP_TEXT_FIELD), "NTP server:", "ntp", "32", ntpServer->c_str());
        page->printAndReplace(FPSTR(HTTP_TEXT_FIELD), "Timezone:", "tzone", "6", timeZoneConfig->c_str());
        page->printAndReplace(FPSTR(HTTP_TEXT_FIELD), "TimeRule switch to Daylight Saving Time:", "tdst", "14", timeDST->c_str());
        page->printAndReplace(FPSTR(HTTP_TEXT_FIELD), "TimeRule switch Back to Standard Time:", "tstd", "14", timeSTD->c_str());
        page->print(FPSTR(HTTP_TEXT_CLOCK_HOWTO));
        page->print(FPSTR(HTTP_CONFIG_SAVE_BUTTON));
    }

    void saveConfigPage(ESP8266WebServer* webServer) {
        network->log()->notice(F("Save clock config page"));
        this->ntpServer->setString(webServer->arg("ntp").c_str());
        this->timeZoneConfig->setString(webServer->arg("tzone").c_str());
        this->timeDST->setString(webServer->arg("tdst").c_str());
        this->timeSTD->setString(webServer->arg("tstd").c_str());
        /* reboot follows */
    }

   private:
    THandlerFunction onTimeUpdate;
    TErrorHandlerFunction onError;
    unsigned long lastTry, lastNtpSync, ntpTime, lastRun;
    WProperty* epochTime;
    WProperty* epochTimeLocalFormatted;
    WProperty* validTime;
    WProperty* ntpServer;
    WProperty* timeZoneConfig;
    WProperty* timeDST;
    WProperty* timeSTD;
    WProperty* offset;

    TimeRule timeRuleStd;
    TimeRule timeRuleDst;
    int16_t timezone_hours;
    int16_t timezone_minutes;

    RTC Rtc;
    TIME_T RtcTime;



    void notifyOnTimeUpdate() {
        if (onTimeUpdate) {
            onTimeUpdate();
        }
    }

    void notifyOnError(const char* error) {
        network->log()->notice(error);
        if (onError) {
            onError(error);
        }
    }

    char* Trim(char* p) {
        while ((*p != '\0') && isblank(*p)) {
            p++;
        }  // Trim leading spaces
        char* q = p + strlen(p) - 1;
        while ((q >= p) && isblank(*q)) {
            q--;
        }  // Trim trailing spaces
        q++;
        *q = '\0';
        return p;
    }

    void resetTimeRule(TimeRule* tr) {
        tr->week = 0;
        tr->month = 0;
        tr->dow = 0;
        tr->hour = 0;
        tr->offset = 0;
        tr->initialized = false;
    }

    void parseTimeRule(const char* ts, TimeRule* tr) {
        resetTimeRule(tr);
        // TimeStd 0/1/2/3/4, 1..12, 1..7, 0..23, +/-780
        if (strlen(ts)) {
            uint32_t tpos = 0;  // Parameter index
            int value = 0;
            char* p = (char*)ts;  // Parameters like "1, 2,3 , 4 ,5, -120" or ",,,,,+240"
            char* q = p;          // Value entered flag
            while (p && (tpos < 7)) {
                if (p > q) {  // Any value entered
                    if (1 == tpos) {
                        tr->week = (value < 0) ? 0 : (value > 4) ? 4 : value;
                    }
                    if (2 == tpos) {
                        tr->month = (value < 1) ? 1 : (value > 12) ? 12 : value;
                    }
                    if (3 == tpos) {
                        tr->dow = (value < 1) ? 1 : (value > 7) ? 7 : value;
                    }
                    if (4 == tpos) {
                        tr->hour = (value < 0) ? 0 : (value > 23) ? 23 : value;
                    }
                    if (5 == tpos) {
                        tr->offset = (value < -900) ? -900 : (value > 900) ? 900 : value;
                    }
                }
                p = Trim(p);  // Skip spaces
                if (tpos && (*p == ',')) {
                    p++;
                }             // Skip separator
                p = Trim(p);  // Skip spaces
                q = p;        // Reset any value entered flag
                value = strtol(p, &p, 10);
                tpos++;  // Next parameter
            }
            tr->initialized = true;
        }
    }

    String getTimeRuleToString(TimeRule tr) {
        char buf[64];
        size_t size = sizeof buf;
        snprintf_P(buf, size, PSTR("\"Week\":%d,\"Month\":%d,\"Day\":%d,\"Hour\":%d,\"Offset\":%d"),
                   tr.week, tr.month, tr.dow, tr.hour, tr.offset);
        return (String)buf;
    }

    String getTimeZoneIntegerToString(int16_t tz_h, int16_t tz_m) {
        /*
        int8_t tz_h = tz / 60;
        int8_t tz_m = abs(tz % 60);
        */
        char buf[7];  //+13:30\0
        size_t size = sizeof buf;
        if (tz_h==99){
            snprintf_P(buf, size, PSTR("DSTSTD"));
        } else {
            snprintf_P(buf, size, PSTR("%+03d:%02d"), tz_h, tz_m);
        }
        return (String)buf;
    }

    void parseTimeZone(const char* ts, int16_t *tz_h_param, int16_t *tz_m_param){
        int16_t tz_h = 0;
        int16_t tz_m = 0;
        char* data = strdup(ts);
        if (strlen(ts)) {
            char* p = strtok(data, ":");

            tz_h = strtol(p, nullptr, 10);
            if (tz_h > 13 || tz_h < -13) {
                tz_h = 99;
            }
            if (p) {
                p = strtok(nullptr, ":");
                if (p) {
                    tz_m = strtol(p, nullptr, 10);
                    if (tz_m > 59) {
                        tz_m = 59;
                    } else if (tz_m < 0) {
                        tz_m = 0;
                    }
                }
            }
        } else {
            tz_h = 99;
            tz_m = 0;
        }
        *tz_h_param=tz_h;
        *tz_m_param=tz_m;
        //return (tz_h * 60) + (tz_h >= 0 ? tz_m : tz_m * -1);
    }

    void BreakTime(uint32_t time_input, TIME_T& tm) {
        // break the given time_input into time components
        // this is a more compact version of the C library localtime function
        uint8_t year;
        uint8_t month;
        uint8_t month_length;
        uint32_t time;
        uint32_t days;

        time = time_input;
        tm.second = time % 60;
        time /= 60;  // now it is minutes
        tm.minute = time % 60;
        time /= 60;  // now it is hours
        tm.hour = time % 24;
        time /= 24;  // now it is days
        tm.days = time;
        tm.day_of_week = ((time + 4) % 7) + 1;  // Sunday is day 1

        year = 0;
        days = 0;
        while ((unsigned)(days += (LEAP_YEAR(year+1970) ? 366 : 365)) <= time) {
            year++;
        }
        tm.year = year+1970;  // year is offset from 1970

        days -= LEAP_YEAR(year) ? 366 : 365;
        time -= days;  // now it is days in this year, starting at 0
        tm.day_of_year = time;

        for (month = 0; month < 12; month++) {
            if (1 == month) {  // february
                if (LEAP_YEAR(year)) {
                    month_length = 29;
                } else {
                    month_length = 28;
                }
            } else {
                month_length = kDaysInMonth[month];
            }

            if (time >= month_length) {
                time -= month_length;
            } else {
                break;
            }
        }
        strlcpy(tm.name_of_month, kMonthNames + (month * 3), 4);
        tm.month = month + 1;                        // jan is month 1
        tm.day_of_month = time + 1;                  // day of month
        tm.valid = (time_input > START_VALID_TIME);  // 2016-01-01
    }

        uint32_t MakeTime(TIME_T& tm) {
        // assemble time elements into time_t

        int i;
        uint32_t seconds;

        // seconds from 1970 till 1 jan 00:00:00 of the given year
        seconds = (tm.year - 1970) * (SECS_PER_DAY * 365);
        for (i = 1970; i < tm.year; i++) {
            if (LEAP_YEAR(i)) {
                seconds += SECS_PER_DAY;  // add extra days for leap years
            }
        }

        // add days for this year, months start from 1
        for (i = 1; i < tm.month; i++) {
            if ((2 == i) && LEAP_YEAR(tm.year)) {
                seconds += SECS_PER_DAY * 29;
            } else {
                seconds += SECS_PER_DAY * kDaysInMonth[i - 1];  // monthDay array starts from 0
            }
        }
        seconds += (tm.day_of_month - 1) * SECS_PER_DAY;
        seconds += tm.hour * SECS_PER_HOUR;
        seconds += tm.minute * SECS_PER_MIN;
        seconds += tm.second;
        return seconds;
    }

    uint32_t RuleToTime(TimeRule r, int yr) {
        TIME_T tm;
        uint32_t t;
        uint8_t m;
        uint8_t w;  // temp copies of r.month and r.week

        m = r.month;
        w = r.week;
        if (0 == w) {        // Last week = 0
            if (++m > 12) {  // for "Last", go to the next month
                m = 1;
                yr++;
            }
            w = 1;  // and treat as first week of next month, subtract 7 days later
        }

        tm.hour = r.hour;
        tm.minute = 0;
        tm.second = 0;
        tm.day_of_month = 1;
        tm.month = m;
        tm.year = yr;
        t = MakeTime(tm);  // First day of the month, or first day of next month for "Last" rules
        BreakTime(t, tm);
        t += (7 * (w - 1) + (r.dow - tm.day_of_week + 7) % 7) * SECS_PER_DAY;
        if (0 == r.week) {
            t -= 7 * SECS_PER_DAY;  // back up a week if this is a "Last" rule
        }
        return t;
    }


};

#endif
