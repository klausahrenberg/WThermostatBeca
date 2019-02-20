/* 
 * File:   SerialManager.h
 * Author: klausahrenberg
 *
 * Created on 15. February 2019
 */

#ifndef BECAMCU_H
#define	BECAMCU_H

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include "ArduinoJson.h"
#include "KaClock.h"

const unsigned char COMMAND_START[] = {0x55, 0xAA};
const char AR_COMMAND_END = '\n';
#define HEARTBEAT_INTERVAL 10000
#define NOTIFY_INTERVAL 300000 //notify at least every 5 minutes
#define MINIMUM_INTERVAL 2000

class BecaMcu {
public:
    typedef std::function<bool()> THandlerFunction;
    BecaMcu(KaClock *kClock);
    ~BecaMcu();
    void loop();
    unsigned char* getCommand();
    int getCommandLength();
    String getCommandAsString();
    void commandHexStrToSerial(String command);
    void commandCharsToSerial(unsigned int length, unsigned char* command);
    void queryState();
    void cancelConfiguration();
    void sendActualTimeToBeca();
    void getMqttState(JsonObject& json);
    void setOnNotify(THandlerFunction onNotify);
    void setOnUnknownCommand(THandlerFunction onUnknownCommand);
    void setOnConfigurationRequest(THandlerFunction onConfigurationRequest);
    void setDeviceOn(bool deviceOn);
    void setDesiredTemperature(float desiredTemperature);
private:
    KaClock *kClock;
    int receiveIndex;
    int commandLength;
    long lastHeartBeat;
    unsigned char receivedCommand[1024];
    void resetAll();
    int getIndex(unsigned char c);

    void processSerialCommand();
    bool isDeviceStateComplete();
    bool deviceOn;
    float desiredTemperature;
    float actualTemperature;
    float actualFloorTemperature;
    bool manualMode;
    bool ecoMode;
    bool locked;
    byte schedules[54];
    boolean receivedStates[7];
    void addSchedules(byte startAddr, JsonObject& json);

    THandlerFunction onNotify, onConfigurationRequest, onUnknownCommand;
    bool notifyImmediatly;
    long lastNotify;
    void notify(bool immediatly);
    void notifyUnknownCommand();
};


#endif

