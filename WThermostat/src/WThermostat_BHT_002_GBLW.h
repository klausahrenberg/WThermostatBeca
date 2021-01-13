#ifndef THERMOSTAT_BHT_002_GBLW_H
#define	THERMOSTAT_BHT_002_GBLW_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

class WThermostat_BHT_002_GBLW : public WThermostat {
public :
  WThermostat_BHT_002_GBLW(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_BHT_002_GBLW created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x01;
    this->byteTemperatureActual = 0x03;
    this->byteTemperatureTarget = 0x02;
    this->byteTemperatureFloor = 0x66;
    this->temperatureFactor = 2.0f;
    this->byteSchedulesMode = 0x04;
    this->byteLocked = 0x06;
    this->byteSchedules = 0x65;
    this->byteSchedulingPosHour = 1;
    this->byteSchedulingPosMinute = 0;
    this->byteSchedulingDays = 18;
    //custom
    this->byteEcoMode = 0x05;
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
      if (cByte == 0x68) {
				if (receivedCommand[5] == 0x05) {
					//Unknown permanently sent from MCU
					//55 aa 01 07 00 05 68 01 00 01 01
					knownCommand = true;
				}
			} else if (cByte == this->byteEcoMode) {
        if (commandLength == 0x05) {
          //ecoMode -> ignore
          knownCommand = true;
        }
      }
    }
		if (changed) {
			notifyState();
		}
	  return knownCommand;
  }

  void ecoModeToMcu(WProperty* property) {
   	if (!isReceivingDataFromMcu()) {
    	//55 AA 00 06 00 05 05 01 00 01 01
    	byte dt = 0x00; //(this->ecoMode->getBoolean() ? 0x01 : 0x00);
    	unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
       		                                this->byteEcoMode, 0x01, 0x00, 0x01, dt};
    	commandCharsToSerial(11, deviceOnCommand);
  	}
  }

private :
  byte byteEcoMode;
};

#endif
