#ifndef THERMOSTAT_ME102H_H
#define	THERMOSTAT_ME102H_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

class WThermostat_ME102H : public WThermostat {
public :
  WThermostat_ME102H(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_ME102H created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x01;
    this->byteTemperatureActual = 0x18;
    this->byteTemperatureTarget = 0x10;
    this->byteTemperatureFloor = 0x65;
    this->temperatureFactor = 1.0f;
    this->byteSchedulesMode = 0x02;
    this->byteLocked = 0x28;
    this->byteSchedules = 0x6c;
    this->byteSchedulingPosHour = 0;
    this->byteSchedulingPosMinute = 1;
    this->byteSchedulingDays = 8;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //schedules mode
    this->schedulesMode->clearEnums();
    this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_OFF);
  }

protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);

		if (!knownCommand) {
      //const char* newS;
      //consume some unsupported commands
      switch (cByte) {
        case 0x17 :
          //Temperature Scale C / MCU: 55 aa 03 07 00 05 17 04 00 01 00
          knownCommand = true;
          break;
        case 0x13 :
          //Temperature ceiling - 35C / MCU: 55 aa 03 07 00 08 13 02 00 04 00 00 00 23
          knownCommand = true;
          break;
        case 0x1a :
          //Lower limit of temperature - 5C / MCU: 55 aa 03 07 00 08 1a 02 00 04 00 00 00 05
          knownCommand = true;
          break;
        case 0x6a :
          //temp_differ_on - 1C / MCU: 55 aa 03 07 00 08 6a 02 00 04 00 00 00 01
          knownCommand = true;
          break;
        case 0x1b :
          //Temperature correction - 0C / MCU: 55 aa 03 07 00 08 1b 02 00 04 00 00 00 00
          knownCommand = true;
          break;
        case 0x2b :
          //sensor selection - in / MCU: 55 aa 03 07 00 05 2b 04 00 01 00
          knownCommand = true;
          break;
        case 0x67 :
          //freeze / MCU: 55 aa 03 07 00 05 67 01 00 01 00
          knownCommand = true;
          break;
        case 0x68 :
          //programming_mode - weekend (2 days off) / MCU: 55 aa 03 07 00 05 68 04 00 01 01
          knownCommand = true;
          break;
        case 0x2d :
          //unknown Wifi state? / MCU: 55 aa 03 07 00 05 2d 05 00 01 00
          knownCommand = true;
          break;
        case 0x24 :
          //unknown Wifi state? / MCU: 55 aa 03 07 00 05 24 04 00 01 00
          knownCommand = true;
          break;
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
