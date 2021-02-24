#ifndef THERMOSTAT_MH_1820_H
#define	THERMOSTAT_MH_1820_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

/* 
const char* SYSTEM_MODE_COOL = "cool";
const char* SYSTEM_MODE_HEAT = "heat";
const char* SYSTEM_MODE_FAN = "fan_only";
const char* FAN_MODE_AUTO = SCHEDULES_MODE_AUTO;
const char* FAN_MODE_LOW  = "low";
const char* FAN_MODE_MEDIUM  = "medium";
const char* FAN_MODE_HIGH = "high";
*/
const char* SYSTEM_MODE_MANUAL = "manual";
const char* SYSTEM_MODE_PROGRAM = "auto";
const char* SYSTEM_MODE_HOLIDAY = "holiday";

class WThermostat_MH_1820 : public WThermostat {
public :
  WThermostat_MH_1820(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_MH_1820 created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x01;
    this->byteTemperatureActual = 0x21;
    this->byteTemperatureTarget = 0x16;
    this->byteTemperatureFloor = 0x65;
    this->temperatureFactor = 10.0f;
    this->temperatureFactorTarget = 1.0f;
    this->byteSchedulesMode = 0x04;
    this->byteLocked = 0x05;
    this->byteSchedules = 0x2a;
    this->byteSchedulingPosHour = 1;
    this->byteSchedulingPosMinute = 0;
    this->byteSchedulingDays = 18;
    //custom
		this->byteRelayState = 3; // start -> 0, stop -> 1
  	this->byteWifiState = 0x2b;
  	this->byteWifiRssi = 0x24;
    this->byteSystemMode = 0x2;
    this->byteHolidays = 0x20;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //systemMode
    this->systemMode = new WProperty("systemMode", "System Mode", STRING, TYPE_THERMOSTAT_MODE_PROPERTY);
    this->systemMode->addEnumString(SYSTEM_MODE_MANUAL);
    this->systemMode->addEnumString(SYSTEM_MODE_PROGRAM);
    this->systemMode->addEnumString(SYSTEM_MODE_HOLIDAY);
    this->systemMode->setOnChange(std::bind(&WThermostat_MH_1820::systemModeToMcu, this, std::placeholders::_1));
    this->addProperty(systemMode);
    //fanMode
    this->fanMode = new WProperty("fanMode", "Fan", STRING, TYPE_FAN_MODE_PROPERTY);
    this->fanMode->addEnumString(FAN_MODE_AUTO);
    this->fanMode->addEnumString(FAN_MODE_HIGH);
    this->fanMode->addEnumString(FAN_MODE_MEDIUM);
    this->fanMode->addEnumString(FAN_MODE_LOW);
    this->fanMode->setOnChange(std::bind(&WThermostat_MH_1820::fanModeToMcu, this, std::placeholders::_1));
    this->addProperty(fanMode);

    this->relay = WProperty::createOnOffProperty("relay", "Relay");
		network->getSettings()->add(this->relay);
		this->relay->setReadOnly(true);
		// this->relay->setVisibility(MQTT);
    network->getSettings()->add(this->relay);
    this->addProperty(relay);

  }


protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);
		char str[10];

		if (!knownCommand) {
      const char* newS;
      if (cByte == this->byteSystemMode) {
        if (commandLength == 0x05) {
          //MODEL_BAC_002_ALW - systemMode
          //cooling:     55 AA 00 06 00 05 66 04 00 01 00
          //heating:     55 AA 00 06 00 05 66 04 00 01 01
          //ventilation: 55 AA 00 06 00 05 66 04 00 01 02
          newS = systemMode->getEnumString(receivedCommand[10]);
					publishMqttProperty("","systemMode",(char *)newS);
          if (newS != nullptr) {
            changed = ((changed) || (systemMode->setString(newS)));
            knownCommand = true;
          }
        }
      } else if (cByte == this->byteFanMode) {
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
			} else if (cByte == 0x25) {
        if (commandLength == 0x08) {
          // room temperature in Fahrenheit -> ignore
          knownCommand = true;
        }
			} else if (cByte == 0x66) {
        if (commandLength == 0x08) {
          // floor temperature in Fahrenheit -> ignore
          knownCommand = true;
        }
			} else if (cByte == byteRelayState) {
				if (commandLength == 0x05) {
					// Relay state:
					//inverted - 55 aa 03 07 00 05 03 04 00 01 00 = on (start)
					//inverted - 55 aa 03 07 00 05 03 04 00 01 01 = off (stop)

					bool rawValue = ((receivedCommand[10]&0xff)?false:true);
					// changed = ((changed) || (!WProperty::isEqual(relay, rawValue, 0)));
					changed = ((changed) || (rawValue != relay->getBoolean()));
					// relayState = rawValue;
					// if (changed) 
					relay->setBoolean(rawValue);
					if (rawValue)
						sprintf(str,"true");
					else
						sprintf(str,"false");
					publishMqttProperty("","relay",str);
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

  virtual bool processCommand(byte commandByte, byte length) {
		bool knownCommand = WTuyaDevice::processCommand(commandByte, length);
    if (commandByte == 0x03) {
			//ignore, MCU response to wifi state
			//55 aa 01 03 00 00
			knownCommand = (length == 0);
		} else if (commandByte == 0x04) {
			//Setup initialization request
			//received: 55 aa 01 04 00 00
			//send answer: 55 aa 00 03 00 01 00
			unsigned char configCommand[] = { 0x55, 0xAA, 0x00, 0x03, 0x00,	0x01, 0x00 };
			commandCharsToSerial(7, configCommand);
			network->startWebServer();
			knownCommand = true;
		} else if (commandByte == 0x1C) {
			//Request for time sync from MCU : 55 aa 01 1c 00 00
			this->sendActualTimeToBeca();
			knownCommand = true;
		} else if (commandByte == this->byteWifiState) { // wifi byte?
			if (commandLength == 0x00) {
				// get wifi state 
				// set wifi status to 3/4 - connected
				int status=0;
				if (network->isWifiConnected()) {
					status=3;
					// rssi=0xff-WiFi.RSSI();
					if (network->isMqttConnected()) {
						status++;
					}
				}
				unsigned char setWifiCommand[] = { 0x55, 0xaa, 0x00, byteWifiState, 0x00, 0x01, status};
				commandCharsToSerial(7, setWifiCommand);
				knownCommand = true;
			}
		} else if (commandByte == this->byteWifiRssi) { // wifi quality
			if (commandLength == 0x00) {
				// get wifi rssi ...
				int rssi=0;
				int connected=network->isWifiConnected();
				if (connected) {
					rssi=0xff-WiFi.RSSI();
				}
				unsigned char setRssiCommand[] = { 0x55, 0xaa, 0x00, byteWifiRssi, 0x00, 0x01, 0xff-rssi};
				commandCharsToSerial(7, setRssiCommand);
				knownCommand = true;
			}    
    }
		return knownCommand;
	}


private :
  WProperty* systemMode;
  byte byteSystemMode;
  byte byteHolidays;
  byte byteRelayState;
  byte byteWifiState;
  byte byteWifiRssi;
  WProperty* fanMode;
  byte byteFanMode;
	WProperty* relay;

};

#endif