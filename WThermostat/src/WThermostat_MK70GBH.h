#ifndef THERMOSTAT_MK70GBH_H
#define	THERMOSTAT_MK70GBH_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

class WThermostat_MK70GBH : public WThermostat {
public :
  WThermostat_MK70GBH(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_MK70GBH created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x01;
    this->byteTemperatureActual = 0x03;
    this->byteTemperatureTarget = 0x02;
    this->byteTemperatureFloor = NOT_SUPPORTED;
    this->temperatureFactor = 10.0f;
    this->byteSchedulesMode = 0x04;
    this->byteLocked = 0x08;
    this->byteSchedules = 0x2b;
    this->byteSchedulingPosHour = 0;
    this->byteSchedulingPosMinute = 1;
    this->byteSchedulingDays = 8;
    //custom parameters
    this->byteStatusMode = 0x05;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //schedulesMode
    this->schedulesMode->clearEnums();
    this->schedulesMode->addEnumString(SCHEDULES_MODE_OFF);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLD);
    //statusMode
    this->statusMode = new WProperty("statusMode", "Status", STRING, TYPE_HEATING_COOLING_PROPERTY);
    this->statusMode->addEnumString(STATE_OFF);
    this->statusMode->addEnumString(STATE_HEATING);
    this->statusMode->setReadOnly(true);
    this->statusMode->setVisibility(MQTT);
    this->addProperty(statusMode);
  }

protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);

		if (!knownCommand) {
      const char* newS;
      if (cByte == byteStatusMode) {
				if (commandLength == 0x05) {
				  //status
				  newS = statusMode->getEnumString(receivedCommand[10]);
				  if (newS != nullptr) {
					  changed = ((changed) || (statusMode->setString(newS)));
					  knownCommand = true;
				  }
				}
			}
    }
		if (changed) {
			notifyState();
		}
	  return knownCommand;
  }

  virtual bool processStatusSchedules(byte commandLength) {
    bool result = (commandLength == 0x24);
    if (result) {
      bool changed = false;
      //schedules for model MK70GB-H
      int res = 1;
      int ii = 0;
      for (int i = 0; i < 32; i++) {
      byte newByte = receivedCommand[i + 10];
      if (i != 2) {
        if (i > 2)
          res = (i+2) % 4;
          if (res != 0) {
            changed = ((changed) || (newByte != schedules[ii]));
            schedules[ii] = newByte;
            ii++;
          }
        }
      }
      if (changed) {
        notifySchedules();
      }
    }
    return result;
  }

  virtual void schedulesToMcu() {
    if (receivedSchedules()) {
			int daysToSend = this->byteSchedulingDays;
			int functionLengthInt = (daysToSend * 3);
			char functionL = 0x20;
			char dataL = 0x24;
			unsigned char scheduleCommand[functionLengthInt+10];
			scheduleCommand[0] = 0x55;
			scheduleCommand[1] = 0xaa;
			scheduleCommand[2] = 0x00;
			scheduleCommand[3] = 0x06;
			scheduleCommand[4] = 0x00;
			scheduleCommand[5] = dataL; //0x3a; // dataLength
			scheduleCommand[6] = byteSchedules;
			scheduleCommand[7] = 0x00;
			scheduleCommand[8] = 0x00;
			scheduleCommand[9] = functionL;

			int res = 1;
			functionLengthInt = functionLengthInt + 8;
			int ii = 0;
			for (int i = 0; i <functionLengthInt; i++) {
				if (i == 2) {
					scheduleCommand[i + 10] = 0x00;
				} else if (i > 2) {
					res = (i+2) % 4;
					if (res != 0) {
						scheduleCommand[i + 10] = schedules[ii];
						ii++;
					} else {
						scheduleCommand[i + 10] = 0x00;
					}
				} else {
					scheduleCommand[i + 10] = schedules[ii];
					ii++;
        }
    	}

			commandCharsToSerial(functionLengthInt+10, scheduleCommand);
			//notify change
			this->notifySchedules();
		}
  }

private :
  WProperty* statusMode;
  byte byteStatusMode;

};

#endif
