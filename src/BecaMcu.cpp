#include "BecaMcu.h"

BecaMcu::BecaMcu(KaClock *kClock) {
	this->logMcu = false;
	this->kClock = kClock;
	this->fanSpeed = FAN_SPEED_NONE;
	this->systemMode = SYSTEM_MODE_NONE;
	this->manualMode = true;
	this->ecoMode = false;
	this->locked = false;
	this->actualTemperature = -100;
	this->actualFloorTemperature = -100;
	this->thermostatModel = MODEL_BHT_002_GBLW;
	lastHeartBeat = lastNotify = lastScheduleNotify = 0;
	this->loadSettings();
	resetAll();
	for (int i = 0; i < STATE_COMPLETE; i++) {
		receivedStates[i] = false;
	}
	this->schedulesDataPoint = 0x00;
}

BecaMcu::~BecaMcu() {
	/*nothing to destruct*/
}

void BecaMcu::setOnNotify(THandlerFunction onNotify) {
	this->onNotify = onNotify;
}

void BecaMcu::setOnNotifyCommand(TCommandHandlerFunction onNotifyCommand) {
	this->onNotifyCommand = onNotifyCommand;
}

void BecaMcu::setOnConfigurationRequest(THandlerFunction onConfigurationRequest) {
	this->onConfigurationRequest = onConfigurationRequest;
}

void BecaMcu::setOnSchedulesChange(THandlerFunction onSchedulesChange) {
	this->onSchedulesChange = onSchedulesChange;
}

void BecaMcu::setDeviceOn(bool deviceOn) {
	if (this->deviceOn != deviceOn) {
		this->deviceOn = deviceOn;
		//55 AA 00 06 00 05 01 01 00 01 01
		byte dt = (this->deviceOn ? 0x01 : 0x00);
		unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
		                                    0x01, 0x01, 0x00, 0x01, dt};
		commandCharsToSerial(11, deviceOnCommand);
		notifyState();
	}
}

void BecaMcu::setDesiredTemperature(float desiredTemperature) {
	if ((this->desiredTemperature != desiredTemperature) && (((int) (desiredTemperature * 10) % 5) == 0)) {
		this->desiredTemperature = desiredTemperature;
		//55 AA 00 06 00 08 02 02 00 04 00 00 00 2C
		byte dt = (byte) (this->desiredTemperature * 2);
		unsigned char setTemperatureCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x08,
		                                          0x02, 0x02, 0x00, 0x04,
		                                          0x00, 0x00, 0x00, dt};
		commandCharsToSerial(14, setTemperatureCommand);
		notifyState();
	}
}

float BecaMcu::getActualTemperature() {
	return this->actualTemperature;
}

float BecaMcu::getActualFloorTemperature() {
	return this->actualFloorTemperature;
}

float BecaMcu::getDesiredTemperature() {
	return this->desiredTemperature;
}

bool BecaMcu::getManualMode() {
	return this->manualMode;
}

bool BecaMcu::getEcoMode() {
	return this->ecoMode;
}

bool BecaMcu::getLocked() {
	return this->locked;
}

void BecaMcu::setManualMode(bool manualMode) {
	if (this->manualMode != manualMode) {
		this->manualMode = manualMode;
		//55 AA 00 06 00 05 04 04 00 01 01
		byte dt = (this->manualMode ? 0x01 : 0x00);
		unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
		                                    0x04, 0x04, 0x00, 0x01, dt};
		commandCharsToSerial(11, deviceOnCommand);
		notifyState();
	}
}

void BecaMcu::setEcoMode(bool ecoMode) {
	if (this->ecoMode != ecoMode) {
		this->ecoMode = ecoMode;
		//55 AA 00 06 00 05 05 01 00 01 01
		byte dt = (this->ecoMode ? 0x01 : 0x00);
		unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
		                                    0x05, 0x01, 0x00, 0x01, dt};
		commandCharsToSerial(11, deviceOnCommand);
		notifyState();
	}
}

void BecaMcu::setLocked(bool locked) {
	if (this->locked != locked) {
		this->locked = locked;
		//55 AA 00 06 00 05 06 01 00 01 01
		byte dt = (this->locked ? 0x01 : 0x00);
		unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
		                                    0x06, 0x01, 0x00, 0x01, dt};
		commandCharsToSerial(11, deviceOnCommand);
		notifyState();
	}
}

void BecaMcu::notifyState() {
	lastNotify = 0;
}

void BecaMcu::notifySchedules() {
	lastScheduleNotify = 0;
}

void BecaMcu::resetAll() {
	receiveIndex = -1;
	commandLength = -1;
}

void BecaMcu::loop() {
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
	//Heartbeat
	long now = millis();
	if ((HEARTBEAT_INTERVAL > 0)
			&& ((lastHeartBeat == 0)
					|| (now - lastHeartBeat > HEARTBEAT_INTERVAL))) {
		unsigned char heartBeatCommand[] =
				{ 0x55, 0xAA, 0x00, 0x00, 0x00, 0x00 };
		commandCharsToSerial(6, heartBeatCommand);
		//commandHexStrToSerial("55 aa 00 00 00 00");
		lastHeartBeat = now;
	}
	//Notify
	if (isDeviceStateComplete()) {
		if (((lastNotify == 0) && (now - lastNotify > MINIMUM_INTERVAL)) || (now - lastNotify > NOTIFY_INTERVAL)) {
			if (onNotify) {
				if (onNotify()) {
					lastNotify = now;
				}
			} else {
				lastNotify = now;
			}
		}
	}
	if (receivedSchedules()) {
		//Notify schedules
		if ((lastScheduleNotify == 0) && (now - lastScheduleNotify > MINIMUM_INTERVAL)) {
			if (onSchedulesChange) {
				onSchedulesChange();
			}
			lastScheduleNotify = now;
		}
	}
}

unsigned char* BecaMcu::getCommand() {
	return receivedCommand;
}

int BecaMcu::getCommandLength() {
	return commandLength;
}

String BecaMcu::getCommandAsString() {
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

int BecaMcu::getIndex(unsigned char c) {
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

void BecaMcu::commandHexStrToSerial(String command) {
	command.trim();
	command.replace(" ", "");
	command.toLowerCase();
	int chkSum = 0;
	if ((command.length() > 1) && (command.length() % 2 == 0)) {
		for (unsigned int i = 0; i < (command.length() / 2); i++) {
			unsigned char chValue = getIndex(command.charAt(i * 2)) * 0x10
					+ getIndex(command.charAt(i * 2 + 1));
			chkSum += chValue;
			Serial.print((char) chValue);
		}
		unsigned char chValue = chkSum % 0x100;
		Serial.print((char) chValue);
	}
}

void BecaMcu::commandCharsToSerial(unsigned int length,
		unsigned char* command) {
	int chkSum = 0;
	if (length > 2) {
		for (unsigned int i = 0; i < length; i++) {
			unsigned char chValue = command[i];
			chkSum += chValue;
			Serial.print((char) chValue);
		}
		unsigned char chValue = chkSum % 0x100;
		Serial.print((char) chValue);
	}
}

void BecaMcu::queryState() {
	//55 AA 00 08 00 00
	unsigned char queryStateCommand[] = { 0x55, 0xAA, 0x00, 0x08, 0x00, 0x00 };
	commandCharsToSerial(6, queryStateCommand);
}

void BecaMcu::cancelConfiguration() {
	unsigned char cancelConfigCommand[] = { 0x55, 0xaa, 0x00, 0x03, 0x00, 0x01,
			0x02 };
	commandCharsToSerial(7, cancelConfigCommand);
}

byte BecaMcu::getDayOfWeek() {
	unsigned long epochTime = kClock->getEpochTime();
	epochTime = epochTime + (schedulesDayOffset * 86400);
	byte dayOfWeek = kClock->getWeekDay(epochTime);
	//make sunday a seven
	dayOfWeek = (dayOfWeek ==0 ? 7 : dayOfWeek);
	return dayOfWeek;
}

bool BecaMcu::isWeekend() {
	byte dayOfWeek = getDayOfWeek();
	return ((dayOfWeek == 6) || (dayOfWeek == 7));
}

void BecaMcu::sendActualTimeToBeca() {
	//Command: Set date and time
	//                       ?? YY MM DD HH MM SS Weekday
	//DEC:                   01 19 02 15 16 04 18 05
	//HEX: 55 AA 00 1C 00 08 01 13 02 0F 10 04 12 05
	//DEC:                   01 19 02 20 17 51 44 03
	//HEX: 55 AA 00 1C 00 08 01 13 02 14 11 33 2C 03
	unsigned long epochTime = kClock->getEpochTime();
	epochTime = epochTime + (schedulesDayOffset * 86400);
	byte year = kClock->getYear(epochTime) % 100;
	byte month = kClock->getMonth(epochTime);
	byte dayOfMonth = kClock->getDay(epochTime);
	byte hours = kClock->getHours(epochTime) ;
	byte minutes = kClock->getMinutes(epochTime);
	byte seconds = kClock->getSeconds(epochTime);
	byte dayOfWeek = getDayOfWeek();
	unsigned char cancelConfigCommand[] = { 0x55, 0xaa, 0x00, 0x1c, 0x00, 0x08,
											0x01, year, month, dayOfMonth,
											hours, minutes, seconds, dayOfWeek};
	commandCharsToSerial(14, cancelConfigCommand);
}

void BecaMcu::processSerialCommand() {
	if (commandLength > -1) {
		//unknown
		//55 aa 00 00 00 00


		if (receivedCommand[3] == 0x00) {
			switch (receivedCommand[6]) {
			case 0x00:
			case 0x01:
				//ignore, heartbeat MCU
				//55 aa 01 00 00 01 01
				//55 aa 01 00 00 01 00
				break;
			default:
				notifyUnknownCommand();
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
			bool changed = false;
			bool schedulesChanged = false;
			bool newB;
			float newValue;
			byte newByte;
			byte commandLength = receivedCommand[5];
			bool knownCommand = false;
			//Status report from MCU
			switch (receivedCommand[6]) {
			case 0x01:
				if (commandLength == 0x05) {
					//device On/Off
					//55 aa 00 06 00 05 01 01 00 01 00|01
					newB = (receivedCommand[10] == 0x01);
					changed = ((changed) || (newB != deviceOn));
					deviceOn = newB;
					receivedStates[0] = true;
					notifyMcuCommand("deviceOn_x01");
					knownCommand = true;
				}
				break;
			case 0x02:
				if (commandLength == 0x08) {
					//desired Temperature
					//e.g. 24.5C: 55 aa 01 07 00 08 02 02 00 04 00 00 00 31
					newValue = (float) receivedCommand[13] / 2.0f;
					changed = ((changed) || (newValue != desiredTemperature));
					desiredTemperature = newValue;
					receivedStates[1] = true;
					notifyMcuCommand("desiredTemperature_x02");
					knownCommand = true;
				}
				break;

			case 0x03:
				if (commandLength == 0x08) {
					//actual Temperature
					//e.g. 23C: 55 aa 01 07 00 08 03 02 00 04 00 00 00 2e
					newValue = (float) receivedCommand[13] / 2.0f;
					changed = ((changed) || (newValue != actualTemperature));
					actualTemperature = newValue;
					notifyMcuCommand("actualTemperature_x03");
					knownCommand = true;
				}
				break;
			case 0x04:
				if (commandLength == 0x05) {
					//manualMode
					newB = (receivedCommand[10] == 0x01);
					changed = ((changed) || (newB != manualMode));
					manualMode = newB;
					receivedStates[2] = true;
					notifyMcuCommand("manualMode_x04");
					knownCommand = true;
				}
				break;
			case 0x05:
				if (commandLength == 0x05) {
					//ecoMode
					newB = (receivedCommand[10] == 0x01);
					changed = ((changed) || (newB != ecoMode));
					ecoMode = newB;
					receivedStates[3] = true;
					notifyMcuCommand("ecoMode_x05");
					knownCommand = true;
				}
				break;
			case 0x06:
				if (commandLength == 0x05) {
					//locked
					newB = (receivedCommand[10] == 0x01);
					changed = ((changed) || (newB != locked));
					locked = newB;
					receivedStates[4] = true;
					notifyMcuCommand("locked_x06");
					knownCommand = true;
				}
				break;
			case 0x65: //MODEL_BHT_002_GBLW
			case 0x68: //MODEL_BAC_002_ALW
				if (commandLength == 0x3A) {
					//schedules 0x65 at heater model, 0x68 at fan model, example
					//55 AA 00 06 00 3A 65 00 00 36
					//00 07 28 00 08 1E 1E 0B 1E 1E 0D 1E 00 11 2C 00 16 1E
					//00 06 28 00 08 28 1E 0B 28 1E 0D 28 00 11 28 00 16 1E
					//00 06 28 00 08 28 1E 0B 28 1E 0D 28 00 11 28 00 16 1E
					this->schedulesDataPoint = receivedCommand[6];
					this->thermostatModel = (this->schedulesDataPoint == 0x65 ? MODEL_BHT_002_GBLW : MODEL_BAC_002_ALW);
					for (int i = 0; i < 54; i++) {
						newByte = receivedCommand[i + 10];
						schedulesChanged = ((schedulesChanged) || (newByte != schedules[i]));
						schedules[i] = newByte;
					}
					notifyMcuCommand(this->thermostatModel == MODEL_BHT_002_GBLW ? "schedules_x65" : "schedules_x68");
					knownCommand = true;
				} else if (receivedCommand[5] == 0x05) {
					//Unknown permanently sent from MCU
					//55 aa 01 07 00 05 68 01 00 01 01
					knownCommand = true;
				}
				break;
			case 0x66:
				if (commandLength == 0x08) {
					//MODEL_BHT_002_GBLW - actualFloorTemperature
					//55 aa 01 07 00 08 66 02 00 04 00 00 00 00
					newValue = (float) receivedCommand[13] / 2.0f;
					changed = ((changed) || (newValue != actualFloorTemperature));
					actualFloorTemperature = newValue;
					notifyMcuCommand("actualFloorTemperature_x66");
					knownCommand = true;
				} else if (commandLength == 0x05) {
					//MODEL_BAC_002_ALW - systemMode
					//cooling:     55 AA 00 06 00 05 66 04 00 01 00
					//heating:     55 AA 00 06 00 05 66 04 00 01 01
					//ventilation: 55 AA 00 06 00 05 66 04 00 01 02
					this->thermostatModel = MODEL_BAC_002_ALW;
					changed = ((changed) || (receivedCommand[10] != this->systemMode));
					this->systemMode = receivedCommand[10];
					notifyMcuCommand("systemMode_x66");
					knownCommand = true;
				}
				break;
			case 0x67:
				if (commandLength == 0x05) {
					//fanSpeed
					//auto   - 55 aa 01 07 00 05 67 04 00 01 00
					//low    - 55 aa 01 07 00 05 67 04 00 01 03
					//medium - 55 aa 01 07 00 05 67 04 00 01 02
					//high   - 55 aa 01 07 00 05 67 04 00 01 01
					setFanSpeed(receivedCommand[10]);
					notifyMcuCommand("fanSpeed_x67");
					knownCommand = true;
				}
				break;
			}
			if (!knownCommand) {
				notifyUnknownCommand();
			} else if (changed) {
				notifyState();
			} else if (schedulesChanged) {
				notifySchedules();
			}

		} else if (receivedCommand[3] == 0x1C) {
			//Request for time sync from MCU : 55 aa 01 1c 00 00
			this->sendActualTimeToBeca();
		} else {
			notifyUnknownCommand();
		}
	}
}

void BecaMcu::notifyUnknownCommand() {
	if (onNotifyCommand) {
		onNotifyCommand("unknown");
	}
}

void BecaMcu::setLogMcu(bool logMcu) {
	if (this->logMcu != logMcu) {
		this->logMcu = logMcu;
		notifyState();
	}
}

void BecaMcu::notifyMcuCommand(String commandType) {	
	if  (onNotifyCommand) {
		if (logMcu) onNotifyCommand("mcu: " + commandType);
		if (commandType == "actualTemperature_x03") {
			onNotifyCommand("actualTemperature");
		} else if (commandType == "actualFloorTemperature_x03") {
			onNotifyCommand("actualFloorTemperature");
		} else if (commandType == "desiredTemperature_x02") {
			onNotifyCommand("desiredTemperature");
		} else if (commandType == "manualMode_x04") {
			onNotifyCommand("manualMode");	
		} else if (commandType == "ecoMode_x05") {
			onNotifyCommand("ecoMode");		
		} else if (commandType == "locked_x06") {
			onNotifyCommand("locked");
		}	
	}
}

bool BecaMcu::isDeviceStateComplete() {
	for (int i = 0; i < STATE_COMPLETE; i++) {
		if (receivedStates[i] == false) {
			return false;
		}
	}
	return true;
}

void BecaMcu::getMqttState(JsonObject& json) {
	json["deviceOn"] = deviceOn;
	json["desiredTemperature"] = desiredTemperature;
	if (actualTemperature != -100) {
		json["actualTemperature"] = actualTemperature;
	}
	if (actualFloorTemperature != -100) {
		json["actualFloorTemperature"] = actualFloorTemperature;
	}
	json["manualMode"] = manualMode;
	json["ecoMode"] = ecoMode;
	json["locked"] = locked;
	if (thermostatModel == MODEL_BAC_002_ALW) {
		json["fanSpeed"] = this->getFanSpeedAsString();
		json["systemMode"] = this->getSystemModeAsString();
	}
	json["thermostatModel"] = this->thermostatModel;
	json["logMcu"] = logMcu;
	json["schedulesDayOffset"] = schedulesDayOffset;
	json["weekend"] = isWeekend();
}

void BecaMcu::getMqttSchedules(JsonObject& json, String dayRange) {
	int startAddr = 0;
	if (SCHEDULE_SATURDAY.equals(dayRange)) {
		startAddr = 18;
	} else if (SCHEDULE_SUNDAY.equals(dayRange)) {
		startAddr = 36;
	}
	JsonObject& jsonDay = json.createNestedObject(dayRange);
	char timeStr[5];
	for (int i = 0; i < 6; i++) {
		JsonObject& sch = jsonDay.createNestedObject((new String(i))->c_str());
		sprintf(timeStr, "%02d:%02d", schedules[startAddr + i * 3 + 1], schedules[startAddr + i * 3 + 0]);
		sch["h"] = timeStr;
		sch["t"] = (float) schedules[startAddr + i * 3 + 2]	/ 2.0f;
	}
}

bool BecaMcu::setSchedules(int startAddr, JsonObject& json, String dayRange) {
	bool changed = false;
	if (json.containsKey(dayRange)) {
		JsonObject& jsonDayRange = json[dayRange];
		for (int i = 0; i < 6; i++) {
			if (jsonDayRange.containsKey(String(i))) {
				JsonObject& jsonItem = jsonDayRange[String(i)];
				if (jsonItem.containsKey(SCHEDULE_HOUR)) {
					String timeStr = jsonItem[SCHEDULE_HOUR];
					timeStr = (timeStr.length() == 4 ? "0" + timeStr : timeStr);
					byte hh = timeStr.substring(0, 2).toInt();
					byte mm = timeStr.substring(3, 5).toInt();
					changed = changed || (schedules[startAddr + i * 3 + 1] != hh);
					schedules[startAddr + i * 3 + 1] = hh;
					changed = changed || (schedules[startAddr + i * 3 + 0] != mm);
					schedules[startAddr + i * 3 + 0] = mm;
				}
				if (jsonItem.containsKey(SCHEDULE_TEMPERATURE)) {
					byte tt = (int) ((float) jsonItem[SCHEDULE_TEMPERATURE] * 2);
					changed = changed || (schedules[startAddr + i * 3 + 2] != tt);
					schedules[startAddr + i * 3 + 2] = tt;
				}
			}
		}
	}
	return changed;
}

boolean BecaMcu::receivedSchedules() {
	return (this->schedulesDataPoint != 0x00);
}

bool BecaMcu::setSchedules(String payload) {
	if (receivedSchedules()) {
		StaticJsonBuffer<1216> jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(payload);
		bool changed = false;
		if (json.success()) {
			changed = ((changed) || (setSchedules( 0, json, SCHEDULE_WORKDAY)));
			changed = ((changed) || (setSchedules(18, json, SCHEDULE_SATURDAY)));
			changed = ((changed) || (setSchedules(36, json, SCHEDULE_SUNDAY)));
		}
		if (changed) {
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
			scheduleCommand[6] = schedulesDataPoint;
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
		return changed;
	} else {
		return false;
	}
}

byte BecaMcu::getFanSpeed() {
	return fanSpeed;
}

String BecaMcu::getFanSpeedAsString() {
	switch (getFanSpeed()) {
	case FAN_SPEED_AUTO:
		return "auto";
	case FAN_SPEED_LOW:
		return "low";
	case FAN_SPEED_MED:
		return "medium";
	case FAN_SPEED_HIGH:
		return "high";
	default:
		//FAN_SPEED_NONE
		return "none";
	}
}

void BecaMcu::setFanSpeed(byte fanSpeed) {
	if ((thermostatModel == MODEL_BAC_002_ALW) && (fanSpeed != this->fanSpeed) &&
		((fanSpeed == FAN_SPEED_NONE) || (fanSpeed == FAN_SPEED_AUTO) || (fanSpeed == FAN_SPEED_LOW) || (fanSpeed == FAN_SPEED_MED) || (fanSpeed == FAN_SPEED_HIGH))) {
		this->fanSpeed = fanSpeed;
		if (this->fanSpeed != FAN_SPEED_NONE) {
			//send to device
		    //auto:   55 aa 00 06 00 05 67 04 00 01 00
			//low:    55 aa 00 06 00 05 67 04 00 01 03
			//medium: 55 aa 00 06 00 05 67 04 00 01 02
			//high:   55 aa 00 06 00 05 67 04 00 01 01
			unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
			                                    0x67, 0x04, 0x00, 0x01, fanSpeed};
			commandCharsToSerial(11, deviceOnCommand);
		}
		notifyState();
	}
}

void BecaMcu::setFanSpeedFromString(String fanSpeedString) {
	fanSpeedString.toLowerCase();
	fanSpeedString.trim();
	if (fanSpeedString.equals("increase")) {
		increaseFanSpeed();
	} else if (fanSpeedString.equals("decrease")) {
		decreaseFanSpeed();
	} else {
		byte fanSpeed = FAN_SPEED_NONE;
		if (fanSpeedString.equals("low")) {
			fanSpeed = FAN_SPEED_LOW;
		} else if (fanSpeedString.equals("medium")) {
			fanSpeed = FAN_SPEED_MED;
		} else if (fanSpeedString.equals("high")) {
			fanSpeed = FAN_SPEED_HIGH;
		} else if (fanSpeedString.equals("auto")) {
			fanSpeed = FAN_SPEED_AUTO;
		}
		setFanSpeed(fanSpeed);
	}
}

void BecaMcu::increaseFanSpeed() {
	switch (getFanSpeed()) {
	case FAN_SPEED_LOW :
		setFanSpeed(FAN_SPEED_MED);
		break;
	case FAN_SPEED_MED :
	case FAN_SPEED_AUTO :
		setFanSpeed(FAN_SPEED_HIGH);
		break;
	}
}

void BecaMcu::decreaseFanSpeed() {
	switch (getFanSpeed()) {
	case FAN_SPEED_MED :
	case FAN_SPEED_AUTO :
		setFanSpeed(FAN_SPEED_LOW);
		break;
	case FAN_SPEED_HIGH :
		setFanSpeed(FAN_SPEED_MED);
		break;
	}
}

byte BecaMcu::getSystemMode() {
	return systemMode;
}

String BecaMcu::getSystemModeAsString() {
	switch (getSystemMode()) {
	case SYSTEM_MODE_COOLING:
		return "cooling";
	case SYSTEM_MODE_HEATING:
		return "heating";
	case SYSTEM_MODE_VENTILATION:
		return "ventilation";
	default:
		//SYSTEM_MODE_NONE
		return "none";
	}
}

void BecaMcu::setSystemMode(byte systemMode) {
	if ((thermostatModel == MODEL_BAC_002_ALW) && (systemMode != this->systemMode) &&
	    ((systemMode == SYSTEM_MODE_NONE) || (systemMode == SYSTEM_MODE_COOLING) || (systemMode == SYSTEM_MODE_HEATING) || (systemMode == SYSTEM_MODE_VENTILATION))) {
		this->systemMode = systemMode;
		if (this->systemMode != SYSTEM_MODE_NONE) {
			//send to device
			//cooling:     55 AA 00 06 00 05 66 04 00 01 00
		    //heating:     55 AA 00 06 00 05 66 04 00 01 01
			//ventilation: 55 AA 00 06 00 05 66 04 00 01 02
			unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
				                                0x66, 0x04, 0x00, 0x01, systemMode};
			commandCharsToSerial(11, deviceOnCommand);
		}
		notifyState();
	}
}

void BecaMcu::setSystemModeFromString(String systemModeString) {
	systemModeString.toLowerCase();
	systemModeString.trim();

	byte systemMode = SYSTEM_MODE_NONE;
	if (systemModeString.equals("cooling")) {
		systemMode = SYSTEM_MODE_COOLING;
	} else if (systemModeString.equals("heating")) {
		systemMode = SYSTEM_MODE_HEATING;
	} else if (systemModeString.equals("ventilation")) {
		systemMode = SYSTEM_MODE_VENTILATION;
	}
	setSystemMode(systemMode);
}

signed char BecaMcu::getSchedulesDayOffset() {
	return schedulesDayOffset;
}

void BecaMcu::setSchedulesDayOffset(signed char schedulesDayOffset) {
	if (this->schedulesDayOffset != schedulesDayOffset) {
		this->schedulesDayOffset = schedulesDayOffset;
		saveSettings();
		this->sendActualTimeToBeca();
		notifyState();
	}
}

void BecaMcu::loadSettings() {
	EEPROM.begin(512);
	if (EEPROM.read(300) == STORED_FLAG) {
		this->schedulesDayOffset = EEPROM.read(301);
	} else {
		this->schedulesDayOffset = 0;
	}
	EEPROM.end();
}

void BecaMcu::saveSettings() {
	EEPROM.begin(512);
	EEPROM.write(301, this->schedulesDayOffset);
	EEPROM.write(300, STORED_FLAG);
	EEPROM.commit();
	EEPROM.end();
}

