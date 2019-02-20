#include "KaClock.h"

#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Time.h>
#include <TimeLib.h>
#include <KaHttpClient.h>

KaClock::KaClock(bool debug) {
	this->debug = debug;
	lastTry = lastNtpSync = lastTimeZoneSync = ntpTime = 0;
	validTime = false;
	rawOffset = 0;
	dstOffset = 0;
	timeZone = "";
}

KaClock::~KaClock() {

}

void KaClock::setOnTimeUpdate(THandlerFunction onTimeUpdate) {
	this->onTimeUpdate = onTimeUpdate;
}

void KaClock::log(String debugMessage) {
	if (debug) {
		Serial.println(debugMessage);
	}
}

void KaClock::notifyOnTimeUpdate() {
	if (onTimeUpdate) {
		onTimeUpdate();
	}
}

void KaClock::loop() {
	unsigned long now = millis();
	//Invalid after 3 hours
	validTime = ((lastNtpSync > 0) && (lastTimeZoneSync > 0)
			&& (now - lastTry < (3 * 60 * 60000)));

	if (((!validTime) && ((lastTry == 0) || (now - lastTry > 60000)))
			&& (WiFi.status() == WL_CONNECTED)) {
		//1. Sync ntp
		if ((lastNtpSync == 0) || (now - lastNtpSync > 60000)) {
			String ntpServer = "de.pool.ntp.org";
			log("Time via NTP server '" + ntpServer + "'");
			WiFiUDP ntpUDP;
			NTPClient ntpClient(ntpUDP, ntpServer.c_str());
			if (ntpClient.update()) {
				lastNtpSync = millis();
				ntpTime = ntpClient.getEpochTime();
				log("NTP time: " + getFormattedTime());
				notifyOnTimeUpdate();
			} else {
				log("NTP sync failed");
			}
		}
		//2. Sync time zone
		if (((lastNtpSync > 0)
				&& ((lastTimeZoneSync == 0) || (now - lastTimeZoneSync > 60000)))
				&& (WiFi.status() == WL_CONNECTED)) {
			String request = "http://worldtimeapi.org/api/ip";
			/*String request = "https://maps.googleapis.com/maps/api/timezone/json?location=" +
			 String(latitude) + "," + String(longitude) +
			 "&timestamp=" + String(ntpTime - lastNtpSync + millis());*/
			log("Time zone update via '" + request + "'");
			HTTPClient http;
			http.begin(request); //, gMapsCrt);
			int httpCode = http.GET();
			if (httpCode > 0) {
				String payload = http.getString();
				StaticJsonBuffer<400> JSONBuffer;
				JsonObject& parsed = JSONBuffer.parseObject(payload);
				if (parsed.success()) {
					//log(payload);
					/*
					 * {"week_number":"08",
					 *  "utc_offset":"+09:00",
					 *  "unixtime":"1550654131",
					 *  "timezone":"Asia/Seoul",
					 *  "dst_until":null,
					 *  "dst_from":null,
					 *  "dst":false,
					 *  "day_of_year":51,
					 *  "day_of_week":3,
					 *  "datetime":"2019-02-20T18:15:31.300495+09:00",
					 *  "abbreviation":"KST"}
					 */
					String utcOffset = parsed["utc_offset"];
					rawOffset = utcOffset.substring(1, 3).toInt() * 3600
							+ utcOffset.substring(4, 6).toInt() * 60;
					dstOffset = (parsed["dst"] == true ? 3600 : 0);
					String tz = parsed["timezone"];
					this->timeZone = tz;
					lastTimeZoneSync = millis();
					validTime = true;
					log(
							"Time zone evaluated. Current local time: "
									+ getFormattedTime());
					notifyOnTimeUpdate();
				} else {
					log("Parsing failed");
				}
			} else {
				log("Time zone update failed: " + httpCode);
			}
			http.end();   //Close connection
		}
		lastTry = millis();
	}
}

unsigned long KaClock::getEpochTime() {
	return (lastNtpSync > 0 ?
			ntpTime + rawOffset + dstOffset
					+ ((millis() - lastNtpSync) / 1000) :
			0);
}

byte KaClock::getWeekDay() {
	return getWeekDay(getEpochTime());
}

byte KaClock::getWeekDay(unsigned long epochTime) {
	//weekday from 0 to 6, 0 is Sunday
	return (((epochTime / 86400L) + 4) % 7);
}

byte KaClock::getHours() {
	return ((getEpochTime() % 86400L) / 3600);
}

byte KaClock::getMinutes() {
	return ((getEpochTime() % 3600) / 60);
}

byte KaClock::getSeconds() {
	return (getEpochTime() % 60);
}

int KaClock::getYear() {
	return year(getEpochTime());
}

byte KaClock::getMonth() {
	//month from 1 to 12
	return month(getEpochTime());
}

byte KaClock::getDay() {
	//day from 1 to 31
	return day(getEpochTime());
}

String KaClock::getTimeZone() {
	//timeZone set during local time sync to e.g. "Europe/Berlin";
	//could be empty
	return timeZone;
}

String KaClock::getFormattedTime() {
	return getFormattedTime(getEpochTime());
}

String KaClock::getFormattedTime(unsigned long rawTime) {
	uint16_t _year = year(rawTime);
	String yearStr = String(_year);

	uint8_t _month = month(rawTime);
	String monthStr = _month < 10 ? "0" + String(_month) : String(_month);

	uint8_t _day = day(rawTime);
	String dayStr = _day < 10 ? "0" + String(_day) : String(_day);

	unsigned long hours = (rawTime % 86400L) / 3600;
	String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

	unsigned long minutes = (rawTime % 3600) / 60;
	String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

	unsigned long seconds = rawTime % 60;
	String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

	return yearStr + "-" + monthStr + "-" + dayStr + " " + hoursStr + ":"
			+ minuteStr + ":" + secondStr;
}

bool KaClock::isValidTime() {
	return validTime;
}

bool KaClock::isClockSynced() {
	return ((lastNtpSync > 0) && (lastTimeZoneSync > 0));
}

long KaClock::getDstOffset() {
	return dstOffset;
}

long KaClock::getRawOffset() {
	return rawOffset;
}

void KaClock::getMqttState(JsonObject& json) {
	json["clockTime"] = getFormattedTime();
	json["clockTimeRaw"] = getEpochTime();
	json["validTime"] = isValidTime();
	json["lastNtpSync"] = (
			lastNtpSync > 0 ?
					getFormattedTime(
							ntpTime + rawOffset + dstOffset
									+ (lastNtpSync / 1000)) :
					"n.a.");
	json["lastTimeZoneSync"] = (
			lastTimeZoneSync > 0 ?
					getFormattedTime(
							ntpTime + rawOffset + dstOffset
									+ (lastTimeZoneSync / 1000)) :
					"n.a.");
	json["dstOffset"] = getDstOffset();
	json["rawOffset"] = getRawOffset();
	json["timeZone"] = getTimeZone();
}

