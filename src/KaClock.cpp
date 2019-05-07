#include "KaClock.h"

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Time.h>
#include <TimeLib.h>

KaClock::KaClock(bool debug, String ntpServer) {
	this->debug = debug;
	this->ntpServer = ntpServer;
	lastTry = lastNtpSync = lastTimeZoneSync = ntpTime = 0;
	validTime = false;
	rawOffset = 0;
	//dstOffset = 0;
	timeZone = "";
}

KaClock::~KaClock() {

}

void KaClock::setOnTimeUpdate(THandlerFunction onTimeUpdate) {
	this->onTimeUpdate = onTimeUpdate;
}

void KaClock::setOnError(TErrorHandlerFunction onError) {
	this->onError = onError;
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

void KaClock::notifyOnError(String error) {
	log(error);
	if (onError) {
		onError(error);
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
			log("Time via NTP server '" + ntpServer + "'");
			WiFiUDP ntpUDP;
			NTPClient ntpClient(ntpUDP, ntpServer.c_str());
			if (ntpClient.update()) {
				lastNtpSync = millis();
				ntpTime = ntpClient.getEpochTime();
				log("NTP time: " + getFormattedTime());
				notifyOnTimeUpdate();
			} else {
				notifyOnError("NTP sync failed: ");
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
				StaticJsonBuffer<1024> JSONBuffer;
				JsonObject& parsed = JSONBuffer.parseObject(payload);
				if (parsed.success()) {
					//log(payload);
					/*
					 * {
					 *  "week_number":19,
					 *  "utc_offset":"+09:00",
					 *  "utc_datetime":"2019-05-07T23:32:15.214725+00:00",
					 *  "unixtime":1557271935,
					 *  "timezone":"Asia/Seoul",
					 *  "raw_offset":32400,
					 *  "dst_until":null,
					 *  "dst_offset":0,
					 *  "dst_from":null,
					 *  "dst":false,
					 *  "day_of_year":128,
					 *  "day_of_week":3,
					 *  "datetime":"2019-05-08T08:32:15.214725+09:00",
					 *  "abbreviation":"KST"}
					 */
					/*
					 * {"week_number":19,
					 *  "utc_offset":"+02:00",
					 *  "utc_datetime":"2019-05-07T23:37:41.963463+00:00",
					 *  "unixtime":1557272261,
					 *  "timezone":"Europe/Berlin",
					 *  "raw_offset":7200,
					 *  "dst_until":"2019-10-27T01:00:00+00:00",
					 *  "dst_offset":3600,
					 *  "dst_from":"2019-03-31T01:00:00+00:00",
					 *  "dst":true,
					 *  "day_of_year":128,
					 *  "day_of_week":3,
					 *  "datetime":"2019-05-08T01:37:41.963463+02:00",
					 *  "abbreviation":"CEST"}
					 */
					String utcOffset = parsed["utc_offset"];
					rawOffset = utcOffset.substring(1, 3).toInt() * 3600
							+ utcOffset.substring(4, 6).toInt() * 60;
					//dstOffset = (parsed["dst"] == true ? 3600 : 0);
					String tz = parsed["timezone"];
					this->timeZone = tz;
					lastTimeZoneSync = millis();
					validTime = true;
					log(
							"Time zone evaluated. Current local time: "
									+ getFormattedTime());
					notifyOnTimeUpdate();
				} else {
					notifyOnError("Can't parse json of time zone request result: " + payload);
				}
			} else {
				notifyOnError("Time zone update failed: " + httpCode);
			}
			http.end();   //Close connection
		}
		lastTry = millis();
	}
}

unsigned long KaClock::getEpochTime() {
	return (lastNtpSync > 0 ?
			ntpTime + rawOffset /*+ dstOffset*/
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
	return getHours(getEpochTime());
}

byte KaClock::getHours(unsigned long epochTime) {
	return ((epochTime % 86400L) / 3600);
}

byte KaClock::getMinutes() {
	return getMinutes(getEpochTime());
}

byte KaClock::getMinutes(unsigned long epochTime) {
	return ((epochTime % 3600) / 60);
}

byte KaClock::getSeconds() {
	return getSeconds(getEpochTime());
}

byte KaClock::getSeconds(unsigned long epochTime) {
	return (epochTime % 60);
}

int KaClock::getYear() {
	return getYear(getEpochTime());
}

int KaClock::getYear(unsigned long epochTime) {
	return year(epochTime);
}

byte KaClock::getMonth() {
	return getMonth(getEpochTime());
}

byte KaClock::getMonth(unsigned long epochTime) {
	//month from 1 to 12
	return month(epochTime);
}

byte KaClock::getDay() {
	return getDay(getEpochTime());
}

byte KaClock::getDay(unsigned long epochTime) {
	//day from 1 to 31
	return day(epochTime);
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

/*long KaClock::getDstOffset() {
	return dstOffset;
}*/

long KaClock::getRawOffset() {
	return rawOffset;
}

void KaClock::getMqttState(JsonObject& json, bool complete) {
	json["clockTime"] = getFormattedTime();
	json["validTime"] = isValidTime();
	json["timeZone"] = getTimeZone();
	json["lastNtpSync"] = (
					lastNtpSync > 0 ?
							getFormattedTime(
								ntpTime + rawOffset /*+ dstOffset*/
											+ (lastNtpSync / 1000)) :
										"n.a.");
	if (complete) {
		json["clockTimeRaw"] = getEpochTime();
		json["lastTimeZoneSync"] = (
				lastTimeZoneSync > 0 ?
						getFormattedTime(
								ntpTime + rawOffset /*+ dstOffset*/
										+ (lastTimeZoneSync / 1000)) :
									"n.a.");
		//json["dstOffset"] = getDstOffset();
		json["rawOffset"] = getRawOffset();
	}

}

