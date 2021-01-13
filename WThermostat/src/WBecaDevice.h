#ifndef BECAMCU_H
#define	BECAMCU_H

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "WThermostat.h"



//const char AR_COMMAND_END = '\n';
//const char* STATE_COOLING = "cooling";


//const float MODEL_TEMPERATURE_FACTOR[]          = {2.0f, 2.0f, 10.0f, 10.0f, 1.0f, 10.0f, 1.0f };
//const byte  MODEL_MCU_BYTE_TEMPERATURE_TARGET[] = {0x02, 0x02, 0x02, 0x02, 0x10, 0x02, 0x10 };
//const byte  MODEL_MCU_BYTE_TEMPERATURE_ACTUAL[] = {0x03, 0x03, 0x08, 0x03, 0x18, 0x03, 0x18 };
//const byte  MODEL_MCU_BYTE_TEMPERATURE_FLOOR[]  = {0x66, 0x00, 0x05, 0x66, 0x2d, 0x00, 0x65 };
//const byte  MODEL_MCU_BYTE_SYSTEM_MODE[]  		  = {0x00, 0x66, 0x00, 0x00, 0x24, 0x00, 0x00 };
//const byte  MODEL_MCU_BYTE_SCHEDULES_MODE[]		  = {0x04, 0x04, 0x03, 0x04, 0x02, 0x04, 0x02 };
//const byte  MODEL_MCU_BYTE_STATUS_MODE[]  	  	= {0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00 };
//const byte  MODEL_MCU_BYTE_FAN_MODE[]  			    = {0x00, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00 };
//const byte  MODEL_MCU_BYTE_SCHEDULES[]          = {0x65, 0x68, 0x00, 0x00, 0x26, 0x2b, 0x6c };
//const byte  MODEL_MCU_BYTE_LOCKED[]             = {0x06, 0x06, 0x06, 0x06, 0x28, 0x08, 0x28 };
//const byte  MODEL_SCHEDULING_DAYS[]             = {18,   18,   18,   18,   8,    8,    8 };
//const byte  MODEL_SCHEDULING_HH_POS[]           = {1,    1,    1,    1,    0,    0,    0 };
//const byte  MODEL_SCHEDULING_MM_POS[]           = {0,    0,    0,    0,    1,    1,    1 };


class WBecaDevice: public WThermostat {
public:
	byte getThermostatModel() {
		return 0;
	}

  WBecaDevice(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {

		network->debug(F("Standard WBecaDevice created"));

  }


    /*void bindWebServerCalls(AsyncWebServer* webServer) {
    	String deviceBase("/things/");
    	deviceBase.concat(getId());
    	deviceBase.concat("/");
    	deviceBase.concat(SCHEDULES);
    	webServer->on(deviceBase.c_str(), HTTP_GET, std::bind(&WBecaDevice::sendSchedules, this, std::placeholders::_1));
    }*/

		virtual void loop(unsigned long now) {

			WThermostat::loop(now);
		}

protected:



private:


};


#endif
