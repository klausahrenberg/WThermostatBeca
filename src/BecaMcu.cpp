#include "BecaMcu.h"

BecaMcu::BecaMcu(KaClock *kClock) {
	this->logMcu = false;
	this->kClock = kClock;
	this->fanSpeed = FAN_NONE;
	this->actualTemperature = -100;
	this->actualFloorTemperature = -100;
	lastHeartBeat = lastNotify = lastScheduleNotify = 0;
	resetAll();
	for (int i = 0; i < STATE_COMPLETE; i++) {
		receivedStates[i] = false;
	}
	this->receivedSchedules = false;
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
		byte dt = (this->deviceOn ? 0x01 : 0x00);
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
	if (receivedSchedules) {
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

void BecaMcu::commandCharsToSerial(unsigned int length,
		unsigned char* command) {
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

void BecaMcu::queryState() {
	unsigned char queryStateCommand[] = { 0x55, 0xAA, 0x00, 0x08, 0x00, 0x00 };
	commandCharsToSerial(6, queryStateCommand);
}

void BecaMcu::cancelConfiguration() {
	unsigned char cancelConfigCommand[] = { 0x55, 0xaa, 0x00, 0x03, 0x00, 0x01,
			0x02 };
	commandCharsToSerial(7, cancelConfigCommand);
}

void BecaMcu::sendActualTimeToBeca() {
	//Command: Set date and time
	//                       ?? YY MM DD HH MM SS Weekday
	//DEC:                   01 19 02 15 16 04 18 05
	//HEX: 55 AA 00 1C 00 08 01 13 02 0F 10 04 12 05
	//DEC:                   01 19 02 20 17 51 44 03
	//HEX: 55 AA 00 1C 00 08 01 13 02 14 11 33 2C 03
	byte year = kClock->getYear() % 100;
	byte month = kClock->getMonth();
	byte dayOfMonth = kClock->getDay();
	byte hours = kClock->getHours() ;
	byte minutes = kClock->getMinutes();
	byte seconds = kClock->getSeconds();
	byte dayOfWeek = kClock->getWeekDay();
	//make sunday a seven
	dayOfWeek = (dayOfWeek ==0 ? 7 : dayOfWeek);
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
			bool newB;
			float newValue;
			byte newByte;
			//Status report from MCU
			switch (receivedCommand[6]) {
			case 0x01:
				//device On/Off
				//
				newB = (receivedCommand[10] == 0x01);
				changed = ((changed) || (newB != deviceOn));
				deviceOn = newB;
				receivedStates[0] = true;
				notifyMcuCommand("0x01");
				break;
			case 0x02:
				//desired Temperature
				//e.g. 24.5C: 55 aa 01 07 00 08 02 02 00 04 00 00 00 31
				newValue = (float) receivedCommand[13] / 2.0f;
				changed = ((changed) || (newValue != desiredTemperature));
				desiredTemperature = newValue;
				receivedStates[1] = true;
				notifyMcuCommand("0x02");
				break;
			case 0x03:
				//actual Temperature
				//e.g. 23C: 55 aa 01 07 00 08 03 02 00 04 00 00 00 2e
				newValue = (float) receivedCommand[13] / 2.0f;
				changed = ((changed) || (newValue != actualTemperature));
				actualTemperature = newValue;
				//receivedStates[2] = true;
				//notifyMcuCommand("0x03");
				break;
			case 0x04:
				//manualMode
				newB = (receivedCommand[10] == 0x01);
				changed = ((changed) || (newB != manualMode));
				manualMode = newB;
				receivedStates[2] = true;
				notifyMcuCommand("0x04");
				break;
			case 0x05:
				//ecoMode
				newB = (receivedCommand[10] == 0x01);
				changed = ((changed) || (newB != ecoMode));
				ecoMode = newB;
				receivedStates[3] = true;
				notifyMcuCommand("0x05");
				break;
			case 0x06:
				//locked
				newB = (receivedCommand[10] == 0x01);
				changed = ((changed) || (newB != locked));
				locked = newB;
				receivedStates[4] = true;
				notifyMcuCommand("0x06");
				break;
			case 0x65:
				//schedules
				for (int i = 0; i < 54; i++) {
					newByte = receivedCommand[i + 10];
					changed = ((changed) || (newByte != schedules[i]));
					schedules[i] = newByte;
				}
				receivedSchedules = true;
				notifyMcuCommand("0x65");
				break;
			case 0x66:
				//actualFloorTemperature
				//55 aa 01 07 00 08 66 02 00 04 00 00 00 00
				newValue = (float) receivedCommand[13] / 2.0f;
				changed = ((changed) || (newValue != actualFloorTemperature));
				actualFloorTemperature = newValue;
				//receivedStates[7] = true;
				//notifyMcuCommand("0x66");
				break;
			case 0x67:
				//fanSpeed
				//auto   - 55 aa 01 07 00 05 67 04 00 01 00
				//low    - 55 aa 01 07 00 05 67 04 00 01 03
				//medium - 55 aa 01 07 00 05 67 04 00 01 02
				//high   - 55 aa 01 07 00 05 67 04 00 01 01
				setFanSpeed(receivedCommand[10]);
				notifyMcuCommand("0x67");
				break;
			case 0x68:
				//Unknown permanently sent from MCU
				//55 aa 01 07 00 05 68 01 00 01 01
				break;
			default:
				notifyUnknownCommand();
			}
			if (changed) {
				notifyState();
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
	if ((logMcu) && (onNotifyCommand)) {
		onNotifyCommand("mcu: " + commandType);
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
	json["fanSpeed"] = this->getFanSpeedAsString();
	json["logMcu"] = logMcu;
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
					byte tt = (int) ((float) jsonItem[SCHEDULE_TEMPERATURE]) * 2;
					changed = changed || (schedules[startAddr + i * 3 + 2] != tt);
					schedules[startAddr + i * 3 + 2] = tt;
				}
			}
		}
	}
	return changed;
}

bool BecaMcu::setSchedules(String payload) {
	if (receivedSchedules) {
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
			scheduleCommand[6] = 0x65;
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

int BecaMcu::getFanSpeed() {
	return fanSpeed;
}

String BecaMcu::getFanSpeedAsString() {
	switch (getFanSpeed()) {
	case FAN_NONE:
		return "none";
	case FAN_AUTO:
		return "auto";
	case FAN_LOW:
		return "low";
	case FAN_MED:
		return "medium";
	case FAN_HIGH:
		return "high";
	default:
		return "unknown";
	}
}

void BecaMcu::setFanSpeed(int fanSpeed) {
	if ((fanSpeed != this->fanSpeed) && ((fanSpeed == FAN_NONE) || (fanSpeed == FAN_AUTO) || (fanSpeed == FAN_LOW) || (fanSpeed == FAN_MED) || (fanSpeed == FAN_HIGH))) {
		this->fanSpeed = fanSpeed;
		if (this->fanSpeed != FAN_NONE) {
			//send to device
		    //e.g. auto: 55 aa 00 06 00 05 67 04 00 01 00
			byte dt = (byte) this->fanSpeed;
			unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
			                                    0x67, 0x04, 0x00, 0x01, dt};
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
		uint8_t fanSpeed = FAN_AUTO;
		if (fanSpeedString.equals("low")) {
			fanSpeed = FAN_LOW;
		} else if (fanSpeedString.equals("medium")) {
			fanSpeed = FAN_MED;
		} else if (fanSpeedString.equals("high")) {
			fanSpeed = FAN_HIGH;
		} else if (fanSpeedString.equals("auto")) {
			fanSpeed = FAN_AUTO;
		}
		setFanSpeed(fanSpeed);
	}
}

void BecaMcu::increaseFanSpeed() {
	switch (getFanSpeed()) {
	case FAN_LOW :
		setFanSpeed(FAN_MED);
		break;
	case FAN_MED :
	case FAN_AUTO :
		setFanSpeed(FAN_HIGH);
		break;
	}
}

void BecaMcu::decreaseFanSpeed() {
	switch (getFanSpeed()) {
	case FAN_MED :
	case FAN_AUTO :
		setFanSpeed(FAN_LOW);
		break;
	case FAN_HIGH :
		setFanSpeed(FAN_MED);
		break;
	}
}

