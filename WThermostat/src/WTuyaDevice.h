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
      this->firstHeartBeatReceived = false;
      //2021-01-24 test for bht-002
      this->mcuRestarted = false;
      lastHeartBeat = lastQueryStatus = 0;
      //notifyAllMcuCommands
  		this->notifyAllMcuCommands = network->getSettings()->setBoolean("notifyAllMcuCommands", false);
  }

  virtual void queryWorkingModeWiFi() {
    //55 AA 00 08 00 00
    //55 AA 00 02 00 00
    unsigned char queryStateCommand[] = { 0x55, 0xAA, 0x00, 0x02, 0x00, 0x00 };
    commandCharsToSerial(6, queryStateCommand);
  }

  virtual void queryDeviceState() {
    //55 AA 00 08 00 00
    unsigned char queryStateCommand[] = { 0x55, 0xAA, 0x00, 0x08, 0x00, 0x00 };
    commandCharsToSerial(6, queryStateCommand);
  }

  virtual void cancelConfiguration() {
  	unsigned char cancelConfigCommand[] = { 0x55, 0xaa, 0x00, 0x03, 0x00, 0x01, 0x02 };
  	commandCharsToSerial(7, cancelConfigCommand);
    delay(1000);
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
    if ((lastHeartBeat > 0) &&
        (now - lastQueryStatus > MINIMUM_INTERVAL) &&
        (now - lastQueryStatus > QUERY_INTERVAL)) {
      this->queryDeviceState();
      lastQueryStatus = now;
    }
  }

protected :
  unsigned char receivedCommand[1024];
  WProperty* notifyAllMcuCommands;
  bool receivingDataFromMcu;
  int commandLength;
  int receiveIndex;
  bool firstHeartBeatReceived;
  //2021-01-24 test for bht-002
  bool mcuRestarted;
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

  virtual bool processCommand(byte commandByte, byte length) {
    bool knownCommand = false;
    switch (commandByte) {
      case 0x00: {
        //heartbeat signal from MCU
        switch (receivedCommand[6]) {
          case 0x00 : //55 aa 01 00 00 01 00: first heartbeat
          case 0x01 : //55 aa 01 00 00 01 01: every heartbeat after
            knownCommand = true;
            break;
        }
        //2021-01-24 test for bht-002
        this->mcuRestarted = (receivedCommand[6] == 0x00);
        if ((knownCommand) && ((!this->firstHeartBeatReceived) || (receivedCommand[6] == 0x00))) {
          //At first packet from MCU or first heart received by ESP, configure WIFI
          network->debug(F("first heart beat received: %s"), this->getCommandAsString().c_str());
          this->firstHeartBeatReceived = true;
          queryWorkingModeWiFi();
        }
        knownCommand = true;
        break;
      }
      case 0x02: {
        network->debug(F("MCU: Working mode of Wifi: %s"), this->getCommandAsString().c_str());
        //Working mode of Wifi
        if (receivedCommand[5] == 0x00) {
          knownCommand = true;
          //nothing to do
        } else if (receivedCommand[5] == 0x02) {
          //ME102H resonds: 55 AA 03 02 00 02 0E 00 14; GPIO 0E - Wifi status; 00 - Reset button
          network->setStatusLedPin(receivedCommand[6], true);
          knownCommand = true;
        }
        //after that, query the product info from MCU
        //..skipped
        //finally query the current state of device
        queryDeviceState();
        break;
      }
      case 0x07: {
        knownCommand = processStatusCommand(receivedCommand[6], length);
        break;
      }
    }
    return knownCommand;
  }

  virtual bool processStatusCommand(byte statusCommandByte, byte length) {
    return false;
  }

  virtual void processSerialCommand() {
    if (commandLength > -1) {
      //unknown
      //55 aa 00 00 00 00
      this->receivingDataFromMcu = true;
      if (notifyAllMcuCommands->getBoolean()) {
        network->notice(F("MCU: %s"), this->getCommandAsString().c_str());
      }
      bool knownCommand = false;
      if (receivedCommand[3] == 0x07) {
        knownCommand = processStatusCommand(receivedCommand[6], receivedCommand[5]);
      } else {
        knownCommand = processCommand(receivedCommand[3], receivedCommand[5]);
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

};

#endif
