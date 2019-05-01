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
#define STATE_COMPLETE 5

const unsigned char COMMAND_START[] = {0x55, 0xAA};
const char AR_COMMAND_END = '\n';
const String SCHEDULE_WORKDAY = "workday";
const String SCHEDULE_SATURDAY = "saturday";
const String SCHEDULE_SUNDAY = "sunday";
const String SCHEDULE_HOUR = "h";
const String SCHEDULE_TEMPERATURE = "t";
const int FAN_NONE = -1;
const int FAN_AUTO = 0;
const int FAN_LOW  = 3;
const int FAN_MED  = 2;
const int FAN_HIGH = 1;

class BecaMcu {
public:
    typedef std::function<bool()> THandlerFunction;
    typedef std::function<bool(String)> TCommandHandlerFunction;
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
    void getMqttSchedules(JsonObject& json, String dayRange);
    //String schedulesToStr(int startAddr, String dayRange);
    //String getMqttSchedules();
    void setOnNotify(THandlerFunction onNotify);
    void setOnNotifyCommand(TCommandHandlerFunction onNotifyCommand);
    void setOnConfigurationRequest(THandlerFunction onConfigurationRequest);
    void setOnSchedulesChange(THandlerFunction onSchedulesChange);
    void setDeviceOn(bool deviceOn);
    void setDesiredTemperature(float desiredTemperature);
    void setManualMode(bool manualMode);
    void setEcoMode(bool ecoMode);
    void setLocked(bool locked);
    bool setSchedules(String payload);
    int getFanSpeed();
    String getFanSpeedAsString();
    void setFanSpeed(int fanSpeed);
    void setFanSpeedFromString(String fanSpeedString);
    void increaseFanSpeed();
    void decreaseFanSpeed();
    void setLogMcu(bool logMcu);
    bool isDeviceStateComplete();
private:
    KaClock *kClock;
    int receiveIndex;
    int commandLength;
    long lastHeartBeat;
    unsigned char receivedCommand[1024];
    void resetAll();
    int getIndex(unsigned char c);
    bool logMcu;
    void notifyMcuCommand(String commmandType);
    void processSerialCommand();
    bool deviceOn;
    float desiredTemperature;
    float actualTemperature;
    float actualFloorTemperature;
    bool manualMode;
    bool ecoMode;
    bool locked;
    int fanSpeed;
    byte schedules[54];
    boolean receivedStates[STATE_COMPLETE];
    boolean receivedSchedules;
    bool addSchedules(int startAddr, JsonArray& array);
    void addSchedules(int startAddr, JsonObject& json);
    bool setSchedules(int startAddr, JsonObject& json, String dayRange);
    THandlerFunction onNotify, onConfigurationRequest, onSchedulesChange;
    TCommandHandlerFunction onNotifyCommand;
    long lastNotify, lastScheduleNotify;
    void notifyState();
    void notifySchedules();
    void notifyUnknownCommand();
};


#endif

