#ifndef THERMOSTAT_TEMPLATE_H
#define	THERMOSTAT_TEMPLATE_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

class WThermostat_TEMPLATE : public WThermostat {
public :
  WThermostat_TEMPLATE(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_TEMPLATE created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x01;
    this->byteTemperatureActual = NOT_SUPPORTED;
    this->byteTemperatureTarget = NOT_SUPPORTED;
    this->byteTemperatureFloor = NOT_SUPPORTED;
    this->temperatureFactor = 10.0f;
    this->byteSchedulesMode = NOT_SUPPORTED;
    this->byteLocked = NOT_SUPPORTED;
    this->byteSchedules = NOT_SUPPORTED;
    this->byteSchedulingPosHour = 0;
    this->byteSchedulingPosMinute = 1;
    this->byteSchedulingDays = 8;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
  }

protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);

		if (!knownCommand) {
      //const char* newS;
      if (cByte == this->byteXXX) {
        if (commandLength == 0xXX) {
          /*newS = systemMode->getEnumString(receivedCommand[10]);
          if (newS != nullptr) {
            changed = ((changed) || (systemMode->setString(newS)));
            knownCommand = true;
          }*/
        }
			}
    }
		if (changed) {
			notifyState();
		}
	  return knownCommand;
  }

private :

};

#endif
