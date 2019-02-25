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

#define HEARTBEAT_INTERVAL 10000
#define NOTIFY_INTERVAL 300000 //notify at least every 5 minutes
#define MINIMUM_INTERVAL 2000
#define STATE_COMPLETE 8

const unsigned char COMMAND_START[] = {0x55, 0xAA};
const char AR_COMMAND_END = '\n';
const String SCHEDULE_WORKDAY = "workday";
const String SCHEDULE_SATURDAY = "saturday";
const String SCHEDULE_SUNDAY = "sunday";
const String SCHEDULE_HOUR = "h";
const String SCHEDULE_TEMPERATURE = "t";

class BecaMcu {
public:
    typedef std::function<bool()> THandlerFunction;
    BecaMcu(bool debug, KaClock *kClock);
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
    void getMqttSchedules(JsonObject& json, String dayRange);
    //String schedulesToStr(int startAddr, String dayRange);
    //String getMqttSchedules();
    void setOnNotify(THandlerFunction onNotify);
    void setOnUnknownCommand(THandlerFunction onUnknownCommand);
    void setOnConfigurationRequest(THandlerFunction onConfigurationRequest);
    void setOnSchedulesChange(THandlerFunction onSchedulesChange);
    void setDeviceOn(bool deviceOn);
    void setDesiredTemperature(float desiredTemperature);
    void setManualMode(bool manualMode);
    void setEcoMode(bool ecoMode);
    void setLocked(bool locked);
    bool setSchedules(String payload);
private:
    KaClock *kClock;
    int receiveIndex;
    int commandLength;
    long lastHeartBeat;
    unsigned char receivedCommand[1024];
    void resetAll();
    int getIndex(unsigned char c);
    bool debug;
    void log(String debugMessage);
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
    boolean receivedStates[STATE_COMPLETE];
    bool addSchedules(int startAddr, JsonArray& array);
    void addSchedules(int startAddr, JsonObject& json);
    bool setSchedules(int startAddr, JsonObject& json, String dayRange);
    THandlerFunction onNotify, onConfigurationRequest, onUnknownCommand, onSchedulesChange;
    long lastNotify, lastScheduleNotify;
    void notifyState();
    void notifySchedules();
    void notifyUnknownCommand();
};


#endif

