#ifndef THERMOSTAT_HY02B05_H
#define	THERMOSTAT_HY02B05_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

class WThermostat_HY02B05 : public WThermostat {
public :
  WThermostat_HY02B05(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_HY02B05 created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x66;
    this->byteTemperatureActual = 0x03;
    this->byteTemperatureTarget = 0x02;
    this->byteTemperatureFloor = 0x67;
    this->temperatureFactor = 10.0f;
    this->byteSchedulesMode = 0x04;
    this->byteLocked = 0x06;
    this->byteSchedules = NOT_SUPPORTED;
    this->byteSchedulingPosHour = 1;
    this->byteSchedulingPosMinute = 0;
    this->byteSchedulingDays = 18;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //2021-01-14 - schedulesMode
    this->schedulesMode->clearEnums();
    this->schedulesMode->addEnumString(SCHEDULES_MODE_OFF);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLIDAY);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLD);
  }

protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);

		if (!knownCommand) {
      //const char* newS;
      /*if (cByte == this->byteXXX) {
        if (commandLength == 0xXX) {
          newS = systemMode->getEnumString(receivedCommand[10]);
          if (newS != nullptr) {
            changed = ((changed) || (systemMode->setString(newS)));
            knownCommand = true;
          }
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
