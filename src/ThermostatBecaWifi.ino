#include "KaNetwork.h"
#include "BecaMcu.h"
#include "KaClock.h"

#define APPLICATION "Thermostat Beca-Wifi"
#define VERSION "0.97"
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
			if (becaMcu->isDeviceStateComplete()) {
				sendMqttStatus();			
			}
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
	becaMcu = new BecaMcu(kClock);
	becaMcu->setOnNotify([]() {
		//send state of device
		return sendMqttStatus();
	});
	becaMcu->setOnSchedulesChange([]() {
		//Send schedules once at ESP start and at every change
		return sendSchedulesViaMqtt();
	});
	becaMcu->setOnNotifyCommand([](String commandType) {
		StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		json[commandType] = becaMcu->getCommandAsString();
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

bool sendMqttActualTemperatur() {
	return network->publishMqtt("actualTemperature", String(becaMcu->getActualTemperature()));
}

bool sendMqttActualFloorTemperatur() {
	return network->publishMqtt("actualFloorTemperature", String(becaMcu->getActualFloorTemperature()));
}

bool sendMqttStatus() {
	sendMqttActualTemperatur();
	sendMqttActualFloorTemperatur();
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	becaMcu->getMqttState(json);
	kClock->getMqttState(json, false);
	json["firmware"] = VERSION;
	json["ip"] = network->getDeviceIpAddress();
	json["webServerRunning"] = network->isWebServerRunning();
	return network->publishMqtt("state", json);
}

bool sendClockStateViaMqttStatus() {
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	kClock->getMqttState(json, true);
	return network->publishMqtt("clock", json);
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
	topic.toLowerCase();
	if (topic.equals("desiredtemperature")) {
		becaMcu->setDesiredTemperature(payload.toFloat());
	} else if (topic.equals("deviceon")) {
		becaMcu->setDeviceOn(payload.equals("true"));
	} else if (topic.equals("manualmode")) {
		becaMcu->setManualMode(payload.equals("true"));
	} else if (topic.equals("ecomode")) {
		becaMcu->setEcoMode(payload.equals("true"));
	} else if (topic.equals("locked")) {
		becaMcu->setLocked(payload.equals("true"));
	} else if (topic.equals("fanspeed")) {
		//MODEL_BAC_002_ALW
		becaMcu->setFanSpeedFromString(payload);
	} else if (topic.equals("systemmode")) {
		//MODEL_BAC_002_ALW
		becaMcu->setSystemModeFromString(payload);
	} else if (topic.equals("schedulesdayoffset")) {
		becaMcu->setSchedulesDayOffset(payload.toInt());
	} else if (topic.equals("logmcu")) {
		becaMcu->setLogMcu(payload.equals("true"));
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
		sendMqttActualTemperatur();
	} else if ((topic.equals("clock")) && (payload.equals("0"))) {
		sendClockStateViaMqttStatus();
	} else if ((topic.equals("webserver")) || (topic.equals("webservice"))) {
		if (payload.equals("true")) {
			network->startWebServer();
		} else {
			network->stopWebServer();
		}
	}
}
