#ifndef BECAMCU_H
#define	BECAMCU_H

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "ArduinoJson.h"
#include "KaClock.h"

#define HEARTBEAT_INTERVAL 10000
#define NOTIFY_INTERVAL 300000 //notify at least every 5 minutes
#define MINIMUM_INTERVAL 2000
#define STATE_COMPLETE 5

const String MODEL_BHT_002_GBLW = "BHT-002-GBLW"; //Heater
const String MODEL_BAC_002_ALW  = "BAC-002-ALW"; //Heater, Cooling, Ventilation

const unsigned char COMMAND_START[] = {0x55, 0xAA};
const char AR_COMMAND_END = '\n';
const String SCHEDULE_WORKDAY = "workday";
const String SCHEDULE_SATURDAY = "saturday";
const String SCHEDULE_SUNDAY = "sunday";
const String SCHEDULE_HOUR = "h";
const String SCHEDULE_TEMPERATURE = "t";
const byte FAN_SPEED_NONE = 0xFF;
const byte FAN_SPEED_AUTO = 0x00;
const byte FAN_SPEED_LOW  = 0x03;
const byte FAN_SPEED_MED  = 0x02;
const byte FAN_SPEED_HIGH = 0x01;
const byte SYSTEM_MODE_NONE        = 0xFF;
const byte SYSTEM_MODE_COOLING     = 0x00;
const byte SYSTEM_MODE_HEATING     = 0x01;
const byte SYSTEM_MODE_VENTILATION = 0x02;

const byte STORED_FLAG = 0x36;

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
    float getActualTemperature();
    float getActualFloorTemperature();
    void setManualMode(bool manualMode);
    void setEcoMode(bool ecoMode);
    void setLocked(bool locked);
    bool setSchedules(String payload);
    byte getFanSpeed();
    String getFanSpeedAsString();
    void setFanSpeed(byte fanSpeed);
    void setFanSpeedFromString(String fanSpeedString);
    void increaseFanSpeed();
    void decreaseFanSpeed();
    byte getSystemMode();
    String getSystemModeAsString();
    void setSystemMode(byte systemMode);
    void setSystemModeFromString(String systemModeString);
    void setLogMcu(bool logMcu);
    bool isDeviceStateComplete();
    signed char getSchedulesDayOffset();
    void setSchedulesDayOffset(signed char schedulesDayOffset);
private:
    KaClock *kClock;
    int receiveIndex;
    int commandLength;
    long lastHeartBeat;
    unsigned char receivedCommand[1024];
    void resetAll();
    int getIndex(unsigned char c);
    bool logMcu;
    byte getDayOfWeek();
    bool isWeekend();
    void notifyMcuCommand(String commmandType);
    void processSerialCommand();
    bool deviceOn;
    float desiredTemperature;
    float actualTemperature;
    float actualFloorTemperature;
    bool manualMode;
    bool ecoMode;
    bool locked;
    byte fanSpeed, systemMode;
    byte schedules[54];
    boolean receivedStates[STATE_COMPLETE];
    byte schedulesDataPoint;
    String thermostatModel;
    signed char schedulesDayOffset;
    boolean receivedSchedules();
    bool addSchedules(int startAddr, JsonArray& array);
    void addSchedules(int startAddr, JsonObject& json);
    bool setSchedules(int startAddr, JsonObject& json, String dayRange);
    THandlerFunction onNotify, onConfigurationRequest, onSchedulesChange;
    TCommandHandlerFunction onNotifyCommand;
    long lastNotify, lastScheduleNotify;
    void notifyState();
    void notifySchedules();
    void notifyUnknownCommand();
    void loadSettings();
    void saveSettings();
};


#endif

