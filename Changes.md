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
