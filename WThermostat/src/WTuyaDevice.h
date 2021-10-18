#ifndef TUYA_DEVICE_H
#define	TUYA_DEVICE_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WDevice.h"

#define HEARTBEAT_INTERVAL 10000
#define MINIMUM_INTERVAL 2000
#define QUERY_INTERVAL 2000
#define CMD_RESP_TIMEOUT 1500

const unsigned char COMMAND_START[] = {0x55, 0xAA};

enum WTuyaDeviceState {
    STATE_INIT,
    STATE_PRODUCT_INFO_WAIT,
    STATE_PRODUCT_INFO_DONE,
    STATE_WIFI_WORKING_MODE_WAIT,
    STATE_WIFI_WORKING_MODE_DONE,
    STATE_COMMAND_WAIT,
    STATE_COMMAND_DONE,
    STATE_IDLE,
};

#define COMMAND_WRITE_Q_LENGTH 8

class WTuyaDevice : public WDevice {
public :
  WTuyaDevice(WNetwork* network, const char* id, const char* name, const char* type)
    : WDevice(network, id, name, type) {
      resetAll();
      this->receivingDataFromMcu = false;
      lastHeartBeat = lastQueryStatus = 0;
      //notifyAllMcuCommands
  		this->notifyAllMcuCommands = network->getSettings()->setBoolean("notifyAllMcuCommands", false);
      // JY
      gpioStatus = -1;
      gpioReset = -1;
      processingState = STATE_INIT;
      commandWriteQDepth = 0;
      m_iCommandRetry = 0;
      iResetState = 0;
      lastCommandSent = 0;
      usingCommandQueue = false;
  }

  virtual void queryProductInfo() {
    unsigned char queryStateCommand[] = { 0x55, 0xAA, 0x00, 0x01, 0x00, 0x00 };
    commandCharsToSerial(6, queryStateCommand, false, STATE_PRODUCT_INFO_WAIT);
  }

  virtual void queryWorkingModeWiFi() {
    unsigned char queryStateCommand[] = { 0x55, 0xAA, 0x00, 0x02, 0x00, 0x00 };
    commandCharsToSerial(6, queryStateCommand, false, STATE_WIFI_WORKING_MODE_WAIT);
  }

  virtual void queryDeviceState() {
    //55 AA 00 08 00 00
    unsigned char queryStateCommand[] = { 0x55, 0xAA, 0x00, 0x08, 0x00, 0x00 };
    commandCharsToSerial(6, queryStateCommand);
  }

  virtual void cancelConfiguration() {
    if (gpioStatus != -1) {
      pinMode(gpioStatus, OUTPUT);
      digitalWrite(gpioStatus, LOW);
    } else {
  	  unsigned char cancelConfigCommand[] = { 0x55, 0xaa, 0x00, 0x03, 0x00, 0x01, 0x02 };
      commandCharsToSerial(7, cancelConfigCommand);
    }
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
    //
    if (gpioReset != -1) {
      int iNewResetState = digitalRead(gpioReset);
      if (iResetState != iNewResetState) {
        network->debug(F("Wifi Reset State change, new state = %d"), iNewResetState);
        iResetState = iNewResetState;
        if (!iResetState) {
          //tbi
          /*if (onConfigurationRequest) {
            network->debug("MCU has signalled Reset, new state = %d", iNewResetState);
            // set special property to force AP mode and reconfig
            // even if connection to configured SSID succeeds
            WProperty* propForce = network->getSettings()->setBoolean("forceConfig", true);
            propForce->setBoolean(true);
            network->getSettings()->save();
            onConfigurationRequest();
          }*/
        }
      }
    }

    switch (processingState) {
      case STATE_INIT: {
        queryProductInfo();
        break;
      }
      case STATE_PRODUCT_INFO_WAIT: {
        if ((now - lastCommandSent) > CMD_RESP_TIMEOUT) {
          setProcessingState(STATE_PRODUCT_INFO_DONE);
          network->error(F("Timeout: waiting for Product Info response"));
        }
        break;
      }
      case STATE_PRODUCT_INFO_DONE: {
        queryWorkingModeWiFi();
        break;
      }
      case STATE_WIFI_WORKING_MODE_WAIT: {
        if ((now - lastCommandSent) > CMD_RESP_TIMEOUT) {
          setProcessingState(STATE_WIFI_WORKING_MODE_DONE);
          network->error(F("Timeout: waiting for Wifi Working Mode response"));
        }
        break;
      }
      case STATE_WIFI_WORKING_MODE_DONE: {
        //Note: if wanting other MCU commands, change the state
        setProcessingState(STATE_IDLE);
        break;
      }
      case STATE_COMMAND_WAIT: {
        // check for response
        if ((now - lastCommandSent) > CMD_RESP_TIMEOUT) {
          // let send the latest command again.... hope for the best
          if (m_iCommandRetry < 1) {
            //network->debug("Timeout: waiting for thermostat command response, try again");
            if (commandWriteQDepth != 0) {
              m_iCommandRetry++;
              byte* pBuffer = (byte*)commandWriteQueue[0];
              commandCharsToSerial(*((unsigned int*)(pBuffer + 0)), pBuffer + sizeof(int), true, STATE_COMMAND_WAIT);
            }
          } else {
            setProcessingState(STATE_COMMAND_DONE);
            //network->debug("Timeout: waiting for thermostat command response, oh well");
          }
        }
        break;
      }
      case STATE_COMMAND_DONE: {
        m_iCommandRetry = 0;
        // dequeue top commanf, shift and send next
        // if we have queued commands..... execute them
        if (commandWriteQDepth != 0) {
          //network->debug(F("loop: command complete.... dequeue top element in Q"));
          // dequeue first in queue
          byte * pBuffer = (byte *)commandWriteQueue[0];
          // release memory
          free(pBuffer);
          // shift all down
          for (int i = 0; i < (commandWriteQDepth-1); i++) {
            commandWriteQueue[i] = commandWriteQueue[i + 1];
          }
          // readjust index
          commandWriteQDepth--;
          // NEXT

          // if stuff on queue, sent it
          if (commandWriteQDepth != 0) {
            //network->debug(F("loop: command complete.... send next element in Q"));
            byte* pBuffer = (byte*)commandWriteQueue[0];
            commandCharsToSerial(*((unsigned int*)(pBuffer + 0)), pBuffer + sizeof(int), true, STATE_COMMAND_WAIT);
          }
        } else {
          setProcessingState(STATE_IDLE);
        }
        break;
      }
      case STATE_IDLE: {
        //Heartbeat
        if ((HEARTBEAT_INTERVAL > 0) &&
            ((lastHeartBeat == 0) || (now - lastHeartBeat > HEARTBEAT_INTERVAL))) {
              unsigned char heartBeatCommand[] = { 0x55, 0xAA, 0x00, 0x00, 0x00, 0x00 };
          commandCharsToSerial(6, heartBeatCommand);
          //commandHexStrToSerial("55 aa 00 00 00 00");
          lastHeartBeat = now;
        }
        //Query
        if (((now - lastHeartBeat) > MINIMUM_INTERVAL)
            && ((lastQueryStatus == 0) || (now - lastQueryStatus > QUERY_INTERVAL))) {
          queryDeviceState();
          lastQueryStatus = now;
        }
        break;
      }
    }

    /*//Heartbeat
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
    }*/
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
  int iResetState;        // JY
  unsigned long lastCommandSent;   // JY
  WTuyaDeviceState processingState; // JY
  void* commandWriteQueue[COMMAND_WRITE_Q_LENGTH]; // JY
  int commandWriteQDepth; // JY
  int m_iCommandRetry; // JY
  int8_t gpioStatus; // JY
  int8_t gpioReset; // JY
  bool usingCommandQueue;

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
    return getBufferAsString(commandLength, receivedCommand);
  }

  String getBufferAsString(int length, unsigned char* command) {
    String result = "";
    bool fSpace = false;
    if (length > -1) {
      for (int i = 0; i < length; i++) {
        unsigned char ch = command[i];
        if (fSpace)
          result = result + " ";
        result = result + (ch < 16 ? "0" : "") + String(ch, HEX);// charToHexStr(ch);
        fSpace = true;
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
        if ((knownCommand) && ((this->processingState == STATE_INIT) || (receivedCommand[6] == 0x00))) {
          //At first packet from MCU or first heart received by ESP, query queryProductInfo
          queryProductInfo();
          this->processingState = STATE_PRODUCT_INFO_WAIT;
        }
        knownCommand = true;
        break;
      }
      case 0x01: {
        network->debug(F("MCU: Product info received: %s"), this->getCommandAsString().c_str());
        //queryWorkingModeWiFi();
        if (processingState == STATE_PRODUCT_INFO_WAIT) {
          setProcessingState(STATE_PRODUCT_INFO_DONE);
        }
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
          this->gpioReset =  receivedCommand[7];
          knownCommand = true;
        }
        //after that, query the product info from MCU
        //..skipped
        //finally query the current state of device
        if (processingState == STATE_WIFI_WORKING_MODE_WAIT) {
          setProcessingState(STATE_WIFI_WORKING_MODE_DONE);
        }
        break;
      }
      case 0x03: {
        //ignore, MCU response to wifi state
        //55 aa 01 03 00 00
        network->debug(F("WiFi state: %s"), this->getCommandAsString().c_str());
        break;
      }
      case 0x04: {
        //Setup initialization request
    		//received: 55 aa 01 04 00 00
        network->debug(F("WiFi reset: %s"), this->getCommandAsString().c_str());
        //send answer: 55 aa 00 03 00 01 00
    		unsigned char configCommand[] = { 0x55, 0xAA, 0x00, 0x03, 0x00, 0x01, 0x00 };
    		commandCharsToSerial(7, configCommand);
    		onConfigurationRequest();
        break;
      }
      case 0x05: {
        //ignore, MCU response to wifi state
        //55 aa 01 03 00 00
        network->debug(F("Reset WiFi selection: %s"), this->getCommandAsString().c_str());
        break;
      }
      case 0x07: {
        knownCommand = processStatusCommand(receivedCommand[6], length);
        if (processingState == STATE_COMMAND_WAIT) {
          setProcessingState(STATE_COMMAND_DONE);
        }
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
        network->debug(F("MCU: %s"), this->getCommandAsString().c_str());
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

  void onConfigurationRequest() {
    //tbi
  }

  void commandCharsToSerial(unsigned int length, unsigned char* command, bool fNoQ = false, WTuyaDeviceState nextState = STATE_COMMAND_WAIT) {
    int chkSum = 0;
    bool fReady = ((!usingCommandQueue) ||
                   ((processingState != STATE_PRODUCT_INFO_WAIT) &&
                    (processingState != STATE_WIFI_WORKING_MODE_WAIT) &&
                    (processingState != STATE_COMMAND_WAIT && !fNoQ)));
    bool fAddQueue = false;
    if (fReady) {
      if (length > 2) {
        for (int i = 0; i < length; i++) {
          unsigned char chValue = command[i];
          chkSum += chValue;
          Serial.print((char)chValue);
        }
        unsigned char chValue = chkSum % 0x100;
        Serial.print((char)chValue);
      }
      setProcessingState(nextState);
      lastCommandSent = millis();
      //network->debug("commandCharsToSerial: %s", getBufferAsString(length, command).c_str());
    } else {
      fAddQueue = true;
      //network->debug("commandCharsToSerial: Command write in progress.... will Q");
    }
    if (usingCommandQueue) {
      // we have now sent something (if we can)
      // if we are in wait mode, then queue the command so that retry logic can be invoked
      if (processingState == STATE_COMMAND_WAIT)
        fAddQueue = true;
      if (fNoQ)
        fAddQueue = false;
      if (fAddQueue) {
        // queue the command for later
        //network->debug("commandCharsToSerial: Command Q'd");
        if (commandWriteQDepth < COMMAND_WRITE_Q_LENGTH) {
          byte* pBuffer = (byte *)malloc(sizeof(int) + length);
          memcpy(pBuffer+0, &length, sizeof(int));
          memcpy(pBuffer+sizeof(int), command, length);
          commandWriteQueue[commandWriteQDepth] = pBuffer;
          commandWriteQDepth++;
        } else {
          network->error(F("commandCharsToSerial: no space left in command write queue"));
        }
      }
    } else {
      //Don't use queue
      if (nextState == STATE_COMMAND_WAIT) {
        setProcessingState(STATE_IDLE);
      }
    }
  }

private :

  void setProcessingState(WTuyaDeviceState processingState) {
    if (this->processingState != processingState) {
      this->processingState = processingState;
      //network->debug(F("Thermostat processing state change. Old = %d, New = %d"), processingStateOld, processingState);
    }
  }

};

#endif
