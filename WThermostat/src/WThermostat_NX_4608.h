#ifndef THERMOSTAT_NX_4608_H
#define	THERMOSTAT_NX_4608_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"
#include "WThermostat_ME102H.h"

class WThermostat_NX_4608 : public WThermostat {
public :
  WThermostat_NX_4608(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_NX_4608 created"));
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
    //custom
    this->byteSensorSelection = 0x74;
    this->byteDisplayOn = 0x01;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //2021-01-14 - schedulesMode
    this->schedulesMode->clearEnums();
    this->schedulesMode->addEnumString(SCHEDULES_MODE_OFF);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLIDAY);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLD);
    //sensorSelection
    this->sensorSelection = new WProperty("sensorSelection", "Sensor Selection", STRING, TYPE_THERMOSTAT_MODE_PROPERTY);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_INTERNAL);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_FLOOR);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_BOTH);
    this->sensorSelection->setVisibility(MQTT);
    this->sensorSelection->setOnChange(std::bind(&WThermostat_NX_4608::sensorSelectionToMcu, this, std::placeholders::_1));
    this->addProperty(this->sensorSelection);
    //display
    this->displayOn = WProperty::createOnOffProperty("displayOn", "Display");
    this->displayOn->setVisibility(MQTT);
    this->addProperty(displayOn);
  }

protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);

		if (!knownCommand) {
      const char* newS;
      bool newB;
      if (cByte == this->byteSensorSelection) {
        if (commandLength == 0x05) {
          //sensor selection -
          //internal: 55 aa 03 07 00 05 74 04 00 01 00
          //floor:    55 aa 03 07 00 05 74 04 00 01 01
          //both:     55 aa 03 07 00 05 74 04 00 01 02
          newS = this->sensorSelection->getEnumString(receivedCommand[10]);
          if (newS != nullptr) {
            changed = ((changed) || (this->sensorSelection->setString(newS)));
            knownCommand = true;
          }
        }
      }
      else if (cByte == this->byteDisplayOn) {
        if (commandLength == 0x05){
          //display on
          // on:  55 aa 03 07 00 05 01 01 00 01 01
          // off: 55 aa 03 07 00 05 01 01 00 01 00
          newB = (receivedCommand[10] == 0x01);
          changed = ((changed) || (newB != this->displayOn->getBoolean()));
          displayOn->setBoolean(newB);
          knownCommand = true;
        }
      } else{
      //consume some unsupported commands
        switch (cByte) {
          case 0x0c :
            // unknown bitmap
            // MCU: 55 aa 03 07 00 05 0c 05 00 01 00
            // MCU: 55 aa 03 07 00 05 0c 05 00 01 08
            knownCommand = true;
            break;
          case 0x65 :
            // unknown boolean
            // MCU: 55 aa 03 07 00 05 65 01 00 01 00
            knownCommand = true;
            break;
          case 0x68 :
            // unknown integer
            // MCU: 55 aa 03 07 00 08 68 02 00 04 00 00 00 0a
            knownCommand = true;
            break;
          case 0x69 :
            // unknown integer
            // MCU: 55 aa 03 07 00 08 69 02 00 04 00 00 00 0f
            knownCommand = true;
            break;
          case 0x6a :
            // disable high temp protection for external sensor (AA Code 2)
            // MCU: off / 55 aa 03 07 00 05 6a 01 00 01 00
            // MCU: on  / 55 aa 03 07 00 05 6a 01 00 01 01
            knownCommand = true;
            break;
          case 0x6b :
            // disable low temp protection for external sensor (A9 Code 2)
            // MCU: off / 55 aa 03 07 00 05 6b 01 00 01 00
            // MCU: on  / 55 aa 03 07 00 05 6b 01 00 01 01
            knownCommand = true;
            break;
          case 0x6c :
            // unknown boolean
            // MCU: 55 aa 03 07 00 05 6c 01 00 01 00
            knownCommand = true;
            break;
          case 0x6d :
            // Temperature correction in millidegree (A1)
            // MCU:  0C / 55 aa 03 07 00 08 6d 02 00 04 00 00 00 00
            // MCU: -2C / 55 aa 03 07 00 08 6d 02 00 04 ff ff ff ec
            knownCommand = true;
            break;
          case 0x6e :
            // hysteresis in millidegree (A2)
            // MCU: 0.5C / 55 aa 03 07 00 08 6e 02 00 04 00 00 00 05
            // MCU: 1C   / 55 aa 03 07 00 08 6e 02 00 04 00 00 00 0a
            // MCU: 1.5C / 55 aa 03 07 00 08 6e 02 00 04 00 00 00 0f
            // MCU: 2C   / 55 aa 03 07 00 08 6e 02 00 04 00 00 00 14
            // MCU: 2.5C / 55 aa 03 07 00 08 6e 02 00 04 00 00 00 19
            knownCommand = true;
            break;
          case 0x6f :
            // hysteresis in degree for external sensor (AB)
            // MCU: 1C / 55 aa 03 07 00 08 6f 02 00 04 00 00 00 01
            // MCU: 2C / 55 aa 03 07 00 08 6f 02 00 04 00 00 00 02
            // ...
            // MCU: 9C / 55 aa 03 07 00 08 6f 02 00 04 00 00 00 09
            knownCommand = true;
            break;
          case 0x70 :
            // max temp external sensor (AA Code 1)
            // MCU: 35C / 55 aa 03 07 00 08 70 02 00 04 00 00 00 23
            // ...
            // MCU: 70C / 55 aa 03 07 00 08 70 02 00 04 00 00 00 46 
          case 0x71 :
            // min temp external sensor (A9 Code 1)
            // MCU:  1C / 55 aa 03 07 00 08 71 02 00 04 00 00 00 01
            // MCU:  5C / 55 aa 03 07 00 08 71 02 00 04 00 00 00 05
            // MCU: 10C / 55 aa 03 07 00 08 71 02 00 04 00 00 00 0a
            knownCommand = true;
            break;
          case 0x72 :
            // max temp (A8)
            // MCU: 20C / 55 aa 03 07 00 08 72 02 00 04 00 00 00 14
            // MCU: 21C / 55 aa 03 07 00 08 72 02 00 04 00 00 00 15
            // ...
            // MCU: 35C / 55 aa 03 07 00 08 72 02 00 04 00 00 00 23
            // ...
            // MCU: 70C / 55 aa 03 07 00 08 72 02 00 04 00 00 00 46
            knownCommand = true;
            break;
          case 0x73 :
            // min temp (A7)
            // MCU: 1C  / 55 aa 03 07 00 08 73 02 00 04 00 00 00 01
            // MCU: 2C  / 55 aa 03 07 00 08 73 02 00 04 00 00 00 02
            // MCU: 3C  / 55 aa 03 07 00 08 73 02 00 04 00 00 00 03
            // ...
            // MCU: 9C  / 55 aa 03 07 00 08 73 02 00 04 00 00 00 09
            // MCU: 10C / 55 aa 03 07 00 08 73 02 00 04 00 00 00 0a
            knownCommand = true;
            break;
          case 0x75 :
            // poweron state (A4)
            // saved: 55 aa 03 07 00 05 75 04 00 01 00
            // off: 55 aa 03 07 00 05 75 04 00 01 01
            // on: 55 aa 03 07 00 05 75 04 00 01 02
            knownCommand = true;
            break;
          case 0x76 :
            // weekend mode (A6)
            // MCU: 5+2 / 55 aa 03 07 00 05 76 04 00 01 00
            // MCU: 6+1 / 55 aa 03 07 00 05 76 04 00 01 01
            // MCU: 7   / 55 aa 03 07 00 05 76 04 00 01 02
            knownCommand = true;
            break;
          case 0x77 :
            // unknown raw
            // MCU: 55 aa 03 07 00 0d 77 00 00 09 06 00 14 08 00 0f 0b 1e 0f
            knownCommand = true;
            break;
          case 0x78 :
            // unknown raw
            // MCU: 55 aa 03 07 00 0d 78 00 00 09 0d 1e 0f 11 00 0f 96 00 0f
            knownCommand = true;
            break;
          case 0x79 :
            // unknown raw
            // MCU: 55 aa 03 07 00 0d 79 00 00 09 06 00 14 08 00 0f 0b 1e 0f
            knownCommand = true;
            break;
          case 0x7A :
            // unknown raw
            // MCU: 55 aa 03 07 00 0d 7a 00 00 09 0d 1e 0f 11 00 0f 16 00 0f
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
        //internal: 55 aa 03 06 00 05 74 05 00 01 00
        //floor:    55 aa 03 06 00 05 74 05 00 01 01
        //both:     55 aa 03 06 00 05 74 05 00 01 02
        unsigned char cm[] = { 0x55, 0xAA, 0x03, 0x06, 0x00, 0x05,
                               this->byteSensorSelection, 0x05, 0x00, 0x01, sm};
        commandCharsToSerial(11, cm);
      }
    }
  }

private :
  WProperty* sensorSelection;
  byte byteSensorSelection;
  WProperty* displayOn;
  byte byteDisplayOn;
};

#endif
