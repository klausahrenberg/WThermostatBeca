#ifndef __KACLOCK_H__
#define __KACLOCK_H__

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

class KaClock {
public:
  typedef std::function<void(void)> THandlerFunction;
  typedef std::function<void(String)> TErrorHandlerFunction;
  KaClock(bool debug, String ntpServer);
  virtual ~KaClock();   
  void loop();
  void setOnTimeUpdate(THandlerFunction onTimeUpdate);
  void setOnError(TErrorHandlerFunction onError);
  unsigned long getEpochTime();
  byte getWeekDay();
  byte getWeekDay(unsigned long epochTime);
  byte getHours();
  byte getHours(unsigned long epochTime);
  byte getMinutes();
  byte getMinutes(unsigned long epochTime);
  byte getSeconds();
  byte getSeconds(unsigned long epochTime);
  int getYear();
  int getYear(unsigned long epochTime);
  byte getMonth();
  byte getMonth(unsigned long epochTime);
  byte getDay();
  byte getDay(unsigned long epochTime);
  String getTimeZone();
  String getFormattedTime();
  String getFormattedTime(unsigned long rawTime);
  bool isValidTime();  
  bool isClockSynced();
  long getDstOffset();
  long getRawOffset();  
  void getMqttState(JsonObject& json, bool complete);
private:    
  THandlerFunction onTimeUpdate;
  TErrorHandlerFunction onError;
  bool debug;
  unsigned long lastTry, lastNtpSync, lastTimeZoneSync, ntpTime, dstOffset, rawOffset;
  bool validTime;
  String timeZone;
  String ntpServer;
  void log(String debugMessage);
  void notifyOnTimeUpdate();
  void notifyOnError(String error);
};

#endif


