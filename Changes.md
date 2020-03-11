## Version 1.01b
* MQTT Topics devided into 'cmnd' for receiving and 'stat' for sending 
* Added new paramter 'Mode' controlling and monitoring both parameters deviceOn and schedulesMode
* Better Home Assitant Support
* by Folke Ashberg <folke@ashberg.de>

## Version 1.00
* Fix: ESP disconnected and was not reconnecting automaticly

## Version 0.99
* Serveral fixes for webinterface (e.g. Upload in Chrome was not working)
* Fix schedules. ESP was hanging in endless loop in last 2 schedules of day

## Version 0.98
* Webserver can't be disabled anymore to ensure that thermostat can configered all the time
* MQTT port can be configured now
* Serveral fixes for webinterface (Saved message, no timeout, clock settings)

## Version 0.97
* Improved behavior for targetTemperature. If target is changed via MQTT/Webthing and thermostat is in 'auto mode', the thermotat switches in 'manual mode' now. Before only the target for 'manual mode' was changed, but had no effect when thermostat was in 'auto mode'
* Fix: targetTemperature gives correct actual value now, when in 'auto mode'

## Version 0.96
* Supports Mozilla Webthings, properties like temperature, desiredTemperature, On/Off, manualMode are available in a fancy web interface which can control the device outside your home network also. MQTT still supports more properties.
* Device configuration via Web-Interface added for things like: Model, NTP-Server, Timezone API, Weekday offset
* Heating relay status supported. To make this work, a hardware modification is needed: 2x10kOhm resistors have to be soldered to IO 5 of the ESP. For detailed description with pictures, see here: https://github.com/klausahrenberg/ThermostatBecaWifi/issues/17#issuecomment-552078026
* ArduinoJson library removed, own json creation and parsing implemented
* ESPAsyncWebServer library removed because of memory issues
* Fix: Automatic reconnection after loss of network should work now
## Version 0.95
* Fix: Buffer for time zone sync increased again to 1024. Length of "http://worldtimeapi.org/api/ip" response had increased because of new parameters
## Version 0.94
* Fix: Schedule temperature can now set in 0.5 steps
## Version 0.93
* Fix: Property "systemMode": Changing of cooling mode at the device should trigger an mqtt message now.
* Fix: Property "schedules": Changing of schedules at device should trigger mqtt messages now.
## Version 0.92
* Fix: Property "systemMode" corrected
## Version 0.91
* Property "systemMode" for model BAC-002-ALW added; possible values: "cooling"|"heating"|"ventilation"
* Property "schedulesDayOffset" added (default: 0); details see description
* Property "thermostatModel" added; values "BHT-002-GBLW"|"BAC-002-ALW"; only readable. Model will be configered by the firmware based on the results of the MCU at device initialization
* Property "actualFloorTemperature" only in device state message included at model BHT-002-GBLW
* Property "fanSpeed" and "systemMode" only in device state message included at model BAC-002-ALW
* Removed some detail informations about the time sync from standard device state message. Details must be requered with a separate clock command
* Fix: schedules for model BAC-002-ALW should now read correctly out of the device
* Fix: wrong clock time zone calculation corrected, if DST was active
## Version 0.9
* Fix: logMcu command is now working
* Fix: webService command is now working: README.md corrected to command 'webServer'. However, now are both words working.
* Fix: locked command is now working with payload false
* Fix: changing the webServer command does invoke a state update over mtqq now
## Version 0.8
* Support for fan at some models with property "fanSpeed" wit states "auto"|"low"|"medium"|"high". If no fan is available at thermostat, "fanSpeed" will always stay at "none"
* With MQTT parameter "logMcu" true|false raw commands from MCU will forwarded to MQTT. By default disabled/false
* IP address added in json state message
* Fix: Devices with cooling/heating function do not sent actualTemperature, actualFloorTemperature and schedules at initialization - the result was, that never a device state was reported to MQTT because the information was incomplete. The state will be send now, even not all information are available yet
## Version 0.7:
* increased JSON buffer in KaClock.cpp to prevent parsing failure
* definition of NTP_SERVER in ThermostatBecaWifi.ino to set a NTP server near by
* state request via mqtt
* state record includes now IP address of thermostat and if webServer is Running or not
## Version 0.6
* initial version
