#ifndef TUYA_DEVICE_H
#define	TUYA_DEVICE_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WDevice.h"

#define HEARTBEAT_INTERVAL 10000
#define MINIMUM_INTERVAL 2000
#define QUERY_INTERVAL 2000

const unsigned char COMMAND_START[] = {0x55, 0xAA};

class WTuyaDevice : public WDevice {
public :
  WTuyaDevice(WNetwork* network, const char* id, const char* name, const char* type)
    : WDevice(network, id, name, type) {
      resetAll();
      this->receivingDataFromMcu = false;
      lastHeartBeat = lastQueryStatus = 0;
      //notifyAllMcuCommands
  		this->notifyAllMcuCommands = network->getSettings()->setBoolean("notifyAllMcuCommands", false);
  }

  virtual void loop(unsigned long now) {
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
    if ((HEARTBEAT_INTERVAL > 0)
        && ((lastHeartBeat == 0)
            || (now - lastHeartBeat > HEARTBEAT_INTERVAL))) {
      unsigned char heartBeatCommand[] =
          { 0x55, 0xAA, 0x00, 0x00, 0x00, 0x00 };
      commandCharsToSerial(6, heartBeatCommand);
      //commandHexStrToSerial("55 aa 00 00 00 00");
      lastHeartBeat = now;
    }
    //Query
    if ((now - lastHeartBeat > MINIMUM_INTERVAL)
        && (now - lastQueryStatus > QUERY_INTERVAL)) {
      //queryState();
      lastQueryStatus = now;
    }
  }

protected :
  unsigned char receivedCommand[1024];
  WProperty* notifyAllMcuCommands;

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

  virtual bool processCommand(byte commandByte) {
    return false;
  }

  virtual bool processStatusCommand(byte statusCommandByte, byte commandLength) {
    return false;
  }

  virtual void processSerialCommand() {
    if (commandLength > -1) {
      //unknown
      //55 aa 00 00 00 00
      this->receivingDataFromMcu = true;
      if (notifyAllMcuCommands->getBoolean()) {
        network->error(F("MCU: %s"), this->getCommandAsString().c_str());
      }
      bool knownCommand = false;
      if (receivedCommand[3] == 0x00) {
        switch (receivedCommand[6]) {
          case 0x00:
          case 0x01:
            //ignore, heartbeat MCU
            //55 aa 01 00 00 01 01
            //55 aa 01 00 00 01 00
            break;
        }
        knownCommand = true;
      } else if (receivedCommand[3] == 0x07) {
        knownCommand = processStatusCommand(receivedCommand[6], receivedCommand[5]);
      } else {
        knownCommand = processCommand(receivedCommand[3]);
      }
      if (!knownCommand) {
        notifyUnknownCommand();
      }
      this->receivingDataFromMcu = false;
    }
  }

  bool isReceivingDataFromMcu() {
    return this->receivingDataFromMcu;
  }

  void notifyUnknownCommand() {
    network->error(F("Unknown MCU command: %s"), this->getCommandAsString().c_str());
  }

private :
  int receiveIndex;
  int commandLength;
  boolean receivingDataFromMcu;
  unsigned long lastHeartBeat;
  unsigned long lastQueryStatus;

  void resetAll() {
    receiveIndex = -1;
    commandLength = -1;
  }

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

};

#endif
