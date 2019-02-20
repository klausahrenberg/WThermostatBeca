#include "KaNetwork.h"
#include "BecaMcu.h"
#include "KaClock.h"

#define APPLICATION "Thermostat Beca-Wifi"
#define VERSION "0.6"
#define DEBUG false

KaNetwork *network;
BecaMcu *becaMcu;
KaClock *kClock;

void setup() {
	Serial.begin(9600);
	//Wifi and Mqtt connection
	network = new KaNetwork(APPLICATION, VERSION, DEBUG, false);
	network->setOnNotify([]() {
		switch (network->getNetworkState()) {
			case NETWORK_CONNECTED :
			becaMcu->queryState();
			break;
			case NETWORK_NOT_CONNECTED :
			break;
		}
	});
	network->setOnConfigurationFinished([]() {
		//Switch blinking thermostat in normal operating mode back
		becaMcu->cancelConfiguration();
	});
	network->onCallbackMqtt(onMqttCallback);
	//KaClock - time sync
	kClock = new KaClock(DEBUG);
	kClock->setOnTimeUpdate([]() {
		becaMcu->sendActualTimeToBeca();
	});
	//Communication between ESP and Beca-Mcu
	becaMcu = new BecaMcu(kClock);
	//becaMcu->cancelConfiguration();
	becaMcu->setOnNotify([]() {
		StaticJsonBuffer<MQTT_MAX_PACKET_SIZE> jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		becaMcu->getMqttState(json);
		kClock->getMqttState(json);
		json["firmware"] = VERSION;
		return network->publishMqtt("state", json);
	});
	becaMcu->setOnUnknownCommand([]() {
		StaticJsonBuffer< MQTT_MAX_PACKET_SIZE> jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		json["unknown"] = becaMcu->getCommandAsString();
		return network->publishMqtt("mcucommand", json);
	});
	becaMcu->setOnConfigurationRequest([]() {
		network->startConfiguration();
		return true;
	});
}

void loop() {
	if (network->loop(false)) {
		becaMcu->loop();
		kClock->loop();
	}
	yield();
}

void onMqttCallback(String topic, String payload) {
	if (topic.equals("desiredTemperature")) {
		becaMcu->setDesiredTemperature(payload.toFloat());
	} else if (topic.equals("deviceOn")) {
		becaMcu->setDeviceOn(payload.equals("true"));
	} else if (topic.equals("mcucommand")) {
		becaMcu->commandHexStrToSerial(payload);
	}
}
