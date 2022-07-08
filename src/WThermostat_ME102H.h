#ifndef THERMOSTAT_ME102H_H
#define	THERMOSTAT_ME102H_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

const char* SENSOR_SELECTION_INTERNAL = "internal";
const char* SENSOR_SELECTION_FLOOR = "floor";
const char* SENSOR_SELECTION_BOTH = "both";

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
    //custom
    this->byteSensorSelection = 0x2b;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //schedules mode
    this->schedulesMode->clearEnums();
    this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_OFF);
    //sensorSelection
    this->sensorSelection = new WProperty("sensorSelection", "Sensor Selection", STRING, TYPE_THERMOSTAT_MODE_PROPERTY);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_INTERNAL);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_FLOOR);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_BOTH);
    this->sensorSelection->setVisibility(MQTT);
    this->sensorSelection->setOnChange(std::bind(&WThermostat_ME102H::sensorSelectionToMcu, this, std::placeholders::_1));
    this->addProperty(this->sensorSelection);
  }

protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);

		if (!knownCommand) {
      const char* newS;
      if (cByte == this->byteSensorSelection) {
        if (commandLength == 0x05) {
          //sensor selection -
          //internal: 55 aa 03 07 00 05 2b 04 00 01 00
          //floor:    55 aa 03 07 00 05 2b 04 00 01 01
          //both:     55 aa 03 07 00 05 2b 04 00 01 02
          newS = this->sensorSelection->getEnumString(receivedCommand[10]);
          if (newS != nullptr) {
            changed = ((changed) || (this->sensorSelection->setString(newS)));
            knownCommand = true;
          }
        }
      } else {
      //consume some unsupported commands
        switch (cByte) {
          case 0x17 :
            //Temperature Scale C /
            //MCU: 55 aa 03 07 00 05 17 04 00 01 00
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
          case 0x67 :
            //freeze / MCU: 55 aa 03 07 00 05 67 01 00 01 00
            knownCommand = true;
            break;
          case 0x68 :
            //  programming_mode - weekend (2 days off) / MCU: 55 aa 03 07 00 05 68 04 00 01 01
            knownCommand = true;
            break;
          case 0x2d :
            //unknown Wifi state? /
            //MCU: 55 aa 03 07 00 05 2d 05 00 01 00
            knownCommand = true;
            break;
          case 0x24 :
            //unknown Wifi state? / MCU: 55 aa 03 07 00 05 24 04 00 01 00
            knownCommand = true;
            break;
        }
      }
    }
		if (changed) {
			notifyState();
		}
	  return knownCommand;
  }

  void sensorSelectionToMcu(WProperty* property) {
    if (!isReceivingDataFromMcu()) {
      byte sm = property->getEnumIndex();
      if (sm != 0xFF) {
        //send to device
        //internal:    55 aa 03 07 00 05 2b 04 00 01 00
        //floor:       55 aa 03 07 00 05 2b 04 00 01 01
        //both:        55 aa 03 07 00 05 2b 04 00 01 02
        unsigned char cm[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
                               this->byteSensorSelection, 0x04, 0x00, 0x01, sm};
        commandCharsToSerial(11, cm);
      }
    }
  }

private :
  WProperty* sensorSelection;
  byte byteSensorSelection;

};

#endif
