#ifndef THERMOSTAT_CALYPSOW_H
#define	THERMOSTAT_CALYPSOW_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

class WThermostat_CalypsoW : public WThermostat {
public :
  WThermostat_CalypsoW(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_CalypsoW created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x01;
    this->byteTemperatureActual = 0x05;
    this->byteTemperatureTarget = 0x02;
    this->byteTemperatureFloor = 0x08;
    this->temperatureFactor = 10.0f;
    this->byteSchedulesMode = 0x03;
    this->byteLocked = 0x06;
    this->byteSchedules = NOT_SUPPORTED;
    this->byteSchedulingPosHour = 1;
    this->byteSchedulingPosMinute = 0;
    this->byteSchedulingDays = 18;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //schedulesMode
    this->schedulesMode->clearEnums();
    this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLIDAY);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLD);
  }

protected :

private :

};

#endif
