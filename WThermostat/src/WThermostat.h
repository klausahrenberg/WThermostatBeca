#ifndef THERMOSTAT_H
#define	THERMOSTAT_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WTuyaDevice.h"
#include "WClock.h"

#define COUNT_DEVICE_MODELS 6
#define MODEL_BHT_002_GBLW 0
#define MODEL_BAC_002_ALW 1
#define MODEL_ET81W 2
#define MODEL_HY08WE 3
#define MODEL_ME81H 4
#define MODEL_MK70GBH 5
#define MODEL_ME102H 6
#define MODEL_CALYPSOW 7
#define MODEL_DLX_LH01 8
#define PIN_STATE_HEATING_RELAY 5
#define NOT_SUPPORTED 0x00

const char* SCHEDULES = "schedules";
const char* SCHEDULES_MODE_OFF = "off";
const char* SCHEDULES_MODE_AUTO = "auto";
const char* SCHEDULES_MODE_HOLD = "hold";
const char* STATE_OFF = SCHEDULES_MODE_OFF;
const char* STATE_HEATING = "heating";
const char SCHEDULES_PERIODS[] = "123456";
const char SCHEDULES_DAYS[] = "wau";

class WThermostat : public WTuyaDevice {
public :
  WThermostat(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WTuyaDevice(network, "thermostat", "thermostat", DEVICE_TYPE_THERMOSTAT) {
    this->thermostatModel = thermostatModel;
    this->wClock = wClock;
    this->wClock->setOnTimeUpdate([this]() {
      this->sendActualTimeToBeca();
    });
    lastNotify = lastScheduleNotify = 0;
    this->schedulesChanged = false;
		this->schedulesReceived = false;
		this->targetTemperatureManualMode = 0.0;
    this->currentSchedulePeriod = -1;
    //HtmlPages
    WPage* configPage = new WPage(this->getId(), "Configure thermostat");
    configPage->setPrintPage(std::bind(&WThermostat::printConfigPage, this, std::placeholders::_1, std::placeholders::_2));
    configPage->setSubmittedPage(std::bind(&WThermostat::submitConfigPage, this, std::placeholders::_1, std::placeholders::_2));
    network->addCustomPage(configPage);
    WPage* schedulesPage = new WPage(SCHEDULES, "Configure schedules");
    schedulesPage->setPrintPage(std::bind(&WThermostat::printConfigSchedulesPage, this, std::placeholders::_1, std::placeholders::_2));
    schedulesPage->setSubmittedPage(std::bind(&WThermostat::submitConfigSchedulesPage, this, std::placeholders::_1, std::placeholders::_2));
    network->addCustomPage(schedulesPage);
  }

  virtual void configureCommandBytes() {
    //command bytes
    this->byteDeviceOn = 0x01;
    this->byteTemperatureActual = NOT_SUPPORTED;
    this->byteTemperatureTarget = NOT_SUPPORTED;
    this->byteTemperatureFloor = NOT_SUPPORTED;
    this->temperatureFactor = 2.0f;
    this->byteSchedulesMode = NOT_SUPPORTED;
    this->byteLocked = NOT_SUPPORTED;
    this->byteSchedules = NOT_SUPPORTED;
    this->byteSchedulingPosHour = 1;
    this->byteSchedulingPosMinute = 0;
    this->byteSchedulingDays = 18;
  }

  virtual bool isDeviceStateComplete() {
      return (((this->byteDeviceOn == NOT_SUPPORTED)          || (!this->deviceOn->isNull())) &&
              ((this->byteTemperatureActual == NOT_SUPPORTED) || (!this->actualTemperature->isNull())) &&
              ((this->byteTemperatureTarget == NOT_SUPPORTED) || (this->targetTemperatureManualMode != 0.0)) &&
              ((this->byteTemperatureFloor == NOT_SUPPORTED)  || (!this->actualFloorTemperature->isNull())) &&
              ((this->byteSchedulesMode == NOT_SUPPORTED)     || (!this->schedulesMode->isNull()))
             );
  }

  virtual void initializeProperties() {
    //schedulesDayOffset
    this->schedulesDayOffset = network->getSettings()->setByte("schedulesDayOffset", 0);
    //standard properties
    this->actualTemperature = WProperty::createTemperatureProperty("temperature", "Actual");
    this->actualTemperature->setReadOnly(true);
    this->addProperty(actualTemperature);
    this->targetTemperature = WProperty::createTargetTemperatureProperty("targetTemperature", "Target");
    this->targetTemperature->setMultipleOf(1.0f / this->temperatureFactor);
    this->targetTemperature->setOnChange(std::bind(&WThermostat::setTargetTemperature, this, std::placeholders::_1));
    this->targetTemperature->setOnValueRequest([this](WProperty* p) {updateTargetTemperature();});
    this->addProperty(targetTemperature);
    if (byteTemperatureFloor != NOT_SUPPORTED) {
			this->actualFloorTemperature = WProperty::createTargetTemperatureProperty("floorTemperature", "Floor");
    	this->actualFloorTemperature->setReadOnly(true);
    	this->actualFloorTemperature->setVisibility(MQTT);
    	this->addProperty(actualFloorTemperature);
		} else {
      this->actualFloorTemperature = nullptr;
    }
    this->deviceOn = WProperty::createOnOffProperty("deviceOn", "Power");
    //2021-01-24 test bht-002 bug
    network->getSettings()->add(this->deviceOn);
    this->deviceOn->setOnChange(std::bind(&WThermostat::deviceOnToMcu, this, std::placeholders::_1));
    this->addProperty(deviceOn);
    this->schedulesMode = new WProperty("schedulesMode", "Schedules", STRING, TYPE_THERMOSTAT_MODE_PROPERTY);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_OFF);
    this->schedulesMode->setOnChange(std::bind(&WThermostat::schedulesModeToMcu, this, std::placeholders::_1));
    this->addProperty(schedulesMode);
    this->switchBackToAuto = network->getSettings()->setBoolean("switchBackToAuto", true);
    this->locked = WProperty::createOnOffProperty("locked", "Lock");
    this->locked->setOnChange(std::bind(&WThermostat::lockedToMcu, this, std::placeholders::_1));
    this->locked->setVisibility(MQTT);
    this->addProperty(locked);
    this->completeDeviceState = network->getSettings()->setBoolean("sendCompleteDeviceState", true);
    //Heating Relay and State property
    this->state = nullptr;
    this->supportingHeatingRelay = network->getSettings()->setBoolean("supportingHeatingRelay", true);
    if (this->supportingHeatingRelay->getBoolean()) {
      pinMode(PIN_STATE_HEATING_RELAY, INPUT);
    	this->state = new WProperty("state", "State", STRING, TYPE_HEATING_COOLING_PROPERTY);
    	this->state->setReadOnly(true);
    	this->state->addEnumString(STATE_OFF);
    	this->state->addEnumString(STATE_HEATING);
    	this->addProperty(state);
    }
  }

  virtual void printConfigPage(AsyncWebServerRequest* request, Print* page) {
    page->printf(HTTP_CONFIG_PAGE_BEGIN, getId());
    //ComboBox with model selection
    page->printf(HTTP_COMBOBOX_BEGIN, "Thermostat model:", "tm");
    page->printf(HTTP_COMBOBOX_ITEM, "0", (this->thermostatModel->getByte() == 0 ? HTTP_SELECTED : ""), "BHT-002, BHT-6000, BHT-3000 (floor heating)");
    page->printf(HTTP_COMBOBOX_ITEM, "6", (this->thermostatModel->getByte() == 6 ? HTTP_SELECTED : ""), "AVATTO ME102H (Touch screen)");
    page->printf(HTTP_COMBOBOX_ITEM, "1", (this->thermostatModel->getByte() == 1 ? HTTP_SELECTED : ""), "BAC-002, BAC-1000 (heating, cooling, ventilation)");
		page->printf(HTTP_COMBOBOX_ITEM, "2", (this->thermostatModel->getByte() == 2 ? HTTP_SELECTED : ""), "ET-81W");
		page->printf(HTTP_COMBOBOX_ITEM, "3", (this->thermostatModel->getByte() == 3 ? HTTP_SELECTED : ""), "Floureon HY08WE");
		page->printf(HTTP_COMBOBOX_ITEM, "4", (this->thermostatModel->getByte() == 4 ? HTTP_SELECTED : ""), "AVATTO ME81AH");
		page->printf(HTTP_COMBOBOX_ITEM, "5", (this->thermostatModel->getByte() == 5 ? HTTP_SELECTED : ""), "Minco Heat MK70GB-H");
    page->printf(HTTP_COMBOBOX_ITEM, "7", (this->thermostatModel->getByte() == 7 ? HTTP_SELECTED : ""), "VH Control Calypso-W");
    page->printf(HTTP_COMBOBOX_ITEM, "8", (this->thermostatModel->getByte() == 8 ? HTTP_SELECTED : ""), "DLX-LH01");
    page->print(FPSTR(HTTP_COMBOBOX_END));
    //Checkbox
    page->printf(HTTP_CHECKBOX_OPTION, "sb", "sb", (this->switchBackToAuto->getBoolean() ? HTTP_CHECKED : ""), "", "Auto mode from manual mode at next schedule period change <br> (not at model ET-81W and ME81AH)");
    //ComboBox with weekday
    page->printf(HTTP_COMBOBOX_BEGIN, "Workday schedules:", "ws");
    page->printf(HTTP_COMBOBOX_ITEM, "0", (getSchedulesDayOffset() == 0 ? HTTP_SELECTED : ""), "Workday Mon-Fri; Weekend Sat-Sun");
    page->printf(HTTP_COMBOBOX_ITEM, "1", (getSchedulesDayOffset() == 1 ? HTTP_SELECTED : ""), "Workday Sun-Thu; Weekend Fri-Sat");
    page->printf(HTTP_COMBOBOX_ITEM, "2", (getSchedulesDayOffset() == 2 ? HTTP_SELECTED : ""), "Workday Sat-Wed; Weekend Thu-Fri");
    page->printf(HTTP_COMBOBOX_ITEM, "3", (getSchedulesDayOffset() == 3 ? HTTP_SELECTED : ""), "Workday Fri-Tue; Weekend Wed-Thu");
    page->printf(HTTP_COMBOBOX_ITEM, "4", (getSchedulesDayOffset() == 4 ? HTTP_SELECTED : ""), "Workday Thu-Mon; Weekend Tue-Wed");
    page->printf(HTTP_COMBOBOX_ITEM, "5", (getSchedulesDayOffset() == 5 ? HTTP_SELECTED : ""), "Workday Wed-Sun; Weekend Mon-Tue");
    page->printf(HTTP_COMBOBOX_ITEM, "6", (getSchedulesDayOffset() == 6 ? HTTP_SELECTED : ""), "Workday Tue-Sat; Weekend Sun-Mon");
    page->print(FPSTR(HTTP_COMBOBOX_END));
		page->printf(HTTP_CHECKBOX_OPTION, "cr", "cr", (this->sendCompleteDeviceState() ? "" : HTTP_CHECKED), "", "Send changes in separate MQTT messages");
		//notifyAllMcuCommands
		page->printf(HTTP_CHECKBOX_OPTION, "am", "am", (this->notifyAllMcuCommands->getBoolean() ? HTTP_CHECKED : ""), "", "Send all MCU commands via MQTT");
    //Checkbox with support for relay
		page->printf(HTTP_CHECKBOX_OPTION, "rs", "rs", (this->supportingHeatingRelay->getBoolean() ? HTTP_CHECKED : ""), "", "Relay at GPIO 5 (not working without hw mod)");

    printConfigPageCustomParameters(request, page);

  	page->print(FPSTR(HTTP_CONFIG_SAVE_BUTTON));
  }

  virtual void printConfigPageCustomParameters(AsyncWebServerRequest* request, Print* page) {

  }

  virtual void submitConfigPage(AsyncWebServerRequest* request, Print* page) {
    this->thermostatModel->setByte(request->arg("tm").toInt());
    this->schedulesDayOffset->setByte(request->arg("ws").toInt());
    this->switchBackToAuto->setBoolean(request->arg("sb") == HTTP_TRUE);
		this->completeDeviceState->setBoolean(request->arg("cr") != HTTP_TRUE);
		this->notifyAllMcuCommands->setBoolean(request->arg("am") == HTTP_TRUE);
    this->supportingHeatingRelay->setBoolean(request->arg("rs") == HTTP_TRUE);
    submitConfigPageCustomParameters(request, page);
  }

  virtual void submitConfigPageCustomParameters(AsyncWebServerRequest* request, Print* page) {

  }

  void handleUnknownMqttCallback(bool getState, String completeTopic, String partialTopic, char *payload, unsigned int length) {
    if (partialTopic.startsWith(SCHEDULES)) {
      if (byteSchedules != NOT_SUPPORTED) {
        partialTopic = partialTopic.substring(strlen(SCHEDULES) + 1);
        if (getState) {
          //Send actual schedules
          handleSchedulesChange(completeTopic);
        } else if (length > 0) {
          //Set schedules
          network->debug(F("Payload for schedules -> set schedules..."));
          WJsonParser* parser = new WJsonParser();
          schedulesChanged = false;
          parser->parse(payload, std::bind(&WThermostat::processSchedulesKeyValue, this,
                std::placeholders::_1, std::placeholders::_2));
          delete parser;
          if (schedulesChanged) {
            network->debug(F("Some schedules changed. Write to MCU..."));
            this->schedulesToMcu();
          }
        }
      }
    }
  }

  void processSchedulesKeyValue(const char* key, const char* value) {
    byte hh_Offset = byteSchedulingPosHour;
    byte mm_Offset = byteSchedulingPosMinute;
    if (strlen(key) == 3) {
      byte startAddr = 255;
      byte period = 255;
      for (int i = 0; i < 6; i++) {
        if (SCHEDULES_PERIODS[i] == key[1]) {
          period = i;
          break;
        }
      }
      if (key[0] == SCHEDULES_DAYS[0]) {
        startAddr = 0;
      } else if (key[0] == SCHEDULES_DAYS[1]) {
        startAddr = 18;
      } else if (key[0] == SCHEDULES_DAYS[2]) {
        startAddr = 36;
      }
      if ((startAddr != 255) && (period != 255)) {
				byte index = startAddr + period * 3;
				if (index < this->byteSchedulingDays * 3) {
        	if (key[2] == 'h') {
          	//hour
          	String timeStr = String(value);
          	timeStr = (timeStr.length() == 4 ? "0" + timeStr : timeStr);
          	if (timeStr.length() == 5) {
            	byte hh = timeStr.substring(0, 2).toInt();
            	byte mm = timeStr.substring(3, 5).toInt();
            	schedulesChanged = schedulesChanged || (schedules[index + hh_Offset] != hh);
            	schedules[index + hh_Offset] = hh;
            	schedulesChanged = schedulesChanged || (schedules[index + mm_Offset] != mm);
            	schedules[index + mm_Offset] = mm;
          	}
        	} else if (key[2] == 't') {
          	//temperature
          	//it will fail, when temperature needs 2 bytes
          	int tt = (int) (atof(value) * this->temperatureFactor);
          	if (tt < 0xFF) {
            	schedulesChanged = schedulesChanged || (schedules[index + 2] != tt);
            	schedules[index + 2] = tt;
          	}
        	}
				}
      }
    }
  }

  void sendSchedules(AsyncWebServerRequest* request) {
    WStringStream* response = network->getResponseStream();
    WJson json(response);
    json.beginObject();
    this->toJsonSchedules(&json, 0);// SCHEDULE_WORKDAY);
    this->toJsonSchedules(&json, 1);// SCHEDULE_WEEKEND_1);
    this->toJsonSchedules(&json, 2);// SCHEDULE_WEEKEND_2);
    json.endObject();
    request->send(200, APPLICATION_JSON, response->c_str());
  }

  virtual void toJsonSchedules(WJson* json, byte schedulesDay) {
    byte startAddr = 0;
    char dayChar = SCHEDULES_DAYS[0];
    byte hh_Offset = byteSchedulingPosHour;
    byte mm_Offset = byteSchedulingPosMinute;
    switch (schedulesDay) {
    case 1 :
      startAddr = 18;
      dayChar = SCHEDULES_DAYS[1];
      break;
    case 2 :
      startAddr = 36;
      dayChar = SCHEDULES_DAYS[2];
      break;
    }
    char timeStr[6];
    timeStr[5] = '\0';
    char* buffer = new char[4];
    buffer[0] = dayChar;
    buffer[3] = '\0';
    for (int i = 0; i < 6; i++) {
			byte index = startAddr + i * 3;
			if (index < this->byteSchedulingDays * 3) {
      	buffer[1] = SCHEDULES_PERIODS[i];
      	sprintf(timeStr, "%02d:%02d", schedules[index + hh_Offset], schedules[index + mm_Offset]);
      	buffer[2] = 'h';
      	json->propertyString(buffer, timeStr);
      	buffer[2] = 't';
      	json->propertyDouble(buffer, (double) schedules[index + 2]	/ this->temperatureFactor);
			} else {
				break;
			}
    }
    delete[] buffer;
  }

  virtual void loop(unsigned long now) {
    if (state != nullptr) {
      bool heating = false;
      if ((this->supportingHeatingRelay->getBoolean()) && (state != nullptr)) {
        heating = digitalRead(PIN_STATE_HEATING_RELAY);
      }
      this->state->setString(heating ? STATE_HEATING : STATE_OFF);
    }
    WTuyaDevice::loop(now);
    updateCurrentSchedulePeriod();
    if (receivedSchedules()) {
      //Notify schedules
      if ((network->isMqttConnected()) && (lastScheduleNotify == 0) && (now - lastScheduleNotify > MINIMUM_INTERVAL)) {
        handleSchedulesChange("");
        schedulesChanged = false;
        lastScheduleNotify = now;
      }
    }
  }

  virtual bool sendCompleteDeviceState() {
      return this->completeDeviceState->getBoolean();
  }

protected :
  WClock *wClock;
  WProperty* schedulesDayOffset;
  byte byteDeviceOn;
  byte byteTemperatureActual;
  byte byteTemperatureTarget;
  byte byteTemperatureFloor;
  byte byteSchedulesMode;
  byte byteLocked;
  byte byteSchedules;
  byte byteSchedulingPosHour;
  byte byteSchedulingPosMinute;
  byte byteSchedulingDays;
  byte byteSchedulingFunctionL;
  byte byteSchedulingDataL;
  float temperatureFactor;
  WProperty* thermostatModel;
  WProperty* actualTemperature;
  WProperty* targetTemperature;
  WProperty* actualFloorTemperature;
  double targetTemperatureManualMode;
  WProperty* deviceOn;
  WProperty* schedulesMode;
  WProperty *completeDeviceState;
  WProperty* switchBackToAuto;
  WProperty* locked;
  WProperty* state;
  WProperty* supportingHeatingRelay;
  byte schedules[54];

  void sendActualTimeToBeca() {
    //Command: Set date and time
    //                       ?? YY MM DD HH MM SS Weekday
    //DEC:                   01 19 02 15 16 04 18 05
    //HEX: 55 AA 00 1C 00 08 01 13 02 0F 10 04 12 05
    //DEC:                   01 19 02 20 17 51 44 03
    //HEX: 55 AA 00 1C 00 08 01 13 02 14 11 33 2C 03
    unsigned long epochTime = wClock->getEpochTime();
    epochTime = epochTime + (getSchedulesDayOffset() * 86400);
    byte year = wClock->getYear(epochTime) % 100;
    byte month = wClock->getMonth(epochTime);
    byte dayOfMonth = wClock->getDay(epochTime);
    byte hours = wClock->getHours(epochTime) ;
    byte minutes = wClock->getMinutes(epochTime);
    byte seconds = wClock->getSeconds(epochTime);
    byte dayOfWeek = getDayOfWeek();
    unsigned char cancelConfigCommand[] = { 0x55, 0xaa, 0x00, 0x1c, 0x00, 0x08,
                                            0x01, year, month, dayOfMonth,
                                            hours, minutes, seconds, dayOfWeek};
    commandCharsToSerial(14, cancelConfigCommand);
  }

  byte getDayOfWeek() {
    unsigned long epochTime = wClock->getEpochTime();
    epochTime = epochTime + (getSchedulesDayOffset() * 86400);
    byte dayOfWeek = wClock->getWeekDay(epochTime);
    //make sunday a seven
    dayOfWeek = (dayOfWeek ==0 ? 7 : dayOfWeek);
    return dayOfWeek;
  }

  bool isWeekend() {
    byte dayOfWeek = getDayOfWeek();
    return ((dayOfWeek == 6) || (dayOfWeek == 7));
  }

  byte getSchedulesDayOffset() {
    return schedulesDayOffset->getByte();
  }

  virtual bool processCommand(byte commandByte, byte length) {
		bool knownCommand = WTuyaDevice::processCommand(commandByte, length);
    switch (commandByte) {
      case 0x03: {
        //ignore, MCU response to wifi state
        //55 aa 01 03 00 00
        knownCommand = (length == 0);
        break;
      }
      case 0x04: {
        //Setup initialization request
        //received: 55 aa 01 04 00 00
        //send answer: 55 aa 00 03 00 01 00
        unsigned char configCommand[] = { 0x55, 0xAA, 0x00, 0x03, 0x00,	0x01, 0x00 };
        commandCharsToSerial(7, configCommand);
        network->startWebServer();
        knownCommand = true;
        break;
      }
      case 0x1C: {
        //Request for time sync from MCU : 55 aa 01 1c 00 00
        this->sendActualTimeToBeca();
        knownCommand = true;
        break;
      }
    }
		return knownCommand;
	}

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
    //Status report from MCU
    bool changed = false;
    bool newB;
    float newValue;
    const char* newS;
    bool knownCommand = false;
    if (cByte == byteDeviceOn ) {
      if (commandLength == 0x05) {
        //device On/Off
        //55 aa 00 06 00 05 01 01 00 01 00|01
        //2021-01-24 test for bht-002
        if (!this->mcuRestarted) {
          newB = (receivedCommand[10] == 0x01);
          changed = ((changed) || (newB != deviceOn->getBoolean()));
          deviceOn->setBoolean(newB);
        } else if (!this->deviceOn->isNull()) {
          deviceOnToMcu(this->deviceOn);
          this->mcuRestarted = false;
        }
        knownCommand = true;
      }
    } else if (cByte == byteTemperatureTarget) {
      if (commandLength == 0x08) {
        //target Temperature for manual mode
        //e.g. 24.5C: 55 aa 01 07 00 08 02 02 00 04 00 00 00 31

        unsigned long rawValue = WSettings::getUnsignedLong(receivedCommand[10], receivedCommand[11], receivedCommand[12], receivedCommand[13]);
        newValue = (float) rawValue / this->temperatureFactor;
        changed = ((changed) || (!WProperty::isEqual(targetTemperatureManualMode, newValue, 0.01)));
        targetTemperatureManualMode = newValue;
				if (changed) updateTargetTemperature();
        knownCommand = true;
      }
    } else if ((byteTemperatureActual != NOT_SUPPORTED) && (cByte == byteTemperatureActual)) {
      if (commandLength == 0x08) {
        //actual Temperature
        //e.g. 23C: 55 aa 01 07 00 08 03 02 00 04 00 00 00 2e
        unsigned long rawValue = WSettings::getUnsignedLong(receivedCommand[10], receivedCommand[11], receivedCommand[12], receivedCommand[13]);
        newValue = (float) rawValue / this->temperatureFactor;
        changed = ((changed) || (!actualTemperature->equalsDouble(newValue)));
        actualTemperature->setDouble(newValue);
        knownCommand = true;
      }
    } else if ((byteTemperatureFloor != NOT_SUPPORTED) && (cByte == byteTemperatureFloor)) {
      if (commandLength == 0x08) {
        //MODEL_BHT_002_GBLW - actualFloorTemperature
        //55 aa 01 07 00 08 66 02 00 04 00 00 00 00
        unsigned long rawValue = WSettings::getUnsignedLong(receivedCommand[10], receivedCommand[11], receivedCommand[12], receivedCommand[13]);
        newValue = (float) rawValue / this->temperatureFactor;
        changed = ((changed) || (!actualFloorTemperature->equalsDouble(newValue)));
        actualFloorTemperature->setDouble(newValue);
        knownCommand = true;
      }
    } else if (cByte == byteSchedulesMode) {
      if (commandLength == 0x05) {
        //schedulesMode
        newS = schedulesMode->getEnumString(receivedCommand[10]);
        if (newS != nullptr) {
          changed = ((changed) || (schedulesMode->setString(newS)));
          if (changed) updateTargetTemperature();
          knownCommand = true;
        }
      }
    } else if (cByte == byteLocked) {
      if (commandLength == 0x05) {
        //locked
        newB = (receivedCommand[10] == 0x01);
        changed = ((changed) || (newB != locked->getBoolean()));
        locked->setBoolean(newB);
        knownCommand = true;
      }
    } else if (cByte == byteSchedules) {
      this->schedulesReceived = this->processStatusSchedules(commandLength);
      knownCommand = this->schedulesReceived;
    }
    if (changed) {
      notifyState();
    }
    return knownCommand;
  }

  virtual bool processStatusSchedules(byte commandLength) {
    bool result = (commandLength == (this->byteSchedulingDays * 3 + 4));
    if (result) {
      bool changed = false;
      //schedules 0x65 at heater model, 0x68 at fan model, example
      //55 AA 00 06 00 3A 65 00 00 36
      //00 07 28 00 08 1E 1E 0B 1E 1E 0D 1E 00 11 2C 00 16 1E
      //00 06 28 00 08 28 1E 0B 28 1E 0D 28 00 11 28 00 16 1E
      //00 06 28 00 08 28 1E 0B 28 1E 0D 28 00 11 28 00 16 1E
      for (int i = 0; i < this->byteSchedulingDays * 3; i++) {
        byte newByte = receivedCommand[i + 10];
        changed = ((changed) || (newByte != schedules[i]));
        schedules[i] = newByte;
      }
      if (changed) {
        notifySchedules();
      }
    }
    return result;
  }

  void updateCurrentSchedulePeriod() {
    if ((receivedSchedules()) && (wClock->isValidTime())) {
      byte hh_Offset = byteSchedulingPosHour;
      byte mm_Offset = byteSchedulingPosMinute;
      byte weekDay = wClock->getWeekDay();
      weekDay += getSchedulesDayOffset();
      weekDay = weekDay % 7;
      int startAddr = (this->byteSchedulingDays == 18 ? (weekDay == 0 ? 36 : (weekDay == 6 ? 18 : 0)) : ((weekDay == 0) || (weekDay == 6) ? 18 : 0));
      int period = 0;
      if (wClock->isTimeEarlierThan(schedules[startAddr + period * 3 + hh_Offset], schedules[startAddr + period * 3 + mm_Offset])) {
        //Jump back to day before and last schedule of day
        weekDay = weekDay - 1;
        weekDay = weekDay % 7;
        startAddr = (this->byteSchedulingDays == 18 ? (weekDay == 0 ? 36 : (weekDay == 6 ? 18 : 0)) : ((weekDay == 0) || (weekDay == 6) ? 18 : 0));
        period = 5;
      } else {
        //check the schedules in same day
        for (int i = 1; i < 6; i++) {
          int index = startAddr + i * 3;
  				if (index < this->byteSchedulingDays * 3) {
            if ((i < 5) && (index + 1 < this->byteSchedulingDays * 3)) {
              if (wClock->isTimeBetween(schedules[index + hh_Offset], schedules[index + mm_Offset],
                                    schedules[startAddr + (i + 1) * 3 + hh_Offset], schedules[startAddr + (i + 1) * 3 + mm_Offset])) {
                period = i;
                break;
              }
            } else if (wClock->isTimeLaterThan(schedules[index + hh_Offset], schedules[index + mm_Offset])) {
              period = (i == 5 ? 5 : 1);
            }
          } else {
            break;
          }
        }
      }
      int newPeriod = startAddr + period * 3;
      if ((this->switchBackToAuto->getBoolean()) &&
          (this->currentSchedulePeriod > -1) && (newPeriod != this->currentSchedulePeriod) &&
          (this->schedulesMode->equalsString(SCHEDULES_MODE_OFF))) {
        this->schedulesMode->setString(SCHEDULES_MODE_AUTO);
      }
      this->currentSchedulePeriod = newPeriod;
    } else {
      this->currentSchedulePeriod = -1;
    }
  }

  virtual void deviceOnToMcu(WProperty* property) {
    if (!isReceivingDataFromMcu()) {
        //55 AA 00 06 00 05 01 01 00 01 01
        byte dt = (this->deviceOn->getBoolean() ? 0x01 : 0x00);
        unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
                                            byteDeviceOn, 0x01, 0x00, 0x01, dt};
        commandCharsToSerial(11, deviceOnCommand);
    }
  }

  void targetTemperatureManualModeToMcu() {
    if ((!isReceivingDataFromMcu()) && (schedulesMode->equalsString(SCHEDULES_MODE_OFF))) {
      network->debug(F("Set target Temperature (manual mode) to %D"), targetTemperatureManualMode);
      //55 AA 00 06 00 08 02 02 00 04 00 00 00 2C
      byte ulValues[4];
      WSettings::getUnsignedLongBytes((targetTemperatureManualMode * this->temperatureFactor), ulValues);
      unsigned char setTemperatureCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x08,
                                                byteTemperatureTarget, 0x02, 0x00, 0x04, ulValues[0], ulValues[1], ulValues[2], ulValues[3]};
      commandCharsToSerial(14, setTemperatureCommand);
    }
  }

  void schedulesModeToMcu(WProperty* property) {
    if ((!isReceivingDataFromMcu()) && (schedulesMode != nullptr)) {
      //55 AA 00 06 00 05 04 04 00 01 01
      byte sm = schedulesMode->getEnumIndex();
      if (sm != 0xFF) {
        unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
                                            byteSchedulesMode, 0x04, 0x00, 0x01, sm};
        commandCharsToSerial(11, deviceOnCommand);
      }
    }
  }

  void lockedToMcu(WProperty* property) {
    if (!isReceivingDataFromMcu()) {
      byte dt = (this->locked->getBoolean() ? 0x01 : 0x00);
      unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
                                          byteLocked, 0x01, 0x00, 0x01, dt};
      commandCharsToSerial(11, deviceOnCommand);
    }
  }

  virtual void schedulesToMcu() {
    if (receivedSchedules()) {
    	//Changed schedules from MQTT server, send to mcu
      //send the changed array to MCU
    	//per unit |MM HH TT|
    	//55 AA 00 06 00 3A 65 00 00 36|
    	//00 06 28|00 08 1E|1E 0B 1E|1E 0D 1E|00 11 2C|00 16 1E|
    	//00 06 28|00 08 28|1E 0B 28|1E 0D 28|00 11 28|00 16 1E|
    	//00 06 28|00 08 28|1E 0B 28|1E 0D 28|00 11 28|00 16 1E|
      int daysToSend = this->byteSchedulingDays;
			int functionLength = (daysToSend * 3);
    	unsigned char scheduleCommand[functionLength];
    	scheduleCommand[0] = 0x55;
    	scheduleCommand[1] = 0xaa;
    	scheduleCommand[2] = 0x00;
    	scheduleCommand[3] = 0x06;
    	scheduleCommand[4] = 0x00;
    	scheduleCommand[5] = (functionLength + 4);
    	scheduleCommand[6] = byteSchedules;
    	scheduleCommand[7] = 0x00;
    	scheduleCommand[8] = 0x00;
    	scheduleCommand[9] = functionLength;
    	for (int i = 0; i < functionLength; i++) {
    		scheduleCommand[i + 10] = schedules[i];
    	}
    	commandCharsToSerial(functionLength + 10, scheduleCommand);
    	//notify change
    	this->notifySchedules();
    }
  }

  void handleSchedulesChange(String completeTopic) {
    network->debug(F("Send Schedules state..."));
    if (completeTopic == "") {
      completeTopic = String(network->getMqttBaseTopic()) + SLASH + String(this->getId()) + SLASH + String(network->getMqttStateTopic()) + SLASH + SCHEDULES;
    }
    WStringStream* response = network->getResponseStream();
    WJson json(response);
    json.beginObject();
    this->toJsonSchedules(&json, 0);// SCHEDULE_WORKDAY);
    this->toJsonSchedules(&json, 1);// SCHEDULE_SATURDAY);
    this->toJsonSchedules(&json, 2);// SCHEDULE_SUNDAY);
    json.endObject();
    network->publishMqtt(completeTopic.c_str(), response);
  }

  void printConfigSchedulesPage(AsyncWebServerRequest* request, Print* page) {
    byte hh_Offset = this->byteSchedulingPosHour;
    byte mm_Offset = this->byteSchedulingPosMinute;
    page->printf(HTTP_CONFIG_PAGE_BEGIN, SCHEDULES);
    page->print(F("<table  class='st'>"));
    page->print(F("<tr>"));
    page->print(F("<th></th>"));
    page->print(F("<th>Weekday</th>"));
		if (this->byteSchedulingDays > 6) {
    	page->print(F("<th>Weekend 1</th>"));
		}
		if (this->byteSchedulingDays > 12) {
    	page->print(F("<th>Weekend 2</th>"));
		}
    page->print(F("</tr>"));
    for (byte period = 0; period < 6; period++) {
      page->print(F("<tr>"));
      page->printf("<td>Period %s</td>", String(period + 1).c_str());
      for (byte sd = 0; sd < 3; sd++) {
        int index = sd * 18 + period * 3;
				if (index < this->byteSchedulingDays * 3) {
        	char timeStr[6];
        	char keyH[4];
        	char keyT[4];
        	snprintf(keyH, 4, "%c%ch", SCHEDULES_DAYS[sd], SCHEDULES_PERIODS[period]);
        	snprintf(keyT, 4, "%c%ct", SCHEDULES_DAYS[sd], SCHEDULES_PERIODS[period]);
        	//hour
        	snprintf(timeStr, 6, "%02d:%02d", schedules[index + hh_Offset], schedules[index + mm_Offset]);

        	page->print(F("<td>"));
        	page->print(F("Time:"));
        	page->printf(HTTP_INPUT_FIELD, keyH, "5", timeStr);
        	//temp
        	String tempStr((double) schedules[index + 2]	/ this->temperatureFactor, 1);
        	page->print(F("Temp:"));
        	page->printf(HTTP_INPUT_FIELD, keyT, "4", tempStr.c_str());
        	page->print(F("</td>"));
				} else {
					break;
				}
      }
      page->print(F("</tr>"));
    }
    page->print(F("</table>"));
    page->print(FPSTR(HTTP_CONFIG_SAVE_BUTTON));
  }

  void submitConfigSchedulesPage(AsyncWebServerRequest* request, Print* page) {
    schedulesChanged = false;
    for (int period = 0; period < 6; period++) {
      for (int sd = 0; sd < 3; sd++) {
        char keyH[4];
        char keyT[4];
        snprintf(keyH, 4, "%c%ch", SCHEDULES_DAYS[sd], SCHEDULES_PERIODS[period]);
        snprintf(keyT, 4, "%c%ct", SCHEDULES_DAYS[sd], SCHEDULES_PERIODS[period]);
				if ((request->hasArg(keyH)) && (request->hasArg(keyT))) {
        	processSchedulesKeyValue(keyH, request->arg(keyH).c_str());
        	processSchedulesKeyValue(keyT, request->arg(keyT).c_str());
				}
      }
    }
    if (schedulesChanged) {
      network->debug(F("Some schedules changed. Write to MCU..."));
      this->schedulesToMcu();
      page->print(F("Changed schedules have been saved."));
    } else {
      page->print(F("Schedules have not changed."));
    }
  }

  void setTargetTemperature(WProperty* property) {
    if (!WProperty::isEqual(targetTemperatureManualMode, this->targetTemperature->getDouble(), 0.01)) {
      targetTemperatureManualMode = this->targetTemperature->getDouble();
      targetTemperatureManualModeToMcu();
      //schedulesMode->setString(SCHEDULES_MODE_OFF);
    }
  }

  void updateTargetTemperature() {
    if ((this->currentSchedulePeriod != -1) && (schedulesMode->equalsString(SCHEDULES_MODE_AUTO))) {
      double temp = (double) schedules[this->currentSchedulePeriod + 2] / this->temperatureFactor;
      targetTemperature->setDouble(temp);
    } else {
      targetTemperature->setDouble(targetTemperatureManualMode);
    }
  }

  bool receivedSchedules() {
    return ((network->isDebugging()) || (this->schedulesReceived));
  }

  void notifyState() {
    lastNotify = 0;
  }

  void notifySchedules() {
    lastScheduleNotify = 0;
  }

private :
  unsigned long lastNotify, lastScheduleNotify;
  bool schedulesChanged, schedulesReceived;
  int currentSchedulePeriod;


};

#endif
