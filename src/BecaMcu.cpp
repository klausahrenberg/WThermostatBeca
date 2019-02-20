#include "BecaMcu.h"

BecaMcu::BecaMcu(KaClock *kClock) {
	this->kClock = kClock;
	lastHeartBeat = lastNotify = 0;
	notifyImmediatly = false;
	resetAll();
	for (int i = 0; i < 7; i++) {
		receivedStates[i] = false;
	}
	//Serial
	//Serial.begin(9600);
}

BecaMcu::~BecaMcu() {
	/*nothing to destruct*/
}

void BecaMcu::setOnNotify(THandlerFunction onNotify) {
	this->onNotify = onNotify;
}

void BecaMcu::setOnUnknownCommand(THandlerFunction onUnknownCommand) {
	this->onUnknownCommand = onUnknownCommand;
}

void BecaMcu::setOnConfigurationRequest(
		THandlerFunction onConfigurationRequest) {
	this->onConfigurationRequest = onConfigurationRequest;
}

void BecaMcu::setDeviceOn(bool deviceOn) {
	if (this->deviceOn != deviceOn) {
		this->deviceOn = deviceOn;
		//55 AA 00 06 00 05 01 01 00 01 01
		byte dt = (this->deviceOn ? 0x01 : 0x00);
		unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
		                                    0x01, 0x01, 0x00, 0x01, dt};
		commandCharsToSerial(11, deviceOnCommand);
		notify(false);
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
		notify(false);
	}
}

void BecaMcu::notify(bool immediatly) {
	notifyImmediatly = true;
	if (immediatly) {
		lastNotify = 0;
	}
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
	if ((isDeviceStateComplete())
			&& ((lastNotify == 0) || (now - lastNotify > NOTIFY_INTERVAL)
					|| ((notifyImmediatly)
							&& (now - lastNotify > MINIMUM_INTERVAL)))) {
		notifyImmediatly = false;
		if (onNotify) {
			if (onNotify()) {
				lastNotify = millis();
			}
		} else {
			lastNotify = millis();
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
		//unknown: 55 aa 01 1c 00 00
		//tbi Request for time sync from MCU : 55 aa 01 1c 00 00
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
		} else if (receivedCommand[3] == 0x04) {
			//received: 55 aa 01 04 00 00 (setup initialization)
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
				break;
			case 0x02:
				//desired Temperature
				//e.g. 24.5C: 55 aa 01 07 00 08 02 02 00 04 00 00 00 31
				newValue = (float) receivedCommand[13] / 2.0f;
				changed = ((changed) || (newValue != desiredTemperature));
				desiredTemperature = newValue;
				receivedStates[1] = true;
				break;
			case 0x03:
				//actual Temperature
				//e.g. 23C: 55 aa 01 07 00 08 03 02 00 04 00 00 00 2e
				newValue = (float) receivedCommand[13] / 2.0f;
				changed = ((changed) || (newValue != actualTemperature));
				actualTemperature = newValue;
				receivedStates[2] = true;
				break;
			case 0x04:
				//manualMode
				newB = (receivedCommand[10] == 0x01);
				changed = ((changed) || (newB != manualMode));
				manualMode = newB;
				receivedStates[3] = true;
				break;
			case 0x05:
				//ecoMode
				newB = (receivedCommand[10] == 0x01);
				changed = ((changed) || (newB != ecoMode));
				ecoMode = newB;
				receivedStates[4] = true;
				break;
			case 0x06:
				//locked
				newB = (receivedCommand[10] == 0x01);
				changed = ((changed) || (newB != locked));
				locked = newB;
				receivedStates[5] = true;
				break;
			case 0x65:
				//schedules
				for (int i = 0; i < 54; i++) {
					newByte = receivedCommand[i + 10];
					changed = ((changed) || (newByte != schedules[i]));
					schedules[i] = newByte;
				}
				receivedStates[6] = true;
				break;
			case 0x66:
				//actualFloorTemperature
				//55 aa 01 07 00 08 66 02 00 04 00 00 00 00
				newValue = (float) receivedCommand[13] / 2.0f;
				changed = ((changed) || (newValue != actualFloorTemperature));
				actualFloorTemperature = newValue;
				break;
			case 0x68:
				//Unknown permanently sent from MCU
				//55 aa 01 07 00 05 68 01 00 01 01
				break;
			default:
				notifyUnknownCommand();
			}
			if (changed) {
				notify(true);
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
	if (onUnknownCommand) {
		onUnknownCommand();
	}
}

bool BecaMcu::isDeviceStateComplete() {
	for (int i = 0; i < 7; i++) {
		if (receivedStates[i] == false) {
			return false;
		}
	}
	return true;
}

void BecaMcu::addSchedules(byte startAddr, JsonObject& json) {
	for (int i = 0; i < 6; i++) {
		JsonObject& sch = json.createNestedObject((new String(i))->c_str());
		String timeStr = String(schedules[startAddr + i * 3 + 0]);
		timeStr = (timeStr.length() == 1 ? "0" + timeStr : timeStr);
		timeStr = String(schedules[startAddr + i * 3 + 1]) + ":" + timeStr;
		sch["time"] = timeStr;
		sch["desiredTemperature"] = (float) schedules[startAddr + i * 3 + 2]
				/ 2.0f;
	}
}

void BecaMcu::getMqttState(JsonObject& json) {
	json["deviceOn"] = deviceOn;
	json["desiredTemperature"] = desiredTemperature;
	json["actualTemperature"] = actualTemperature;
	json["actualFloorTemperature"] = actualFloorTemperature;
	json["manualMode"] = manualMode;
	json["ecoMode"] = ecoMode;
	json["locked"] = locked;
	//addSchedules(10, json.createNestedObject("workday"));
	//addSchedules(28, json.createNestedObject("saturday"));
	//addSchedules(46, json.createNestedObject("sunday"));
}

