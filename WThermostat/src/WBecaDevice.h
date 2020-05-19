#ifndef BECAMCU_H
#define	BECAMCU_H

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "WDevice.h"
#include "WClock.h"

#define COUNT_DEVICE_MODELS 4
#define MODEL_BHT_002_GBLW 0
#define MODEL_BAC_002_ALW 1
#define MODEL_ET_81_W 2
#define MODEL_FLOUREON_HY08WE 3

#define HEARTBEAT_INTERVAL 10000
#define MINIMUM_INTERVAL 2000
#define PIN_STATE_HEATING_RELAY 5
#define PIN_STATE_COOLING_RELAY 4

const unsigned char COMMAND_START[] = {0x55, 0xAA};
const char AR_COMMAND_END = '\n';
const char* SCHEDULES = "schedules";
const String LOGMCU = "logmcu";
const char* SCHEDULES_MODE_OFF = "off";
const char* SCHEDULES_MODE_AUTO = "auto";
const char* SCHEDULES_MODE_HOLIDAY = "holiday";
const char* SCHEDULES_MODE_HOLD = "hold";
const char* SYSTEM_MODE_COOL = "cool";
const char* SYSTEM_MODE_HEAT = "heat";
const char* SYSTEM_MODE_FAN = "fan_only";
const char* STATE_OFF = SCHEDULES_MODE_OFF;
const char* STATE_HEATING = "heating";
const char* STATE_COOLING = "cooling";
const char* FAN_MODE_AUTO = SCHEDULES_MODE_AUTO;
const char* FAN_MODE_LOW  = "low";
const char* FAN_MODE_MEDIUM  = "medium";
const char* FAN_MODE_HIGH = "high";

const byte STORED_FLAG_BECA = 0x36;
const char SCHEDULES_PERIODS[] = "123456";
const char SCHEDULES_DAYS[] = "wau";

const float MODEL_TEMPERATURE_FACTOR[]                  = { 2.0f, 2.0f, 10.0f, 10.0f };
const byte  MODEL_MCU_BYTE_TEMPERATURE_TARGET[]         = {0x02, 0x02, 0x02, 0x02 };
const byte  MODEL_MCU_BYTE_TEMPERATURE_ACTUAL[]         = {0x03, 0x03, 0x08, 0x03 };
const byte  MODEL_MCU_BYTE_TEMPERATURE_FLOOR[]          = {0x66, 0x00, 0x05, 0x66 };
const byte  MODEL_MCU_BYTE_SYSTEM_MODE[]  			        = {0x00, 0x66, 0x00, 0x00 };
const byte  MODEL_MCU_BYTE_SCHEDULES_MODE[]			        = {0x04, 0x04, 0x03, 0x04 };
const byte  MODEL_MCU_BYTE_FAN_MODE[]  					        = {0x00, 0x67, 0x00, 0x00 };
const byte  MODEL_MCU_BYTE_ECO_MODE[]  					        = {0x05, 0x05, 0x00, 0x00 };
const byte  MODEL_MCU_BYTE_SCHEDULES[]                  = {0x65, 0x68, 0x00, 0x00 };

class WBecaDevice: public WDevice {
public:
  typedef std::function<bool()> THandlerFunction;

  WBecaDevice(WNetwork* network, WClock* wClock)
    : WDevice(network, "thermostat", "thermostat", DEVICE_TYPE_THERMOSTAT) {
    this->logMcu = false;
    this->receivingDataFromMcu = false;
    this->schedulesChanged = false;
		this->schedulesReceived = false;
		this->targetTemperatureManualMode = 0.0;
    this->currentSchedulePeriod = -1;
    this->wClock = wClock;
    this->wClock->setOnTimeUpdate([this]() {
  		this->sendActualTimeToBeca();
  	});
    this->actualTemperature = WProperty::createTemperatureProperty("temperature", "Actual");
    this->actualTemperature->setReadOnly(true);
    this->addProperty(actualTemperature);
    this->targetTemperature = WProperty::createTargetTemperatureProperty("targetTemperature", "Target");
    this->targetTemperature->setOnChange(std::bind(&WBecaDevice::setTargetTemperature, this, std::placeholders::_1));
    this->targetTemperature->setOnValueRequest([this](WProperty* p) {updateTargetTemperature();});
    this->addProperty(targetTemperature);
    this->deviceOn = WProperty::createOnOffProperty("deviceOn", "Power");
    this->deviceOn->setOnChange(std::bind(&WBecaDevice::deviceOnToMcu, this, std::placeholders::_1));
    this->addProperty(deviceOn);
    //Model
    this->thermostatModel = network->getSettings()->setByte("thermostatModel", MODEL_BHT_002_GBLW);
    this->schedulesMode = new WProperty("schedulesMode", "Schedules", STRING, TYPE_THERMOSTAT_MODE_PROPERTY);
    if (getThermostatModel() != MODEL_ET_81_W) {
      this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
      this->schedulesMode->addEnumString(SCHEDULES_MODE_OFF);
    } else {
      this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLIDAY);
      this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
      this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLD);
    }
    this->switchBackToAuto = network->getSettings()->setBoolean("switchBackToAuto", true);
    this->schedulesMode->setOnChange(std::bind(&WBecaDevice::schedulesModeToMcu, this, std::placeholders::_1));
    this->addProperty(schedulesMode);
    if (MODEL_MCU_BYTE_ECO_MODE[getThermostatModel()] != 0x00) {
      this->ecoMode = WProperty::createOnOffProperty("ecoMode", "Eco");
    	this->ecoMode->setOnChange(std::bind(&WBecaDevice::ecoModeToMcu, this, std::placeholders::_1));
    	this->ecoMode->setVisibility(MQTT);
    	this->addProperty(ecoMode);
    } else {
      this->ecoMode = nullptr;
    }
    this->locked = WProperty::createOnOffProperty("locked", "Lock");
    this->locked->setOnChange(std::bind(&WBecaDevice::lockedToMcu, this, std::placeholders::_1));
    this->locked->setVisibility(MQTT);
    this->addProperty(locked);

		if (MODEL_MCU_BYTE_TEMPERATURE_FLOOR[getThermostatModel()] != 0x00) {
			this->actualFloorTemperature = WProperty::createTargetTemperatureProperty("floorTemperature", "Floor");
    	this->actualFloorTemperature->setReadOnly(true);
    	this->actualFloorTemperature->setVisibility(MQTT);
    	this->addProperty(actualFloorTemperature);
		} else {
      this->actualFloorTemperature = nullptr;
    }
		if (MODEL_MCU_BYTE_SYSTEM_MODE[getThermostatModel()] != 0x00) {
    	this->systemMode = new WProperty("systemMode", "System Mode", STRING, TYPE_THERMOSTAT_MODE_PROPERTY);
      this->systemMode->addEnumString(SYSTEM_MODE_COOL);
      this->systemMode->addEnumString(SYSTEM_MODE_HEAT);
      this->systemMode->addEnumString(SYSTEM_MODE_FAN);
			this->systemMode->setOnChange(std::bind(&WBecaDevice::systemModeToMcu, this, std::placeholders::_1));
      this->addProperty(systemMode);
		} else {
      this->systemMode = nullptr;
    }
		if (MODEL_MCU_BYTE_FAN_MODE[getThermostatModel()] != 0x00) {
    	this->fanMode = new WProperty("fanMode", "Fan", STRING, TYPE_FAN_MODE_PROPERTY);
      this->fanMode->addEnumString(FAN_MODE_AUTO);
      this->fanMode->addEnumString(FAN_MODE_HIGH);
      this->fanMode->addEnumString(FAN_MODE_MEDIUM);
      this->fanMode->addEnumString(FAN_MODE_LOW);
			this->fanMode->setOnChange(std::bind(&WBecaDevice::fanModeToMcu, this, std::placeholders::_1));
      this->addProperty(fanMode);
    } else {
      this->fanMode = nullptr;
    }
    //Heating Relay and State property
    this->state = nullptr;
    this->supportingHeatingRelay = network->getSettings()->setBoolean("supportingHeatingRelay", true);
    this->supportingCoolingRelay = network->getSettings()->setBoolean("supportingCoolingRelay", false);
    if (isSupportingHeatingRelay()) pinMode(PIN_STATE_HEATING_RELAY, INPUT);
    if (isSupportingCoolingRelay()) pinMode(PIN_STATE_COOLING_RELAY, INPUT);
    if ((isSupportingHeatingRelay()) || (isSupportingCoolingRelay())) {
    	this->state = new WProperty("state", "State", STRING, TYPE_HEATING_COOLING_PROPERTY);
    	this->state->setReadOnly(true);
    	this->state->addEnumString(STATE_OFF);
    	this->state->addEnumString(STATE_HEATING);
    	this->state->addEnumString(STATE_COOLING);
    	this->addProperty(state);
    }
		this->targetTemperature->setMultipleOf(1.0f / getTemperatureFactor());
		this->completeDeviceState = network->getSettings()->setBoolean("sendCompleteDeviceState", true);
    //schedulesDayOffset
    this->schedulesDayOffset = network->getSettings()->setByte("schedulesDayOffset", 0);
    //HtmlPages
    WPage* configPage = new WPage(this->getId(), "Configure thermostat");
    configPage->setPrintPage(std::bind(&WBecaDevice::printConfigPage, this, std::placeholders::_1, std::placeholders::_2));
    configPage->setSubmittedPage(std::bind(&WBecaDevice::submitConfigPage, this, std::placeholders::_1, std::placeholders::_2));
    network->addCustomPage(configPage);
    WPage* schedulesPage = new WPage(SCHEDULES, "Configure schedules");
    schedulesPage->setPrintPage(std::bind(&WBecaDevice::printConfigSchedulesPage, this, std::placeholders::_1, std::placeholders::_2));
    schedulesPage->setSubmittedPage(std::bind(&WBecaDevice::submitConfigSchedulesPage, this, std::placeholders::_1, std::placeholders::_2));
    network->addCustomPage(schedulesPage);

    lastHeartBeat = lastNotify = lastScheduleNotify = 0;
    resetAll();
  }

  virtual bool isProvidingConfigPage() {
    return true;
  }

  virtual void printConfigPage(ESP8266WebServer* webServer, WStringStream* page) {
    	network->notice(F("Beca thermostat config page"));
    	page->printAndReplace(FPSTR(HTTP_CONFIG_PAGE_BEGIN), getId());
    	//ComboBox with model selection
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_BEGIN), "Thermostat model:", "tm");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "0", (getThermostatModel() == 0 ? HTTP_SELECTED : ""), "Floor heating (BHT-002-GBLW)");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "1", (getThermostatModel() == 1 ? HTTP_SELECTED : ""), "Heating, Cooling, Ventilation (BAC-002-ALW)");
			page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "2", (getThermostatModel() == 2 ? HTTP_SELECTED : ""), "Floor heating (ET-81W)");
			page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "3", (getThermostatModel() == 3 ? HTTP_SELECTED : ""), "Floor heating (Floureon HY08WE)");
    	page->print(FPSTR(HTTP_COMBOBOX_END));
      //Checkbox
      page->printAndReplace(FPSTR(HTTP_CHECKBOX_OPTION), "sb", "sb", (this->switchBackToAuto->getBoolean() ? HTTP_CHECKED : ""), "", "Auto mode from manual mode at next schedule period change (not at model ET-81W)");
      //Checkbox with support for relay
			page->printAndReplace(FPSTR(HTTP_CHECKBOX_OPTION), "rs", "rs", (this->isSupportingHeatingRelay() ? HTTP_CHECKED : ""), "", "Relay at GPIO 5 *");
    	//ComboBox with weekday
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_BEGIN), "Workday schedules:", "ws");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "0", (getSchedulesDayOffset() == 0 ? HTTP_SELECTED : ""), "Workday Mon-Fri; Weekend Sat-Sun");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "1", (getSchedulesDayOffset() == 1 ? HTTP_SELECTED : ""), "Workday Sun-Thu; Weekend Fri-Sat");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "2", (getSchedulesDayOffset() == 2 ? HTTP_SELECTED : ""), "Workday Sat-Wed; Weekend Thu-Fri");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "3", (getSchedulesDayOffset() == 3 ? HTTP_SELECTED : ""), "Workday Fri-Tue; Weekend Wed-Thu");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "4", (getSchedulesDayOffset() == 4 ? HTTP_SELECTED : ""), "Workday Thu-Mon; Weekend Tue-Wed");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "5", (getSchedulesDayOffset() == 5 ? HTTP_SELECTED : ""), "Workday Wed-Sun; Weekend Mon-Tue");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "6", (getSchedulesDayOffset() == 6 ? HTTP_SELECTED : ""), "Workday Tue-Sat; Weekend Sun-Mon");
    	page->print(FPSTR(HTTP_COMBOBOX_END));

			page->printAndReplace(FPSTR(HTTP_CHECKBOX_OPTION), "cr", "cr", (this->sendCompleteDeviceState() ? "" : HTTP_CHECKED), "", "Send changes in separate MQTT messages");

    	page->print(FPSTR(HTTP_CONFIG_SAVE_BUTTON));
    }

    void submitConfigPage(ESP8266WebServer* webServer, WStringStream* page) {
        network->notice(F("Save Beca config page"));
        this->thermostatModel->setByte(webServer->arg("tm").toInt());
        this->schedulesDayOffset->setByte(webServer->arg("ws").toInt());
        this->supportingHeatingRelay->setBoolean(webServer->arg("rs") == HTTP_TRUE);
        this->switchBackToAuto->setBoolean(webServer->arg("sb") == HTTP_TRUE);
				this->completeDeviceState->setBoolean(webServer->arg("cr") != HTTP_TRUE);
    }

    void loop(unsigned long now) {
    	if (state != nullptr) {
    		bool heating = false;
    		bool cooling = false;
    		if ((isSupportingHeatingRelay()) && (state != nullptr)) {
    			heating = digitalRead(PIN_STATE_HEATING_RELAY);
    		}
    		if ((isSupportingCoolingRelay()) && (state != nullptr)) {
    			cooling = digitalRead(PIN_STATE_COOLING_RELAY);
    		}
    		this->state->setString(heating ? STATE_HEATING : (cooling ? STATE_COOLING : STATE_OFF));
    	}
    	while (Serial.available() > 0) {
    		receiveIndex++;
    		unsigned char inChar = Serial.read();
    		receivedCommand[receiveIndex] = inChar;
    		if (receiveIndex < 2) {
    			//Check command start
    			if (COMMAND_START[receiveIndex] != receivedCommand[receiveIndex]) {
    				resetAll();
    			}
    		} else if (receiveIndex == 5) {
    			//length information now available
    			commandLength = receivedCommand[4] * 0x100 + receivedCommand[5];
    		} else if ((commandLength > -1)
    				&& (receiveIndex == (6 + commandLength))) {
    			//verify checksum
    			int expChecksum = 0;
    			for (int i = 0; i < receiveIndex; i++) {
    				expChecksum += receivedCommand[i];
    			}
    			expChecksum = expChecksum % 0x100;
    			if (expChecksum == receivedCommand[receiveIndex]) {
    				processSerialCommand();
    			}
    			resetAll();
    		}
    	}
      //
      updateCurrentSchedulePeriod();
    	//Heartbeat
    	//long now = millis();
    	if ((HEARTBEAT_INTERVAL > 0)
    			&& ((lastHeartBeat == 0)
    					|| (now - lastHeartBeat > HEARTBEAT_INTERVAL))) {
    		unsigned char heartBeatCommand[] =
    				{ 0x55, 0xAA, 0x00, 0x00, 0x00, 0x00 };
    		commandCharsToSerial(6, heartBeatCommand);
    		//commandHexStrToSerial("55 aa 00 00 00 00");
    		lastHeartBeat = now;
    	}
    	if (receivedSchedules()) {
    		//Notify schedules
    		if ((lastScheduleNotify == 0) && (now - lastScheduleNotify > MINIMUM_INTERVAL)) {
					handleSchedulesChange("");
    			lastScheduleNotify = now;
    		}
    	}
    }

    unsigned char* getCommand() {
    	return receivedCommand;
    }

    int getCommandLength() {
    	return commandLength;
    }

    String getCommandAsString() {
    	String result = "";
    	if (commandLength > -1) {
    		for (int i = 0; i < 6 + commandLength; i++) {
    			unsigned char ch = receivedCommand[i];
    			result = result + (ch < 16 ? "0" : "") + String(ch, HEX);// charToHexStr(ch);
    			if (i + 1 < 6 + commandLength) {
    				result = result + " ";
    			}
    		}
    	}
    	return result;
    }

    void commandHexStrToSerial(String command) {
    	command.trim();
    	command.replace(" ", "");
    	command.toLowerCase();
    	int chkSum = 0;
    	if ((command.length() > 1) && (command.length() % 2 == 0)) {
    		for (int i = 0; i < (command.length() / 2); i++) {
    			unsigned char chValue = getIndex(command.charAt(i * 2)) * 0x10
    					+ getIndex(command.charAt(i * 2 + 1));
    			chkSum += chValue;
    			Serial.print((char) chValue);
    		}
    		unsigned char chValue = chkSum % 0x100;
    		Serial.print((char) chValue);
    	}
    }

    void commandCharsToSerial(unsigned int length, unsigned char* command) {
    	int chkSum = 0;
    	if (length > 2) {
    		for (int i = 0; i < length; i++) {
    			unsigned char chValue = command[i];
    			chkSum += chValue;
    			Serial.print((char) chValue);
    		}
    		unsigned char chValue = chkSum % 0x100;
    		Serial.print((char) chValue);
    	}
    }

    void queryState() {
    	//55 AA 00 08 00 00
    	unsigned char queryStateCommand[] = { 0x55, 0xAA, 0x00, 0x08, 0x00, 0x00 };
    	commandCharsToSerial(6, queryStateCommand);
    }

    void cancelConfiguration() {
    	unsigned char cancelConfigCommand[] = { 0x55, 0xaa, 0x00, 0x03, 0x00, 0x01,
    			0x02 };
    	commandCharsToSerial(7, cancelConfigCommand);
    }

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

    void bindWebServerCalls(ESP8266WebServer* webServer) {
    	String deviceBase("/things/");
    	deviceBase.concat(getId());
    	deviceBase.concat("/");
    	deviceBase.concat(SCHEDULES);
    	webServer->on(deviceBase.c_str(), HTTP_GET, std::bind(&WBecaDevice::sendSchedules, this, webServer));
    }

    void handleUnknownMqttCallback(bool getState, String completeTopic, String partialTopic, char *payload, unsigned int length) {
    	if (partialTopic.startsWith(SCHEDULES)) {
        if (MODEL_MCU_BYTE_SCHEDULES[getThermostatModel()] != 0x00) {
    		  partialTopic = partialTopic.substring(strlen(SCHEDULES) + 1);
				  if (getState) {
					  //Send actual schedules
					  handleSchedulesChange(completeTopic);
				  } else if (length > 0) {
					  //Set schedules
					  network->notice(F("Payload for schedules -> set schedules..."));
					  WJsonParser* parser = new WJsonParser();
					  schedulesChanged = false;
					  parser->parse(payload, std::bind(&WBecaDevice::processSchedulesKeyValue, this,
						  		std::placeholders::_1, std::placeholders::_2));
					  delete parser;
					  if (schedulesChanged) {
						  network->notice(F("Some schedules changed. Write to MCU..."));
						  this->schedulesToMcu();
					  }
				  }
        }
    	} else if (partialTopic.startsWith(LOGMCU)) {
				if ((!getState) && (length > 1)) {
					this->setLogMcu(strcmp(HTTP_TRUE, payload) == 0);
				}
			}
    }

    void processSchedulesKeyValue(const char* key, const char* value) {
    	network->notice(F("Process key '%s', value '%s'"), key, value);
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
    			if (key[2] == 'h') {
    				//hour
    				String timeStr = String(value);
    				timeStr = (timeStr.length() == 4 ? "0" + timeStr : timeStr);
            if (timeStr.length() == 5) {
    				  byte hh = timeStr.substring(0, 2).toInt();
    				  byte mm = timeStr.substring(3, 5).toInt();
    				  schedulesChanged = schedulesChanged || (schedules[startAddr + period * 3 + 1] != hh);
    				  schedules[startAddr + period * 3 + 1] = hh;
    				  schedulesChanged = schedulesChanged || (schedules[startAddr + period * 3 + 0] != mm);
    				  schedules[startAddr + period * 3 + 0] = mm;
            }
    			} else if (key[2] == 't') {
    				//temperature
            //it will fail, when temperature needs 2 bytes
    				int tt = (int) (atof(value) * getTemperatureFactor());
            if (tt < 0xFF) {
    				  schedulesChanged = schedulesChanged || (schedules[startAddr + period * 3 + 2] != tt);
    				  schedules[startAddr + period * 3 + 2] = tt;
            }
    			}
    		}
    	}
    }

    void sendSchedules(ESP8266WebServer* webServer) {
    	WStringStream* response = network->getResponseStream();
    	WJson json(response);
    	json.beginObject();
    	this->toJsonSchedules(&json, 0);// SCHEDULE_WORKDAY);
    	this->toJsonSchedules(&json, 1);// SCHEDULE_SATURDAY);
    	this->toJsonSchedules(&json, 2);// SCHEDULE_SUNDAY);
    	json.endObject();
    	webServer->send(200, APPLICATION_JSON, response->c_str());
    }

    virtual void toJsonSchedules(WJson* json, byte schedulesDay) {
    	byte startAddr = 0;
		char dayChar = SCHEDULES_DAYS[0];
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
    		buffer[1] = SCHEDULES_PERIODS[i];
    		sprintf(timeStr, "%02d:%02d", schedules[startAddr + i * 3 + 1], schedules[startAddr + i * 3 + 0]);
    		buffer[2] = 'h';
    		json->propertyString(buffer, timeStr);
    		buffer[2] = 't';
    		json->propertyDouble(buffer, (double) schedules[startAddr + i * 3 + 2]	/ getTemperatureFactor());
    	}
    	delete[] buffer;
    }

    void setOnConfigurationRequest(THandlerFunction onConfigurationRequest) {
    	this->onConfigurationRequest = onConfigurationRequest;
    }

    void schedulesToMcu() {
    	if (receivedSchedules()) {
    		//Changed schedules from MQTT server, send to mcu
    		//send the changed array to MCU
    		//per unit |MM HH TT|
    		//55 AA 00 06 00 3A 65 00 00 36|
    		//00 06 28|00 08 1E|1E 0B 1E|1E 0D 1E|00 11 2C|00 16 1E|
    		//00 06 28|00 08 28|1E 0B 28|1E 0D 28|00 11 28|00 16 1E|
    		//00 06 28|00 08 28|1E 0B 28|1E 0D 28|00 11 28|00 16 1E|
    		unsigned char scheduleCommand[64];
    		scheduleCommand[0] = 0x55;
    		scheduleCommand[1] = 0xaa;
    		scheduleCommand[2] = 0x00;
    		scheduleCommand[3] = 0x06;
    		scheduleCommand[4] = 0x00;
    		scheduleCommand[5] = 0x3a;
    		scheduleCommand[6] = MODEL_MCU_BYTE_SCHEDULES[getThermostatModel()];
    		scheduleCommand[7] = 0x00;
    		scheduleCommand[8] = 0x00;
    		scheduleCommand[9] = 0x36;
    		for (int i = 0; i < 54; i++) {
    			scheduleCommand[i + 10] = schedules[i];
    		}
    		commandCharsToSerial(64, scheduleCommand);
    		//notify change
    		this->notifySchedules();
    	}
    }

    void fanModeToMcu(WProperty* property) {
    	if ((!this->receivingDataFromMcu) && (fanMode != nullptr)) {
        byte fm = fanMode->getEnumIndex();
    		if (fm != 0xFF) {
    			//send to device
    		  //auto:   55 aa 00 06 00 05 67 04 00 01 00
    			//high:   55 aa 00 06 00 05 67 04 00 01 01
          //medium: 55 aa 00 06 00 05 67 04 00 01 02
    			//low:    55 aa 00 06 00 05 67 04 00 01 03
    			unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
    			                                    0x67, 0x04, 0x00, 0x01, fm};
    			commandCharsToSerial(11, deviceOnCommand);
    		}
    	}
    }

    void systemModeToMcu(WProperty* property) {
    	if ((!this->receivingDataFromMcu) && (systemMode != nullptr)) {
    		byte sm = systemMode->getEnumIndex();
    		if (sm != 0xFF) {
    			//send to device
    			//cooling:     55 AA 00 06 00 05 66 04 00 01 00
    			//heating:     55 AA 00 06 00 05 66 04 00 01 01
    			//ventilation: 55 AA 00 06 00 05 66 04 00 01 02
    			unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
    												0x66, 0x04, 0x00, 0x01, sm};
    			commandCharsToSerial(11, deviceOnCommand);
    		}
    	}
    }

    void setLogMcu(bool logMcu) {
    	if (this->logMcu != logMcu) {
    		this->logMcu = logMcu;
    		notifyState();
    	}
    }

		bool sendCompleteDeviceState() {
			return completeDeviceState->getBoolean();
		}

    byte getSchedulesDayOffset() {
    	return schedulesDayOffset->getByte();
    }

protected:

    byte getThermostatModel() {
    	return this->thermostatModel->getByte();
    }

    bool isSupportingHeatingRelay() {
        return this->supportingHeatingRelay->getBoolean();
    }

    bool isSupportingCoolingRelay() {
        return this->supportingCoolingRelay->getBoolean();
    }

		float getTemperatureFactor() {
			//return 2.0f;
			return MODEL_TEMPERATURE_FACTOR[getThermostatModel()];
		}

private:
    WClock *wClock;
    int receiveIndex;
    int commandLength;
    long lastHeartBeat;
    unsigned char receivedCommand[1024];
    bool logMcu;
    boolean receivingDataFromMcu;
    double targetTemperatureManualMode;
    WProperty* deviceOn;
    WProperty* state;
    WProperty* targetTemperature;
    WProperty* actualTemperature;
    WProperty* actualFloorTemperature;
    WProperty* schedulesMode;
    WProperty* systemMode;
    WProperty* fanMode;
    WProperty* ecoMode;
    WProperty* locked;
    byte schedules[54];
    WProperty* thermostatModel;
    WProperty *supportingHeatingRelay;
    WProperty *supportingCoolingRelay;
    WProperty* ntpServer;
    WProperty* schedulesDayOffset;
		WProperty *completeDeviceState;
    WProperty* switchBackToAuto;
    THandlerFunction onConfigurationRequest;
    unsigned long lastNotify, lastScheduleNotify;
    bool schedulesChanged, schedulesReceived;
    int currentSchedulePeriod;

    int getIndex(unsigned char c) {
    	const char HEX_DIGITS[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8',
    			'9', 'a', 'b', 'c', 'd', 'e', 'f' };
    	int result = -1;
    	for (int i = 0; i < 16; i++) {
    		if (c == HEX_DIGITS[i]) {
    			result = i;
    			break;
    		}
    	}
    	return result;
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

    void notifyKnownCommand(const char* commandType) {
			if (logMcu) {
        network->debug(commandType, this->getCommandAsString().c_str());
    	}
    }

		void notifyUnknownCommand() {
			network->error("Unknown MCU command", this->getCommandAsString().c_str());
    }

    void processSerialCommand() {
    	if (commandLength > -1) {
    		//unknown
    		//55 aa 00 00 00 00
    		this->receivingDataFromMcu = true;

    		if (receivedCommand[3] == 0x00) {
    			switch (receivedCommand[6]) {
    			case 0x00:
    			case 0x01:
    				//ignore, heartbeat MCU
    				//55 aa 01 00 00 01 01
    				//55 aa 01 00 00 01 00
    				break;
    			//default:
    				//notifyUnknownCommand();
    			}
    		} else if (receivedCommand[3] == 0x03) {
    			//ignore, MCU response to wifi state
    			//55 aa 01 03 00 00
    		} else if (receivedCommand[3] == 0x04) {
    			//Setup initialization request
    			//received: 55 aa 01 04 00 00
    			if (onConfigurationRequest) {
    				//send answer: 55 aa 00 03 00 01 00
    				unsigned char configCommand[] = { 0x55, 0xAA, 0x00, 0x03, 0x00,
    						0x01, 0x00 };
    				commandCharsToSerial(7, configCommand);
    				onConfigurationRequest();
    			}

    		} else if (receivedCommand[3] == 0x07) {
					//Status report from MCU
    			bool changed = false;
    			bool schedulesChangedMCU = false;
    			bool newB;
          const char* newS;
    			float newValue;
    			byte newByte;
    			byte commandLength = receivedCommand[5];
    			bool knownCommand = false;
					byte cByte = receivedCommand[6];
					byte thModel = this->getThermostatModel();
    			if (cByte == 0x01 ) {
    				if (commandLength == 0x05) {
    					//device On/Off
    					//55 aa 00 06 00 05 01 01 00 01 00|01
    					newB = (receivedCommand[10] == 0x01);
    					changed = ((changed) || (newB != deviceOn->getBoolean()));
    					deviceOn->setBoolean(newB);
    					notifyKnownCommand("deviceOn %s");
    					knownCommand = true;
    				}
    			} else if (cByte == MODEL_MCU_BYTE_TEMPERATURE_TARGET[thModel]) {
    				if (commandLength == 0x08) {
    					//target Temperature for manual mode
    					//e.g. 24.5C: 55 aa 01 07 00 08 02 02 00 04 00 00 00 31
							unsigned long rawValue = WSettings::getUnsignedLong(receivedCommand[10], receivedCommand[11], receivedCommand[12], receivedCommand[13]);
    					newValue = (float) rawValue / getTemperatureFactor();
    					changed = ((changed) || (WProperty::isEqual(targetTemperatureManualMode, newValue, 0.01)));
    					targetTemperatureManualMode = newValue;
							if (changed) updateTargetTemperature();
    					notifyKnownCommand("targetTemperature %s");
    					knownCommand = true;
    				}
					} else if (cByte == MODEL_MCU_BYTE_TEMPERATURE_ACTUAL[thModel]) {
    				if (commandLength == 0x08) {
    					//actual Temperature
    					//e.g. 23C: 55 aa 01 07 00 08 03 02 00 04 00 00 00 2e
							unsigned long rawValue = WSettings::getUnsignedLong(receivedCommand[10], receivedCommand[11], receivedCommand[12], receivedCommand[13]);
    					newValue = (float) rawValue / getTemperatureFactor();
    					changed = ((changed) || (!actualTemperature->equalsDouble(newValue)));
    					actualTemperature->setDouble(newValue);
    					notifyKnownCommand("actualTemperature %s");
    					knownCommand = true;
    				}
    			} else if ((cByte == MODEL_MCU_BYTE_SCHEDULES_MODE[thModel]) && (schedulesMode != nullptr)) {
    				if (commandLength == 0x05) {
    					//schedulesMode?
              newS = schedulesMode->getEnumString(receivedCommand[10]);
              if (newS != nullptr) {
    					  changed = ((changed) || (schedulesMode->setString(newS)));
							  if (changed) updateTargetTemperature();
    					  notifyKnownCommand("schedulesMode %s");
    					  knownCommand = true;
              }
    				}
    			} else if ((cByte == MODEL_MCU_BYTE_ECO_MODE[thModel]) && (ecoMode != nullptr)) {
    				if (commandLength == 0x05) {
    					//ecoMode
    					newB = (receivedCommand[10] == 0x01);
    					changed = ((changed) || (newB != ecoMode->getBoolean()));
    					ecoMode->setBoolean(newB);
    					notifyKnownCommand("ecoMode %s");
    					knownCommand = true;
    				}
    			} else if (cByte == 0x06) {
    				if (commandLength == 0x05) {
    					//locked
    					newB = (receivedCommand[10] == 0x01);
    					changed = ((changed) || (newB != locked->getBoolean()));
    					locked->setBoolean(newB);
    					notifyKnownCommand("locked %s");
    					knownCommand = true;
    				}
    			} else if (cByte == MODEL_MCU_BYTE_SCHEDULES[thModel]) {
    				if (commandLength == 0x3A) {
    					//schedules 0x65 at heater model, 0x68 at fan model, example
    					//55 AA 00 06 00 3A 65 00 00 36
    					//00 07 28 00 08 1E 1E 0B 1E 1E 0D 1E 00 11 2C 00 16 1E
    					//00 06 28 00 08 28 1E 0B 28 1E 0D 28 00 11 28 00 16 1E
    					//00 06 28 00 08 28 1E 0B 28 1E 0D 28 00 11 28 00 16 1E
    					this->schedulesReceived = true;
    					for (int i = 0; i < 54; i++) {
    						newByte = receivedCommand[i + 10];
    						schedulesChangedMCU = ((schedulesChangedMCU) || (newByte != schedules[i]));
    						schedules[i] = newByte;
    					}
    					notifyKnownCommand("schedules %s");
    					knownCommand = true;
    				}
					} else if (cByte == 0x68) {
						if (receivedCommand[5] == 0x05) {
    					//Unknown permanently sent from MCU
    					//55 aa 01 07 00 05 68 01 00 01 01
    					knownCommand = true;
    				}
    			} else if ((cByte == MODEL_MCU_BYTE_TEMPERATURE_FLOOR[thModel]) && (actualFloorTemperature != nullptr)) {
    				if (commandLength == 0x08) {
    					//MODEL_BHT_002_GBLW - actualFloorTemperature
    					//55 aa 01 07 00 08 66 02 00 04 00 00 00 00
							unsigned long rawValue = WSettings::getUnsignedLong(receivedCommand[10], receivedCommand[11], receivedCommand[12], receivedCommand[13]);
    					newValue = (float) rawValue / getTemperatureFactor();
    					if (actualFloorTemperature != nullptr) {
    						changed = ((changed) || (!actualFloorTemperature->equalsDouble(newValue)));
    						actualFloorTemperature->setDouble(newValue);
    					}
    					notifyKnownCommand("actualFloorTemperature %s");
    					knownCommand = true;
    				}
					} else if ((cByte == MODEL_MCU_BYTE_SYSTEM_MODE[thModel]) && (systemMode != nullptr)) {
						 if (commandLength == 0x05) {
    					//MODEL_BAC_002_ALW - systemMode
    					//cooling:     55 AA 00 06 00 05 66 04 00 01 00
    					//heating:     55 AA 00 06 00 05 66 04 00 01 01
    					//ventilation: 55 AA 00 06 00 05 66 04 00 01 02
    					newS = systemMode->getEnumString(receivedCommand[10]);
              if (newS != nullptr) {
                changed = ((changed) || (systemMode->setString(newS)));
                notifyKnownCommand("systemMode %s");
      					knownCommand = true;
              }
    				}
					} else if ((cByte == MODEL_MCU_BYTE_FAN_MODE[thModel]) && (fanMode != nullptr)) {
    				if (commandLength == 0x05) {
    					//fanMode
    					//auto   - 55 aa 01 07 00 05 67 04 00 01 00
              //high   - 55 aa 01 07 00 05 67 04 00 01 01
              //medium - 55 aa 01 07 00 05 67 04 00 01 02
    					//low    - 55 aa 01 07 00 05 67 04 00 01 03
              newS = fanMode->getEnumString(receivedCommand[10]);
              if (newS != nullptr) {
                changed = ((changed) || (fanMode->setString(newS)));
                notifyKnownCommand("fanMode %s");
      					knownCommand = true;
              }
    				}
    			}
    			if (!knownCommand) {
    				notifyUnknownCommand();
    			} else if (changed) {
    				notifyState();
    			} else if (schedulesChangedMCU) {
    				notifySchedules();
    			}

    		} else if (receivedCommand[3] == 0x1C) {
    			//Request for time sync from MCU : 55 aa 01 1c 00 00
    			this->sendActualTimeToBeca();
    		} else {
    			notifyUnknownCommand();
    		}
    		this->receivingDataFromMcu = false;
    	}
    }

    void deviceOnToMcu(WProperty* property) {
    	if (!this->receivingDataFromMcu) {
       		//55 AA 00 06 00 05 01 01 00 01 01
       		byte dt = (this->deviceOn->getBoolean() ? 0x01 : 0x00);
       		unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
       		                                    0x01, 0x01, 0x00, 0x01, dt};
       		commandCharsToSerial(11, deviceOnCommand);
       		//notifyState();
     	}
    }

    void updateTargetTemperature() {
      if ((this->currentSchedulePeriod != -1) && (schedulesMode->equalsString(SCHEDULES_MODE_AUTO))) {
        double temp = (double) schedules[this->currentSchedulePeriod + 2] / getTemperatureFactor();
    		network->notice(F("Schedule temperature is: %D"), temp);
    		targetTemperature->setDouble(temp);
      } else {
        targetTemperature->setDouble(targetTemperatureManualMode);
      }
    }

    void updateCurrentSchedulePeriod() {
    	if ((receivedSchedules()) && (wClock->isValidTime())) {
    		byte weekDay = wClock->getWeekDay();
    		weekDay += schedulesDayOffset->getByte();
    		weekDay = weekDay % 7;
    		int startAddr = (weekDay == 0 ? 36 : (weekDay == 6 ? 18 : 0));
    		int period = 0;
    		if (wClock->isTimeEarlierThan(schedules[startAddr + period * 3 + 1], schedules[startAddr + period * 3 + 0])) {
    			//Jump back to day before and last schedule of day
    			weekDay = weekDay - 1;
    			weekDay = weekDay % 7;
    			startAddr = (weekDay == 0 ? 36 : (weekDay == 6 ? 18 : 0));
    			period = 5;
    		} else {
    			//check the schedules in same day
    			for (int i = 1; i < 6; i++) {
    				if (i < 5) {
    					if (wClock->isTimeBetween(schedules[startAddr + i * 3 + 1], schedules[startAddr + i * 3 + 0],
    							                  schedules[startAddr + (i + 1) * 3 + 1], schedules[startAddr + (i + 1) * 3 + 0])) {
    						period = i;
    						break;
    					}
    				} else if (wClock->isTimeLaterThan(schedules[startAddr + 5 * 3 + 1], schedules[startAddr + 5 * 3 + 0])) {
    					period = 5;
    				}
    			}
    		}
        int newPeriod = startAddr + period * 3;
        if ((getThermostatModel() != MODEL_ET_81_W) && (this->switchBackToAuto->getBoolean()) &&
            (this->currentSchedulePeriod > -1) && (newPeriod != this->currentSchedulePeriod) &&
            (this->schedulesMode->equalsString(SCHEDULES_MODE_OFF))) {
          this->schedulesMode->setString(SCHEDULES_MODE_AUTO);
        }
        this->currentSchedulePeriod = newPeriod;
    	} else {
        this->currentSchedulePeriod = -1;
    	}
    }

    void setTargetTemperature(WProperty* property) {
    	if (!WProperty::isEqual(targetTemperatureManualMode, this->targetTemperature->getDouble(), 0.01)) {
    		targetTemperatureManualMode = this->targetTemperature->getDouble();
    		targetTemperatureManualModeToMcu();
    		schedulesMode->setString(SCHEDULES_MODE_OFF);
    	}
    }

    void targetTemperatureManualModeToMcu() {
    	if (!this->receivingDataFromMcu) {
    		network->notice(F("Set target Temperature (manual mode) to %D"), targetTemperatureManualMode);
    	    //55 AA 00 06 00 08 02 02 00 04 00 00 00 2C
					byte ulValues[4];
					WSettings::getUnsignedLongBytes((targetTemperatureManualMode * getTemperatureFactor()), ulValues);
    	    unsigned char setTemperatureCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x08,
    	    		0x02, 0x02, 0x00, 0x04,
							ulValues[0], ulValues[1], ulValues[2], ulValues[3]};
    	    commandCharsToSerial(14, setTemperatureCommand);
    	}
    }

    void schedulesModeToMcu(WProperty* property) {
    	if ((!this->receivingDataFromMcu) && (schedulesMode != nullptr)) {
        	//55 AA 00 06 00 05 04 04 00 01 01
          byte sm = schedulesMode->getEnumIndex();
          if (sm != 0xFF) {
        	  unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
        	                                     0x04, 0x04, 0x00, 0x01, sm};
        	  commandCharsToSerial(11, deviceOnCommand);
          }
        }
    }

    void ecoModeToMcu(WProperty* property) {
       	if ((!this->receivingDataFromMcu) && (ecoMode != nullptr)) {
       		//55 AA 00 06 00 05 05 01 00 01 01
       		byte dt = (this->ecoMode->getBoolean() ? 0x01 : 0x00);
       		unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
       		                                    0x05, 0x01, 0x00, 0x01, dt};
       		commandCharsToSerial(11, deviceOnCommand);
       		//notifyState();
       	}
    }

    void lockedToMcu(WProperty* property) {
       	if (!this->receivingDataFromMcu) {
       		//55 AA 00 06 00 05 06 01 00 01 01
       		byte dt = (this->locked->getBoolean() ? 0x01 : 0x00);
       		unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
       		                                    0x06, 0x01, 0x00, 0x01, dt};
       		commandCharsToSerial(11, deviceOnCommand);
       		//notifyState();
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

    void resetAll() {
       	receiveIndex = -1;
       	commandLength = -1;
    }

		void handleSchedulesChange(String completeTopic) {
			network->notice(F("Send Schedules state..."));
			if (completeTopic == "") {
				completeTopic = String(network->getMqttBaseTopic()) + SLASH + String(this->getId()) + SLASH + String(network->getMqttStateTopic() + SLASH + SCHEDULES);
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

    void printConfigSchedulesPage(ESP8266WebServer* webServer, WStringStream* page) {
      network->notice(F("Schedules config page"));
			page->printAndReplace(FPSTR(HTTP_CONFIG_PAGE_BEGIN), SCHEDULES);
			page->print(F("<table  class='settingstable'>"));
      page->print(F("<tr>"));
        page->print(F("<th></th>"));
        page->print(F("<th>Weekday</th>"));
        page->print(F("<th>Weekend 1</th>"));
        page->print(F("<th>Weekend 2</th>"));
      page->print(F("</tr>"));
      for (byte period = 0; period < 6; period++) {
        page->print(F("<tr>"));
        page->printAndReplace(F("<td>Period %s</td>"), String(period + 1).c_str());
        for (byte sd = 0; sd < 3; sd++) {
          int index = sd * 18 + period * 3;
          char timeStr[6];
          char keyH[4];
          char keyT[4];
					snprintf(keyH, 4, "%c%ch", SCHEDULES_DAYS[sd], SCHEDULES_PERIODS[period]);
					snprintf(keyT, 4, "%c%ct", SCHEDULES_DAYS[sd], SCHEDULES_PERIODS[period]);
          //hour
          snprintf(timeStr, 6, "%02d:%02d", schedules[index + 1], schedules[index + 0]);
          page->print(F("<td>"));
          page->print(F("Time:"));
          page->printAndReplace(FPSTR(HTTP_INPUT_FIELD), keyH, "5", timeStr);
          //temp
          String tempStr((double) schedules[index + 2]	/ getTemperatureFactor(), 1);
          page->print(F("Temp:"));
          page->printAndReplace(FPSTR(HTTP_INPUT_FIELD), keyT, "4", tempStr.c_str());
          page->print(F("</td>"));
        }
        page->print(F("</tr>"));
      }
      page->print(F("</table>"));
			page->print(FPSTR(HTTP_CONFIG_SAVE_BUTTON));
		}

    void submitConfigSchedulesPage(ESP8266WebServer* webServer, WStringStream* page) {
      network->notice(F("Save schedules config page"));
      schedulesChanged = false;
			for (int period = 0; period < 6; period++) {
				for (int sd = 0; sd < 3; sd++) {
          char keyH[4];
          char keyT[4];
					snprintf(keyH, 4, "%c%ch", SCHEDULES_DAYS[sd], SCHEDULES_PERIODS[period]);
					snprintf(keyT, 4, "%c%ct", SCHEDULES_DAYS[sd], SCHEDULES_PERIODS[period]);
					processSchedulesKeyValue(keyH, webServer->arg(keyH).c_str());
					processSchedulesKeyValue(keyT, webServer->arg(keyT).c_str());
				}
			}
			if (schedulesChanged) {
				network->notice(F("Some schedules changed. Write to MCU..."));
				this->schedulesToMcu();
				page->print(F("Changed schedules have been saved."));
			} else {
				page->print(F("Schedules have not changed."));
			}
    }

};


#endif
