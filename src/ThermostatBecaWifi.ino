#include "KaNetwork.h"
#include "BecaMcu.h"
#include "KaClock.h"

#define APPLICATION "Thermostat Beca-Wifi"
#define VERSION "0.7"
#define DEBUG false
#define JSON_BUFFER_SIZE 768
#define NTP_SERVER "de.pool.ntp.org"

KaNetwork *network;
BecaMcu *becaMcu;
KaClock *kClock;

void setup() {
	Serial.begin(9600);
	//Wifi and Mqtt connection
	network = new KaNetwork(APPLICATION, VERSION, DEBUG, false);
	network->setOnNotify([]() {
		if (network->isWifiConnected()) {

		}
		if (network->isMqttConnected()) {
			becaMcu->queryState();
		}
	});
	network->setOnConfigurationFinished([]() {
		//Switch blinking thermostat in normal operating mode back
		becaMcu->cancelConfiguration();
	});
	network->onCallbackMqtt(onMqttCallback);
	//KaClock - time sync
	kClock = new KaClock(DEBUG, NTP_SERVER);
	kClock->setOnTimeUpdate([]() {
		becaMcu->sendActualTimeToBeca();
	});
	kClock->setOnError([](String error) {
		StaticJsonBuffer< JSON_BUFFER_SIZE> jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		json["message"] = error;
		return network->publishMqtt("error", json);
	});
	//Communication between ESP and Beca-Mcu
	becaMcu = new BecaMcu(DEBUG, kClock);
	becaMcu->setOnNotify([]() {
		//send state of device
		return sendMqttStatus();
	});
	becaMcu->setOnSchedulesChange([]() {
		//Send schedules once at ESP start and at every change
		return sendSchedulesViaMqtt();
	});
	becaMcu->setOnUnknownCommand([]() {
		StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		json["unknown"] = becaMcu->getCommandAsString();
		return network->publishMqtt("mcucommand", json);
	});
	becaMcu->setOnConfigurationRequest([]() {
		network->startWebServer();
		return true;
	});
}

void loop() {
	if (network->loop(false)) {
		becaMcu->loop();
		kClock->loop();
	}
	delay(50);
}

bool sendMqttStatus() {
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	becaMcu->getMqttState(json);
	kClock->getMqttState(json);
	json["firmware"] = VERSION;
	json["ip"] = network->getDeviceIpAddress();
	json["webServerRunning"] = network->isWebServerRunning();
	return network->publishMqtt("state", json);
}

/**
 * Sends the schedule in 3 messages because of maximum message length
 */

bool sendSchedulesViaMqtt() {
	boolean result = true;
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	becaMcu->getMqttSchedules(json, SCHEDULE_WORKDAY);
	result = result && network->publishMqtt("schedules", json);
	jsonBuffer.clear();
	JsonObject& json2 = jsonBuffer.createObject();
	becaMcu->getMqttSchedules(json2, SCHEDULE_SATURDAY);
	result = result && network->publishMqtt("schedules", json2);
	jsonBuffer.clear();
	JsonObject& json3 = jsonBuffer.createObject();
	becaMcu->getMqttSchedules(json3, SCHEDULE_SUNDAY);
	result = result && network->publishMqtt("schedules", json3);
	jsonBuffer.clear();
	return result;
}

void onMqttCallback(String topic, String payload) {
	if (topic.equals("desiredTemperature")) {
		becaMcu->setDesiredTemperature(payload.toFloat());
	} else if (topic.equals("deviceOn")) {
		becaMcu->setDeviceOn(payload.equals("true"));
	} else if (topic.equals("manualMode")) {
		becaMcu->setManualMode(payload.equals("true"));
	} else if (topic.equals("ecoMode")) {
		becaMcu->setEcoMode(payload.equals("true"));
	} else if (topic.equals("locked")) {
		becaMcu->setLocked(payload.equals("true"));
	} else if (topic.equals("schedules")) {
		if (payload.equals("0")) {
			//Schedules request
			sendSchedulesViaMqtt();
		} else {
			becaMcu->setSchedules(payload);
		}
	} else if (topic.equals("mcucommand")) {
		becaMcu->commandHexStrToSerial(payload);
	} else if ((topic.equals("state")) && (payload.equals("0"))) {
			sendMqttStatus();
	} else if (topic.equals("webServer")) {
		if (payload.equals("true")) {
			network->startWebServer();
		} else {
			network->stopWebServer();
		}
	}
}
