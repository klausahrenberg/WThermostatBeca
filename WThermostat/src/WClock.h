#ifndef __WCLOCK_H__
#define __WCLOCK_H__

#include "Arduino.h"
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#elif ESP32
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#endif
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Time.h>
#include <TimeLib.h>
#include "WDevice.h"
#include "WNetwork.h"

const char* DEFAULT_NTP_SERVER = "pool.ntp.org";
const char* DEFAULT_TIME_ZONE_SERVER = "http://worldtimeapi.org/api/ip";
const byte STD_MONTH = 0;
const byte STD_WEEK = 1;
const byte STD_WEEKDAY = 2;
const byte STD_HOUR = 3;
const byte DST_MONTH = 4;
const byte DST_WEEK = 5;
const byte DST_WEEKDAY = 6;
const byte DST_HOUR = 7;
const byte *DEFAULT_DST_RULE = (const byte[]){10, 0, 0, 3, 3, 0, 0, 2};
const byte *DEFAULT_NIGHT_SWITCHES = (const byte[]){22, 00, 7, 00};

class WClock: public WDevice {
public:
	typedef std::function<void(void)> THandlerFunction;

	WClock(WNetwork* network, bool supportNightMode)
		: WDevice(network, "clock", "clock", DEVICE_TYPE_TEXT_DISPLAY) {
		this->mainDevice = false;
		this->visibility = MQTT;
		this->ntpServer = network->getSettings()->setString("ntpServer", DEFAULT_NTP_SERVER);
		this->ntpServer->setReadOnly(true);
		this->ntpServer->setVisibility(MQTT);
		this->addProperty(ntpServer);
		this->useTimeZoneServer = network->getSettings()->setBoolean("useTimeZoneServer", true);
		this->useTimeZoneServer->setReadOnly(true);
		this->useTimeZoneServer->setVisibility(NONE);
		this->addProperty(useTimeZoneServer);
		this->timeZoneServer = network->getSettings()->setString("timeZoneServer", DEFAULT_TIME_ZONE_SERVER);
		this->timeZoneServer->setReadOnly(true);
		this->timeZoneServer->setVisibility(this->useTimeZoneServer->getBoolean() ? MQTT : NONE);
		//this->ntpServer->setVisibility(MQTT);
		this->addProperty(timeZoneServer);
		this->epochTime = WProperty::createUnsignedLongProperty("epochTime", "epochTime");
		this->epochTime->setReadOnly(true);
		this->epochTime->setOnValueRequest([this](WProperty* p) {
			p->setUnsignedLong(getEpochTime());
		});
		this->addProperty(epochTime);
		this->epochTimeFormatted = WProperty::createStringProperty("epochTimeFormatted", "epochTimeFormatted");
		this->epochTimeFormatted->setReadOnly(true);
		this->epochTimeFormatted->setOnValueRequest([this](WProperty* p) {updateFormattedTime();});
		this->addProperty(epochTimeFormatted);
		this->validTime = WProperty::createOnOffProperty("validTime", "validTime");
		this->validTime->setBoolean(false);
		this->validTime->setReadOnly(true);
		this->addProperty(validTime);
		if (this->useTimeZoneServer->getBoolean()) {
			this->timeZone = WProperty::createStringProperty("timezone", "timeZone");
			this->timeZone->setReadOnly(true);
			this->addProperty(timeZone);
		} else {
			this->timeZone = nullptr;
		}
		this->rawOffset = WProperty::createIntegerProperty("raw_offset", "rawOffset");
		this->rawOffset->setInteger(0);
		this->rawOffset->setVisibility(NONE);
		this->network->getSettings()->add(this->rawOffset);
		this->rawOffset->setReadOnly(true);
		this->addProperty(rawOffset);
		this->dstOffset = WProperty::createIntegerProperty("dst_offset", "dstOffset");
		this->dstOffset->setInteger(0);
		this->dstOffset->setVisibility(NONE);
		this->network->getSettings()->add(this->dstOffset);
		this->dstOffset->setReadOnly(true);
		this->addProperty(dstOffset);
		this->useDaySavingTimes = network->getSettings()->setBoolean("useDaySavingTimes", false);
		this->useDaySavingTimes->setVisibility(NONE);
		this->dstRule = network->getSettings()->setByteArray("dstRule", DEFAULT_DST_RULE);
		//HtmlPages
    WPage* configPage = new WPage(this->getId(), "Configure clock");
    configPage->setPrintPage(std::bind(&WClock::printConfigPage, this, std::placeholders::_1, std::placeholders::_2));
    configPage->setSubmittedPage(std::bind(&WClock::saveConfigPage, this, std::placeholders::_1, std::placeholders::_2));
    network->addCustomPage(configPage);

		lastTry = lastNtpSync = lastTimeZoneSync = ntpTime = dstStart = dstEnd = 0;
		failedTimeZoneSync = 0;
		//enableNightMode
		this->enableNightMode = nullptr;
		this->nightMode = nullptr;
		this->nightSwitches = nullptr;
		if (supportNightMode) {
			this->enableNightMode = network->getSettings()->setBoolean("enableNightMode", true);
			this->nightMode = WProperty::createBooleanProperty("nightMode", "nightMode");
			this->addProperty(this->nightMode);
			this->nightSwitches = network->getSettings()->setByteArray("nightSwitches", DEFAULT_NIGHT_SWITCHES);
		}
		this->wifiClient = nullptr;
	}

	void loop(unsigned long now) {
		//Invalid after 3 hours
		validTime->setBoolean((lastNtpSync > 0) && ((!this->useTimeZoneServer->getBoolean()) || (lastTimeZoneSync > 0)) && (now - lastTry < (3 * 60 * 60000)));

		if (((lastTry == 0) || (now - lastTry > 10000))
		    && (WiFi.status() == WL_CONNECTED)) {
			bool timeUpdated = false;
			//1. Sync ntp
			if ((!isValidTime())
			    && ((lastNtpSync == 0) || (now - lastNtpSync > 60000))) {
				network->debug(F("Time via NTP server '%s'"), ntpServer->c_str());
				WiFiUDP ntpUDP;
				NTPClient ntpClient(ntpUDP, ntpServer->c_str());
				if (ntpClient.update()) {
					lastNtpSync = millis();
					ntpTime = ntpClient.getEpochTime();
					this->calculateDstStartAndEnd();
					validTime->setBoolean(!this->useTimeZoneServer->getBoolean());
					network->debug(F("NTP time synced: %s"), epochTimeFormatted->c_str());
					timeUpdated = true;
				} else {
					network->error(F("NTP sync failed. "));
				}
			}
			//2. Sync time zone
			if ((!isValidTime())
			    && ((lastNtpSync > 0) && ((lastTimeZoneSync == 0) || (now - lastTimeZoneSync > 60000)))
					&& (useTimeZoneServer->getBoolean())
				  && (!timeZoneServer->equalsString(""))) {
				String request = timeZoneServer->c_str();
				network->debug(F("Time zone update via '%s'"), request.c_str());
				HTTPClient http;
				if (this->wifiClient == nullptr) {
					this->wifiClient = new WiFiClient();
				}
				http.begin(*wifiClient, request);
				int httpCode = http.GET();
				if (httpCode > 0) {
					WJsonParser parser;
					this->timeZone->setReadOnly(false);
					this->rawOffset->setReadOnly(false);
					this->dstOffset->setReadOnly(false);
					WProperty* property = parser.parse(http.getString().c_str(), this);
					this->timeZone->setReadOnly(true);
					this->rawOffset->setReadOnly(true);
					this->dstOffset->setReadOnly(true);
					if (property != nullptr) {
						failedTimeZoneSync = 0;
						lastTimeZoneSync = millis();
						validTime->setBoolean(true);
						network->debug(F("Time zone evaluated. Current local time: %s"), epochTimeFormatted->c_str());
						timeUpdated = true;
					} else {
						failedTimeZoneSync++;
						network->error(F("Time zone update failed. (%d. attempt): Wrong html response."), failedTimeZoneSync);
					}
				} else {
					failedTimeZoneSync++;
					network->error(F("Time zone update failed (%d. attempt): http code %d"), failedTimeZoneSync, httpCode);
				}
				http.end();
				if (failedTimeZoneSync == 3) {
					failedTimeZoneSync = 0;
					lastTimeZoneSync = millis();
				}
			}
			//check nightMode
			if ((validTime) && (this->enableNightMode) && (this->enableNightMode->getBoolean())) {
				this->nightMode->setBoolean(this->isTimeBetween(this->nightSwitches->getByteArrayValue(0), this->nightSwitches->getByteArrayValue(1),
																												this->nightSwitches->getByteArrayValue(2), this->nightSwitches->getByteArrayValue(3)));
			}
			if (timeUpdated) {
				notifyOnTimeUpdate();
			}
			lastTry = millis();
		}
	}

	void setOnTimeUpdate(THandlerFunction onTimeUpdate) {
		this->onTimeUpdate = onTimeUpdate;
	}

	unsigned long getEpochTime() {
		return getEpochTime(true);
	}

	byte getWeekDay() {
		return getWeekDay(getEpochTime());
	}

	static byte getWeekDay(unsigned long epochTime) {
		//weekday from 0 to 6, 0 is Sunday
		return (((epochTime / 86400L) + 4) % 7);
	}

	byte getHours() {
		return getHours(getEpochTime());
	}

	static byte getHours(unsigned long epochTime) {
		return ((epochTime % 86400L) / 3600);
	}

	byte getMinutes() {
		return getMinutes(getEpochTime());
	}

	static byte getMinutes(unsigned long epochTime) {
		return ((epochTime % 3600) / 60);
	}

	byte getSeconds() {
		return getSeconds(getEpochTime());
	}

	static byte getSeconds(unsigned long epochTime) {
		return (epochTime % 60);
	}

	int getYear() {
		return getYear(getEpochTime());
	}

	static int getYear(unsigned long epochTime) {
		return year(epochTime);
	}

	byte getMonth() {
		return getMonth(getEpochTime());
	}

	static byte getMonth(unsigned long epochTime) {
		//month from 1 to 12
		return month(epochTime);
	}

	byte getDay() {
		return getDay(getEpochTime());
	}

	static byte getDay(unsigned long epochTime) {
		//day from 1 to 31
		return day(epochTime);
	}

	static bool isTimeLaterThan(byte epochTimeHours, byte epochTimeMinutes, byte hours, byte minutes) {
		return ((epochTimeHours > hours) || ((epochTimeHours == hours) && (epochTimeMinutes >= minutes)));
	}

	bool isTimeLaterThan(byte hours, byte minutes) {
		return isTimeLaterThan(getHours(), getMinutes(), hours, minutes);
	}

	bool isTimeEarlierThan(byte hours, byte minutes) {
		return ((getHours() < hours) || ((getHours() == hours) && (getMinutes() < minutes)));
	}

	bool isTimeBetween(byte fromHours, byte fromMinutes, byte toHours, byte toMinutes) {
		if (isTimeLaterThan(fromHours, fromMinutes, toHours, toMinutes)) {
			//e.g. 22:00-06:00
			return ((isTimeLaterThan(fromHours, fromMinutes)) || (isTimeEarlierThan(toHours, toMinutes)));
		} else {
			//e.g. 06:00-22:00
			return ((isTimeLaterThan(fromHours, fromMinutes)) && (isTimeEarlierThan(toHours, toMinutes)));
		}
	}

	void updateFormattedTime() {
		WStringStream* stream = updateFormattedTime(getEpochTime());
		epochTimeFormatted->setString(stream->c_str());
		delete stream;
	}

	static WStringStream* updateFormattedTime(unsigned long rawTime) {
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

		return stream;
	}

	bool isValidTime() {
		return validTime->getBoolean();
	}

	bool isClockSynced() {
		return ((lastNtpSync > 0) && (lastTimeZoneSync > 0));
	}

	int getRawOffset() {
		return rawOffset->getInteger();
	}

	int getDstOffset() {
		return (useTimeZoneServer->getBoolean() || isDaySavingTime() ? dstOffset->getInteger() : 0);
	}

	void printConfigPage(AsyncWebServerRequest* request, Print* page) {
    	page->printf(HTTP_CONFIG_PAGE_BEGIN, getId());
			page->printf(HTTP_TOGGLE_GROUP_STYLE, "ga", (useTimeZoneServer->getBoolean() ? HTTP_BLOCK : HTTP_NONE), "gb", (useTimeZoneServer->getBoolean() ? HTTP_NONE : HTTP_BLOCK));
			page->printf(HTTP_TOGGLE_GROUP_STYLE, "gd", (useDaySavingTimes->getBoolean() ? HTTP_BLOCK : HTTP_NONE), "ge", HTTP_NONE);
			if (this->enableNightMode) {
				page->printf(HTTP_TOGGLE_GROUP_STYLE, "gn", (enableNightMode->getBoolean() ? HTTP_BLOCK : HTTP_NONE), "gm", HTTP_NONE);
			}
			//NTP Server
			page->printf(HTTP_TEXT_FIELD, "NTP server:", "ntp", "32", ntpServer->c_str());

			page->print(FPSTR(HTTP_DIV_BEGIN));
			page->printf(HTTP_RADIO_OPTION, "sa", "sa", HTTP_TRUE, (useTimeZoneServer->getBoolean() ? HTTP_CHECKED : ""), "tg()", "Get time zone via internet");
			page->printf(HTTP_RADIO_OPTION, "sb", "sa", HTTP_FALSE, (useTimeZoneServer->getBoolean() ? "" : HTTP_CHECKED), "tg()", "Use fixed offset to UTC time");
			page->print(FPSTR(HTTP_DIV_END));

			page->printf(HTTP_DIV_ID_BEGIN, "ga");
			page->printf(HTTP_TEXT_FIELD, "Time zone server:", "tz", "64", timeZoneServer->c_str());
			page->print(FPSTR(HTTP_DIV_END));
			page->printf(HTTP_DIV_ID_BEGIN, "gb");
			page->printf(HTTP_TEXT_FIELD, "Fixed offset to UTC in minutes:", "ro", "5", String(rawOffset->getInteger() / 60).c_str());

			page->printf(HTTP_CHECKBOX_OPTION, "sd", "sd", (useDaySavingTimes->getBoolean() ? HTTP_CHECKED : ""), "td()", "Calculate day saving time (summer time)");
			page->printf(HTTP_DIV_ID_BEGIN, "gd");
			page->print(F("<table  class='st'>"));
				page->print(F("<tr>"));
					page->print(F("<th></th>"));
					page->print(F("<th>Standard time</th>"));
					page->print(F("<th>Day saving time<br>(summer time)</th>"));
				page->print(F("</tr>"));
				page->print(F("<tr>"));
					page->print(F("<td>Offset to standard time in minutes</td>"));
					page->print(F("<td></td>"));
					page->print(F("<td>"));
					page->printf(HTTP_INPUT_FIELD, "do", "5", String(dstOffset->getInteger() / 60).c_str());
					page->print(F("</td>"));
				page->print(F("</tr>"));
				page->print(F("<tr>"));
					page->print(F("<td>Month [1..12]</td>"));
					page->print(F("<td>"));
					page->printf(HTTP_INPUT_FIELD, "rm", "2", String(dstRule->getByteArrayValue(STD_MONTH)).c_str());
					page->print(F("</td>"));
					page->print(F("<td>"));
					page->printf(HTTP_INPUT_FIELD, "dm", "2", String(dstRule->getByteArrayValue(DST_MONTH)).c_str());
					page->print(F("</td>"));
				page->print(F("</tr>"));
				page->print(F("<tr>"));
					page->print(F("<td>Week [0: last week of month; 1..4]</td>"));
					page->print(F("<td>"));
					page->printf(HTTP_INPUT_FIELD, "rw", "1", String(dstRule->getByteArrayValue(STD_WEEK)).c_str());
					page->print(F("</td>"));
					page->print(F("<td>"));
					page->printf(HTTP_INPUT_FIELD, "dw", "1", String(dstRule->getByteArrayValue(DST_WEEK)).c_str());
					page->print(F("</td>"));
				page->print(F("</tr>"));
				page->print(F("<tr>"));
					page->print(F("<td>Weekday [0:sunday .. 6:saturday]</td>"));
					page->print(F("<td>"));
					page->printf(HTTP_INPUT_FIELD, "rd", "1", String(dstRule->getByteArrayValue(STD_WEEKDAY)).c_str());
					page->print(F("</td>"));
					page->print(F("<td>"));
					page->printf(HTTP_INPUT_FIELD, "dd", "1", String(dstRule->getByteArrayValue(DST_WEEKDAY)).c_str());
					page->print(F("</td>"));
				page->print(F("</tr>"));
				page->print(F("<tr>"));
					page->print(F("<td>Hour [0..23]</td>"));
					page->print(F("<td>"));
					page->printf(HTTP_INPUT_FIELD, "rh", "2", String(dstRule->getByteArrayValue(STD_HOUR)).c_str());
					page->print(F("</td>"));
					page->print(F("<td>"));
					page->printf(HTTP_INPUT_FIELD, "dh", "2", String(dstRule->getByteArrayValue(DST_HOUR)).c_str());
					page->print(F("</td>"));
				page->print(F("</tr>"));
			page->print(F("</table>"));
			page->print(FPSTR(HTTP_DIV_END));
			page->print(FPSTR(HTTP_DIV_END));
			if (this->enableNightMode) {
				//nightMode
				page->printf(HTTP_CHECKBOX_OPTION, "sn", "sn", (enableNightMode->getBoolean() ? HTTP_CHECKED : ""), "tn()", "Enable support for night mode");
				page->printf(HTTP_DIV_ID_BEGIN, "gn");
				page->print(F("<table  class='settingstable'>"));
					page->print(F("<tr>"));
						char timeFrom[6];
						snprintf(timeFrom, 6, "%02d:%02d", this->nightSwitches->getByteArrayValue(0), this->nightSwitches->getByteArrayValue(1));
						page->print(F("<td>from"));
						page->printf(HTTP_INPUT_FIELD, "nf", "5", timeFrom);
						page->print(F("</td>"));
						char timeTo[6];
						snprintf(timeTo, 6, "%02d:%02d", this->nightSwitches->getByteArrayValue(2), this->nightSwitches->getByteArrayValue(3));
						page->print(F("<td>to"));
						page->printf(HTTP_INPUT_FIELD, "nt", "5", timeTo);
						page->print(F("</td>"));
					page->print(F("</tr>"));
				page->print(F("</table>"));
				page->print(FPSTR(HTTP_DIV_END));
				page->printf(HTTP_TOGGLE_FUNCTION_SCRIPT, "tn()", "sn", "gn", "gm");
			}
			page->printf(HTTP_TOGGLE_FUNCTION_SCRIPT, "tg()", "sa", "ga", "gb");
			page->printf(HTTP_TOGGLE_FUNCTION_SCRIPT, "td()", "sd", "gd", "ge");
    	page->print(FPSTR(HTTP_CONFIG_SAVE_BUTTON));
	}

	void saveConfigPage(AsyncWebServerRequest* request, Print* page) {
		this->ntpServer->setString(request->arg("ntp").c_str());
		this->timeZoneServer->setString(request->arg("tz").c_str());
		this->useTimeZoneServer->setBoolean(request->arg("sa") == HTTP_TRUE);
		this->useDaySavingTimes->setBoolean(request->arg("sd") == HTTP_TRUE);
		this->rawOffset->setInteger(atol(request->arg("ro").c_str()) * 60);
		this->dstOffset->setInteger(atol(request->arg("do").c_str()) * 60);
		this->dstRule->setByteArrayValue(STD_MONTH, atoi(request->arg("rm").c_str()));
		this->dstRule->setByteArrayValue(STD_WEEK, atoi(request->arg("rw").c_str()));
		this->dstRule->setByteArrayValue(STD_WEEKDAY, atoi(request->arg("rd").c_str()));
		this->dstRule->setByteArrayValue(STD_HOUR, atoi(request->arg("rh").c_str()));
		this->dstRule->setByteArrayValue(DST_MONTH, atoi(request->arg("dm").c_str()));
		this->dstRule->setByteArrayValue(DST_WEEK, atoi(request->arg("dw").c_str()));
		this->dstRule->setByteArrayValue(DST_WEEKDAY, atoi(request->arg("dd").c_str()));
		this->dstRule->setByteArrayValue(DST_HOUR, atoi(request->arg("dh").c_str()));
		if (this->enableNightMode) {
			this->enableNightMode->setBoolean(request->arg("sn") == HTTP_TRUE);
			processNightModeTime(0, request->arg("nf").c_str());
			processNightModeTime(2, request->arg("nt").c_str());
		}
	}

	WProperty* getEpochTimeFormatted() {
		return epochTimeFormatted;
	}

	WProperty* nightMode;

private:
	THandlerFunction onTimeUpdate;
	unsigned long lastTry, lastNtpSync, lastTimeZoneSync, ntpTime;
	unsigned long dstStart, dstEnd;
	byte failedTimeZoneSync;
	WProperty* epochTime;
	WProperty* epochTimeFormatted;
	WProperty* validTime;
	WProperty* ntpServer;
	WProperty* useTimeZoneServer;
	WProperty* timeZoneServer;
	WProperty* timeZone;
	WProperty* rawOffset;
	WProperty* dstOffset;
	WProperty* useDaySavingTimes;
	WProperty* dstRule;
	WProperty* enableNightMode;
	WProperty* nightSwitches;
	WiFiClient* wifiClient;

	void notifyOnTimeUpdate() {
		if (onTimeUpdate) {
			onTimeUpdate();
		}
	}

	unsigned long getEpochTime(bool useDstOffset) {
		return (lastNtpSync > 0 ? ntpTime + getRawOffset() + (useDstOffset ? getDstOffset() : 0) + ((millis() - lastNtpSync) / 1000) :	0);
	}

	unsigned long getEpochTime(int year, byte month, byte week, byte weekday, byte hour) {
		tmElements_t ds;
		ds.Year = (week == 0 && month == 12 ? year + 1 : year) - 1970;
		ds.Month = (week == 0 ? (month == 12 ? 1 : month + 1) : month);
		ds.Day = 1;
		ds.Hour = hour;
		ds.Minute = 0;
		ds.Second = 0;
		unsigned long tt = makeTime(ds);
		byte iwd = getWeekDay(tt);
		if (week == 0) {
			//last week of last month
			short diffwd = iwd - weekday;
			diffwd = (diffwd <= 0 ? diffwd + 7 : diffwd);
			tt = tt - (diffwd * 60 * 60 * 24);
		} else {
			short diffwd = weekday - iwd;
			diffwd = (diffwd < 0 ? diffwd + 7 : diffwd);
			tt = tt + (((7 * (week - 1)) + diffwd) * 60 * 60 * 24);
		}
		return tt;
	}

	void calculateDstStartAndEnd() {
		if ((!this->useTimeZoneServer->getBoolean()) && (this->useDaySavingTimes->getBoolean())) {
			int year = getYear(getEpochTime(false));
			dstStart = getEpochTime(year, dstRule->getByteArrayValue(DST_MONTH), dstRule->getByteArrayValue(DST_WEEK), dstRule->getByteArrayValue(DST_WEEKDAY), dstRule->getByteArrayValue(DST_HOUR));
			WStringStream* stream = updateFormattedTime(dstStart);
			network->debug(F("DST start is: %s"), stream->c_str());
			delete stream;
			dstEnd = getEpochTime(year, dstRule->getByteArrayValue(STD_MONTH), dstRule->getByteArrayValue(STD_WEEK), dstRule->getByteArrayValue(STD_WEEKDAY), dstRule->getByteArrayValue(STD_HOUR));
			stream = updateFormattedTime(dstEnd);
			network->debug(F("STD start is: %s"), stream->c_str());
			delete stream;
		}
	}

	bool isDaySavingTime() {
		if ((!this->useTimeZoneServer->getBoolean()) && (this->useDaySavingTimes->getBoolean())) {
			if ((this->dstStart != 0) && (this->dstEnd != 0)) {
				unsigned long now = getEpochTime(false);
				if (getYear(now) != getYear(dstStart)) {
					calculateDstStartAndEnd();
				}
				if (dstStart < dstEnd) {
					return ((now >= dstStart) && (now < dstEnd));
				} else {
					return ((now < dstEnd) || (now >= dstStart));
				}
			} else {
				return false;
			}
		} else {
			return (dstOffset->getInteger() != 0);
		}
	}

	void processNightModeTime(byte arrayIndex, String timeStr) {
		timeStr = (timeStr.length() == 4 ? "0" + timeStr : timeStr);
		if (timeStr.length() == 5) {
			byte hh = timeStr.substring(0, 2).toInt();
			byte mm = timeStr.substring(3, 5).toInt();
			this->nightSwitches->setByteArrayValue(arrayIndex, hh);
			this->nightSwitches->setByteArrayValue(arrayIndex + 1, mm);
		}
	}

};

#endif
