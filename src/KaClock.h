#ifndef __KACLOCK_H__
#define __KACLOCK_H__

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

class KaClock {
public:
  typedef std::function<void(void)> THandlerFunction;
  typedef std::function<void(String)> TErrorHandlerFunction;
  KaClock(bool debug);
  virtual ~KaClock();   
  void loop();
  void setOnTimeUpdate(THandlerFunction onTimeUpdate);
  void setOnError(TErrorHandlerFunction onError);
  unsigned long getEpochTime();
  byte getWeekDay();
  byte getWeekDay(unsigned long epochTime);
  byte getHours();
  byte getMinutes();
  byte getSeconds();
  int getYear();
  byte getMonth();
  byte getDay();
  String getTimeZone();
  String getFormattedTime();
  String getFormattedTime(unsigned long rawTime);
  bool isValidTime();  
  bool isClockSynced();
  long getDstOffset();
  long getRawOffset();  
  void getMqttState(JsonObject& json);
private:    
  THandlerFunction onTimeUpdate;
  TErrorHandlerFunction onError;
  bool debug;
  unsigned long lastTry, lastNtpSync, lastTimeZoneSync, ntpTime, dstOffset, rawOffset;
  bool validTime;
  String timeZone;
  void log(String debugMessage);
  void notifyOnTimeUpdate();
  void notifyOnError(String error);
};

#endif


