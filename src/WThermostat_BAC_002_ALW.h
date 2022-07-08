#ifndef THERMOSTAT_BAC_002_ALW_H
#define	THERMOSTAT_BAC_002_ALW_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

const char* SYSTEM_MODE_COOL = "cool";
const char* SYSTEM_MODE_HEAT = "heat";
const char* SYSTEM_MODE_FAN = "fan_only";
const char* FAN_MODE_AUTO = SCHEDULES_MODE_AUTO;
const char* FAN_MODE_LOW  = "low";
const char* FAN_MODE_MEDIUM  = "medium";
const char* FAN_MODE_HIGH = "high";

class WThermostat_BAC_002_ALW : public WThermostat {
public :
  WThermostat_BAC_002_ALW(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_BAC_002_ALW created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x01;
    this->byteTemperatureActual = 0x03;
    this->byteTemperatureTarget = 0x02;
    this->byteTemperatureFloor = NOT_SUPPORTED;
    this->temperatureFactor = 2.0f;
    this->byteSchedulesMode = 0x04;
    this->byteLocked = 0x06;
    this->byteSchedules = 0x68;
    this->byteSchedulingPosHour = 1;
    this->byteSchedulingPosMinute = 0;
    this->byteSchedulingDays = 18;
    //custom
    this->byteSystemMode = 0x66;
    this->byteFanMode = 0x67;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //systemMode
    this->systemMode = new WProperty("systemMode", "System Mode", STRING, TYPE_THERMOSTAT_MODE_PROPERTY);
    this->systemMode->addEnumString(SYSTEM_MODE_COOL);
    this->systemMode->addEnumString(SYSTEM_MODE_HEAT);
    this->systemMode->addEnumString(SYSTEM_MODE_FAN);
    this->systemMode->setOnChange(std::bind(&WThermostat_BAC_002_ALW::systemModeToMcu, this, std::placeholders::_1));
    this->addProperty(systemMode);
    //fanMode
    this->fanMode = new WProperty("fanMode", "Fan", STRING, TYPE_FAN_MODE_PROPERTY);
    this->fanMode->addEnumString(FAN_MODE_AUTO);
    this->fanMode->addEnumString(FAN_MODE_HIGH);
    this->fanMode->addEnumString(FAN_MODE_MEDIUM);
    this->fanMode->addEnumString(FAN_MODE_LOW);
    this->fanMode->setOnChange(std::bind(&WThermostat_BAC_002_ALW::fanModeToMcu, this, std::placeholders::_1));
    this->addProperty(fanMode);
  }

protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);

		if (!knownCommand) {
      const char* newS;
      if (cByte == this->byteSystemMode) {
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
      } else if (cByte == this->byteFanMode) {
				if (commandLength == 0x05) {
					//fanMode
					//auto   - 55 aa 01 07 00 05 67 04 00 01 00
					//high   - 55 aa 01 07 00 05 67 04 00 01 01
					//medium - 55 aa 01 07 00 05 67 04 00 01 02
					//low    - 55 aa 01 07 00 05 67 04 00 01 03
					newS = fanMode->getEnumString(receivedCommand[10]);
					if (newS != nullptr) {
						changed = ((changed) || (fanMode->setString(newS)));
						knownCommand = true;
					}
				}
			} else if (cByte == 0x05) {
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

  void fanModeToMcu(WProperty* property) {
    if (!isReceivingDataFromMcu()) {
      byte fm = fanMode->getEnumIndex();
      if (fm != 0xFF) {
        //send to device
        //auto:   55 aa 00 06 00 05 67 04 00 01 00
        //high:   55 aa 00 06 00 05 67 04 00 01 01
        //medium: 55 aa 00 06 00 05 67 04 00 01 02
        //low:    55 aa 00 06 00 05 67 04 00 01 03
        unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
                                            this->byteFanMode, 0x04, 0x00, 0x01, fm};
        commandCharsToSerial(11, deviceOnCommand);
      }
    }
  }

private :
  WProperty* systemMode;
  byte byteSystemMode;
  WProperty* fanMode;
  byte byteFanMode;

};

#endif
