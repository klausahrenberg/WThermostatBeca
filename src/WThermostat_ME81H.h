#ifndef THERMOSTAT_ME81H_H
#define	THERMOSTAT_ME81H_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"
#include "WThermostat_BAC_002_ALW.h"
#include "WThermostat_ME102H.h"

class WThermostat_ME81H : public WThermostat {
public :
  WThermostat_ME81H(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_ME81H created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x01;
    //actual temperature must be handled in processStatusCommand (byte 0x08)
    this->byteTemperatureActual = NOT_SUPPORTED;
    this->byteTemperatureTarget = 0x10;
    this->byteTemperatureFloor = 0x00;
    this->temperatureFactor = 1.0f;
    this->byteSchedulesMode = 0x02;
    this->byteLocked = 0x28;
    this->byteSchedules = 0x26;
    this->byteSchedulingPosHour = 0;
    this->byteSchedulingPosMinute = 1;
    this->byteSchedulingDays = 8;
    //custom
    this->byteSystemMode = 0x24;
    this->byteSensorSelection = 0x2b;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //systemMode
    this->systemMode = new WProperty("systemMode", "System Mode", STRING, TYPE_THERMOSTAT_MODE_PROPERTY);
    this->systemMode->addEnumString(SYSTEM_MODE_HEAT);
    this->systemMode->addEnumString(SYSTEM_MODE_COOL);
    this->systemMode->addEnumString(SYSTEM_MODE_FAN);
    this->systemMode->setOnChange(std::bind(&WThermostat_ME81H::systemModeToMcu, this, std::placeholders::_1));
    this->addProperty(systemMode);
    //sensorSelection
    this->sensorSelection = new WProperty("sensorSelection", "Sensor Selection", STRING, TYPE_THERMOSTAT_MODE_PROPERTY);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_INTERNAL);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_FLOOR);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_BOTH);
    this->sensorSelection->setVisibility(MQTT);
    this->sensorSelection->setOnChange(std::bind(&WThermostat_ME81H::sensorSelectionToMcu, this, std::placeholders::_1));
    this->addProperty(this->sensorSelection);
  }

protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);

		if (!knownCommand) {
      const char* newS;
      if (cByte == 0x18) {
        if (commandLength == 0x08) {
          //actual Temperature at this model has a different divider of 10
					unsigned long rawValue = WSettings::getUnsignedLong(receivedCommand[10], receivedCommand[11], receivedCommand[12], receivedCommand[13]);
				  float newValue = (float) rawValue / 10.0f;
    			changed = ((changed) || (!actualTemperature->equalsDouble(newValue)));
    			actualTemperature->setDouble(newValue);
    			knownCommand = true;
        }
      } else if (cByte == this->byteSystemMode) {
        if (commandLength == 0x05) {
          //MODEL_BAC_002_ALW - systemMode
          //cooling:     55 AA 00 06 00 05 66 04 00 01 00
          //heating:     55 AA 00 06 00 05 66 04 00 01 01
          //ventilation: 55 AA 00 06 00 05 66 04 00 01 02
          newS = systemMode->getEnumString(receivedCommand[10]);
          if (newS != nullptr) {
            changed = ((changed) || (systemMode->setString(newS)));
            knownCommand = true;
          }
        }
			} else if (cByte == this->byteSensorSelection) {
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
          /*case 0x66 :
            //Temperature Scale C /
            //MCU:  C / 55 aa 03 07 00 05 66 04 00 01 00
            //MCU:  F / 55 aa 03 07 00 05 66 04 00 01 01
            knownCommand = true;
            break;*/
          case 0x13 :
            //Temperature ceiling
            //MCU: 35C / 55 aa 03 07 00 08 13 02 00 04 00 00 00 23
            //MCU: 40C / 55 aa 03 07 00 08 13 02 00 04 00 00 00 28
            knownCommand = true;
            break;
          case 0x1a :
            //Lower limit of temperature
            //MCU:  5C / 55 aa 03 07 00 08 1a 02 00 04 00 00 00 05
            //MCU: 10C / 55 aa 03 07 00 08 1a 02 00 04 00 00 00 0a
            knownCommand = true;
            break;
          case 0x1b :
            //Temperature correction
            //MCU:  0C / 55 aa 03 07 00 08 1b 02 00 04 00 00 00 00
            //MCU: -2C / 55 aa 03 07 00 08 1b 02 00 04 ff ff ff fe
            knownCommand = true;
            break;
          case 0x0a :
            //freeze /
            //MCU: off / 55 aa 03 07 00 05 0a 01 00 01 00
            //MCU:  on / 55 aa 03 07 00 05 0a 01 00 01 01
            knownCommand = true;
            break;
          case 0x65 :
            //temp_differ_on - 1C /
            //MCU: 55 aa 03 07 00 08 65 02 00 04 00 00 00 01
            knownCommand = true;
            break;
          case 0x66 :
            //  programming_mode - weekend (2 days off) / MCU: 55 aa 03 07 00 05 68 04 00 01 01
            //01: 5+2 / 02: 6+1 / 03 7+0 day mode
            knownCommand = true;
            break;
          case 0x2d :
            //unknown Wifi state? / MCU: 55 aa 03 07 00 05 2d 05 00 01 00
            knownCommand = true;
            break;
          /*case 0x24 :
            //unknown Wifi state? / MCU: 55 aa 03 07 00 05 24 04 00 01 00
            knownCommand = true;
            break;*/
        }
      }
    }
		if (changed) {
			notifyState();
		}
	  return knownCommand;
  }

  void systemModeToMcu(WProperty* property) {
    if (!isReceivingDataFromMcu()) {
      byte sm = property->getEnumIndex();
      if (sm != 0xFF) {
        //send to device
        //cooling:     55 AA 00 06 00 05 66 04 00 01 00
        //heating:     55 AA 00 06 00 05 66 04 00 01 01
        //ventilation: 55 AA 00 06 00 05 66 04 00 01 02
        unsigned char cm[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
                                            this->byteSystemMode, 0x04, 0x00, 0x01, sm};
        commandCharsToSerial(11, cm);
      }
    }
  }

  void sensorSelectionToMcu(WProperty* property) {
    if (!isReceivingDataFromMcu()) {
      byte sm = property->getEnumIndex();
      if (sm != 0xFF) {
        //send to device
        //internal: 55 aa 03 07 00 05 2d 05 00 01 00
        //floor:    55 aa 03 07 00 05 2d 05 00 01 01
        //both:     55 aa 03 07 00 05 2d 05 00 01 02
        unsigned char cm[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
                               this->byteSensorSelection, 0x05, 0x00, 0x01, sm};
        commandCharsToSerial(11, cm);
      }
    }
  }

private :
  WProperty* systemMode;
  byte byteSystemMode;
  WProperty* sensorSelection;
  byte byteSensorSelection;
};

#endif
