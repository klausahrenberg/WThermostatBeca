#include <Arduino.h>
#include "../../WAdapter/Wadapter/WNetwork.h"
#include "WBecaDevice.h"
#include "WClock.h"

#define APPLICATION "Thermostat Beca"
#define VERSION "0.75"
#define DEBUG false
#define NTP_SERVER "de.pool.ntp.org"

WNetwork* network;
WBecaDevice* becaDevice;
WClock* wClock;

void setup() {
	Serial.begin(9600);
	//Wifi and Mqtt connection
	network = new WNetwork(DEBUG, APPLICATION, VERSION, true, NO_LED, 1100);
	network->setOnNotify([]() {
		if (network->isWifiConnected()) {

		}
		if (network->isMqttConnected()) {
			becaDevice->queryState();
			if (becaDevice->isDeviceStateComplete()) {
				//sendMqttStatus();
			}
		}
	});
	network->setOnConfigurationFinished([]() {
		//Switch blinking thermostat in normal operating mode back
		becaDevice->cancelConfiguration();
	});
	//KaClock - time sync
	wClock = new WClock(DEBUG, network, NTP_SERVER);
	wClock->setOnTimeUpdate([]() {
		becaDevice->sendActualTimeToBeca();
	});
	wClock->setOnError([](String error) {
		DynamicJsonDocument* jsonDocument = network->getJsonDocument();
		JsonObject json = jsonDocument->to<JsonObject>();
		json["message"] = error;
		return network->publishMqtt("error", json);
	});
	//Communication between ESP and Beca-Mcu
	becaDevice = new WBecaDevice(DEBUG, APPLICATION, network->getSettings(), wClock);
	network->addDevice(becaDevice);

	becaDevice->setOnSchedulesChange([]() {
		//Send schedules once at ESP start and at every change
		return true;// sendSchedulesViaMqtt();
	});
	becaDevice->setOnNotifyCommand([](String commandType) {
		DynamicJsonDocument* jsonDocument = network->getJsonDocument();
		JsonObject json = jsonDocument->to<JsonObject>();
		json[commandType] = becaDevice->getCommandAsString();
		return network->publishMqtt("mcucommand", json);
	});
	becaDevice->setOnConfigurationRequest([]() {
		network->startWebServer();
		return true;
	});
}

void loop() {
	unsigned long now = millis();
	wClock->loop();
	network->loop(now);
	delay(50);
}

/*bool sendClockStateViaMqttStatus() {
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	kClock->getMqttState(json, true);
	return network->publishMqtt("clock", json);
}*/

/**
 * Sends the schedule in 3 messages because of maximum message length
 */
/*
bool sendSchedulesViaMqtt() {
	boolean result = true;
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	becaDevice->getMqttSchedules(json, SCHEDULE_WORKDAY);
	result = result && network->publishMqtt("schedules", json);
	jsonBuffer.clear();
	JsonObject& json2 = jsonBuffer.createObject();
	becaDevice->getMqttSchedules(json2, SCHEDULE_SATURDAY);
	result = result && network->publishMqtt("schedules", json2);
	jsonBuffer.clear();
	JsonObject& json3 = jsonBuffer.createObject();
	becaDevice->getMqttSchedules(json3, SCHEDULE_SUNDAY);
	result = result && network->publishMqtt("schedules", json3);
	jsonBuffer.clear();
	return result;
}*/
