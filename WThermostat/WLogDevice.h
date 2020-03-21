#ifndef LOGDEVICE_H
#define	LOGDEVICE_H

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "../lib/WAdapter/Wadapter/WDevice.h"

const char* LOG_MODE_SILENT  = "silent";
const char* LOG_MODE_FATAL   = "fatal";
const char* LOG_MODE_ERROR   = "error";
const char* LOG_MODE_WARNING = "warning";
const char* LOG_MODE_NOTICE  = "notice";
const char* LOG_MODE_TRACE   = "trace";
const char* LOG_MODE_VERBOSE = "verbose";

class WLogDevice: public WDevice {
public:

    WLogDevice(WNetwork* network) 
        	: WDevice(network, "logging", "logging", network->getIdx(), DEVICE_TYPE_LOG) {
		
		this->providingConfigPage = true;
		this->configNeedsReboot = false;
		
		/* properties */
		this->logLevelByte = network->getSettings()->setByte("logLevelByte", LOG_LEVEL_WARNING);
		this->logLevelByte->setVisibility(NONE);
    	this->addProperty(logLevelByte);
		setlogLevelByte(constrain(getlogLevelByte(),LOG_LEVEL_SILENT, LOG_LEVEL_VERBOSE ));
    	this->logLevel = new WProperty("logLevel", "LogLevel", STRING);
    	this->logLevel->setAtType("logLevel");
    	this->logLevel->addEnumString(LOG_MODE_SILENT);
    	this->logLevel->addEnumString(LOG_MODE_FATAL);
    	this->logLevel->addEnumString(LOG_MODE_ERROR);
    	this->logLevel->addEnumString(LOG_MODE_WARNING);
    	this->logLevel->addEnumString(LOG_MODE_NOTICE);
    	this->logLevel->addEnumString(LOG_MODE_TRACE);
    	this->logLevel->addEnumString(LOG_MODE_VERBOSE);
		this->logLevel->setVisibility(MQTT);
    	this->addProperty(logLevel);
		// first apply stored Byte value from EEPROM
		this->setlogLevel(getlogLevelByte());
		// then set Handler, because now logLevelByte follows String logLevel
		this->logLevel->setOnChange(std::bind(&WLogDevice::onlogLevelChange, this, std::placeholders::_1));
    
    }

    virtual void printConfigPage(WStringStream* page) {
    	network->log()->notice(F("Log config page"));
    	page->printAndReplace(FPSTR(HTTP_CONFIG_PAGE_BEGIN), getId());
    	//ComboBox with model selection

    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_BEGIN), "Log Mode (Logging to MQTT Only!):", "lm");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "0", (getlogLevelByte() == LOG_LEVEL_SILENT  ? "selected" : ""), "Logging Disabled");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "1", (getlogLevelByte() == LOG_LEVEL_FATAL  ? "selected" : ""), "Fatal Messages");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "2", (getlogLevelByte() == LOG_LEVEL_ERROR   ? "selected" : ""), "Error Messages");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "3", (getlogLevelByte() == LOG_LEVEL_WARNING ? "selected" : ""), "Warning Messages");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "4", (getlogLevelByte() == LOG_LEVEL_NOTICE ? "selected" : ""), "Notice Messages");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "5", (getlogLevelByte() == LOG_LEVEL_TRACE ? "selected" : ""), "Trace Messages");
    	page->printAndReplace(FPSTR(HTTP_COMBOBOX_ITEM), "6", (getlogLevelByte() == LOG_LEVEL_VERBOSE ? "selected" : ""), "Verbose Messages");
    	page->print(FPSTR(HTTP_COMBOBOX_END));

    	page->print(FPSTR(HTTP_CONFIG_SAVE_BUTTON));
    }
    void saveConfigPage(ESP8266WebServer* webServer) {
        network->log()->notice(F("Log Beca config save lm=%s/%d"), webServer->arg("lm").c_str(), webServer->arg("lm").toInt());
        setlogLevelByte(constrain(webServer->arg("lm").toInt(),LOG_LEVEL_SILENT, LOG_LEVEL_VERBOSE ));
		this->setlogLevel(getlogLevelByte());
    }

    void loop(unsigned long now) {
        /* noop */
    }

    void handleUnknownMqttCallback(String stat_topic, String partialTopic, String payload, unsigned int length) {
		//logCommand(((String)"handleUnknownMqttCallback " + stat_topic + " / " + partialTopic + " / " + payload).c_str());
    }

	byte getlogLevelByte() {
    	return logLevelByte->getByte();
    }
	void setlogLevelByte(byte lm) {
		network->log()->notice(F("WLogDevice setlogLevelByte (%d)"), lm);
    	logLevelByte->setByte(lm);
		network->log()->setLevelNetwork(lm);
    }

    void onlogLevelChange(WProperty* property) {
		if (property->equalsString(LOG_MODE_SILENT)) setlogLevelByte(LOG_LEVEL_SILENT);
		else if (property->equalsString(LOG_MODE_FATAL)) setlogLevelByte(LOG_LEVEL_FATAL);
		else if (property->equalsString(LOG_MODE_ERROR)) setlogLevelByte(LOG_LEVEL_ERROR);
		else if (property->equalsString(LOG_MODE_WARNING)) setlogLevelByte(LOG_LEVEL_WARNING);
		else if (property->equalsString(LOG_MODE_NOTICE)) setlogLevelByte(LOG_LEVEL_NOTICE);
		else if (property->equalsString(LOG_MODE_TRACE)) setlogLevelByte(LOG_LEVEL_TRACE);
		else if (property->equalsString(LOG_MODE_VERBOSE)) setlogLevelByte(LOG_LEVEL_VERBOSE);
		else setlogLevelByte(LOG_LEVEL_SILENT);
    }

    void setlogLevel(byte lm) {
		const char * lms;
		lms=logLevelByteToString(lm);
		logLevel->setString(lms);
    }

	const char * logLevelByteToString(byte lm) {
		if (lm == LOG_LEVEL_SILENT) return LOG_MODE_SILENT;
		else if (lm == LOG_LEVEL_FATAL) return LOG_MODE_FATAL;
		else if (lm == LOG_LEVEL_ERROR) return LOG_MODE_ERROR;
		else if (lm == LOG_LEVEL_WARNING) return LOG_MODE_WARNING;
		else if (lm == LOG_LEVEL_NOTICE) return LOG_MODE_NOTICE;
		else if (lm == LOG_LEVEL_TRACE) return LOG_MODE_TRACE;
		else if (lm == LOG_LEVEL_VERBOSE) return LOG_MODE_VERBOSE;
		else return LOG_MODE_SILENT;
    }

	void sendLog(int level, const char* message) {
		String t = (String)network->getMqttTopic()+ "/" + MQTT_TELE +"/log" ;
		//network->log()->verbose(F("sendLog (%s)"), t.c_str());
		if (network->isMqttConnected()){
			network->publishMqtt(t.c_str(), ((String)logLevelByteToString(level)+": "+message).c_str());
		}
		//https://github.com/me-no-dev/EspExceptionDecoder
	};

private:
    WProperty* logLevel;
    WProperty* logLevelByte;

};


#endif