#ifndef THERMOSTAT_DLX_LH01_H
#define	THERMOSTAT_DLX_LH01_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

const char* SCHEDULES_MODE_ECO = "eco";

class WThermostat_DLX_LH01 : public WThermostat {
public :
  WThermostat_DLX_LH01(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_DLX_LH01 created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x01;
    this->byteTemperatureActual = 0x03;
    this->byteTemperatureTarget = 0x02;
    this->byteTemperatureFloor = NOT_SUPPORTED;
    this->temperatureFactor = 1.0f;
    this->byteSchedulesMode = 0x04;
    this->byteLocked = 0x07;
    this->byteSchedules = NOT_SUPPORTED;
    this->byteSchedulingPosHour = 1;
    this->byteSchedulingPosMinute = 0;
    this->byteSchedulingDays = 18;
    //custom
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //schedulesMode
    this->schedulesMode->clearEnums();
    this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_OFF);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_ECO);
  }

protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);

		if (!knownCommand) {
      /*if (cByte == this->byteEcoMode) {
        if (commandLength == 0x05) {
          //ecoMode -> ignore
          knownCommand = true;
        }
      }*/
    }
		if (changed) {
			notifyState();
		}
	  return knownCommand;
  }

private :

};

#endif
