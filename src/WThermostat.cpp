#include <Arduino.h>
#include "WNetwork.h"
#include "WClock.h"
#include "WThermostat.h"
#include "WThermostat_BHT_002_GBLW.h"
#include "WThermostat_BAC_002_ALW.h"
#include "WThermostat_ET81W.h"
#include "WThermostat_HY08WE.h"
#include "WThermostat_ME81H.h"
#include "WThermostat_MK70GBH.h"
#include "WThermostat_ME102H.h"
#include "WThermostat_CalypsoW.h"
#include "WThermostat_DLX_LH01.h"

#define APPLICATION "Thermostat"
#define VERSION "1.25beta"
#define FLAG_SETTINGS 0x22
#define DEBUG false

WNetwork* network;
WProperty* thermostatModel;
WThermostat* device;
WClock* wClock;

void setup() {
	Serial.begin(9600);
	//Wifi and Mqtt connection
	network = new WNetwork(DEBUG, APPLICATION, VERSION, NO_LED, FLAG_SETTINGS, nullptr);
	network->setOnConfigurationFinished([]() {
		//Switch blinking thermostat in normal operating mode back
		device->cancelConfiguration();
	});
	//WClock - time sync
	wClock = new WClock(network, false);
	network->addDevice(wClock);
	//Model
	thermostatModel = network->getSettings()->setByte("thermostatModel", MODEL_BHT_002_GBLW);
	//Thermostat device
	device = nullptr;
	switch (thermostatModel->getByte()) {
		case MODEL_BHT_002_GBLW :
			device = new WThermostat_BHT_002_GBLW(network, thermostatModel, wClock);
			break;
		case MODEL_BAC_002_ALW :
			device = new WThermostat_BAC_002_ALW(network, thermostatModel, wClock);
			break;
		case MODEL_ET81W :
			device = new WThermostat_ET81W(network, thermostatModel, wClock);
			break;
		case MODEL_HY08WE :
			device = new WThermostat_HY08WE(network, thermostatModel, wClock);
			break;
		case MODEL_ME81H :
		  device = new WThermostat_ME81H(network, thermostatModel, wClock);
			break;
		case MODEL_MK70GBH :
		  device = new WThermostat_MK70GBH(network, thermostatModel, wClock);
			break;
		case MODEL_ME102H :
			device = new WThermostat_ME102H(network, thermostatModel, wClock);
			break;
		case MODEL_CALYPSOW :
			device = new WThermostat_CalypsoW(network, thermostatModel, wClock);
			break;
		case MODEL_DLX_LH01 :
			device = new WThermostat_DLX_LH01(network, thermostatModel, wClock);
			break;
		default :
		  network->error(F("Can't start device. Wrong thermostatModel (%d)"), thermostatModel->getByte());
	}
	if (device != nullptr) {
		device->configureCommandBytes();
		device->initializeProperties();
	}
	network->addDevice(device);
}

void loop() {
	network->loop(millis());
	delay(50);
}
