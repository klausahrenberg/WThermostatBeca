#include <Arduino.h>
#include "WNetwork.h"
#include "WBecaDevice.h"
#include "WClock.h"

#define APPLICATION "Thermostat"
#define VERSION "1.18"
#define FLAG_SETTINGS 0x18
#define DEBUG false

WNetwork* network;
WBecaDevice* becaDevice;
WClock* wClock;

void setup() {
	Serial.begin(9600);
	//Wifi and Mqtt connection
	network = new WNetwork(DEBUG, APPLICATION, VERSION, true, NO_LED, FLAG_SETTINGS);
	network->setOnNotify([]() {
		if (network->isWifiConnected()) {
			//nothing to do
		}
		if (network->isMqttConnected()) {
			becaDevice->queryState();
			if (becaDevice->isDeviceStateComplete()) {
				//nothing to do;
			}
		}
	});
	network->setOnConfigurationFinished([]() {
		//Switch blinking thermostat in normal operating mode back
		becaDevice->cancelConfiguration();
	});
	//WClock - time sync
	wClock = new WClock(network, false);
	network->addDevice(wClock);
	//Communication between ESP and Beca-Mcu
	becaDevice = new WBecaDevice(network, wClock);
	network->addDevice(becaDevice);
	becaDevice->setOnConfigurationRequest([]() {
		network->startWebServer();
		return true;
	});
}

void loop() {
	network->loop(millis());
	delay(50);
}
