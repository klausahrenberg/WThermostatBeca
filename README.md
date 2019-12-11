# ThermostatBecaWifi
Replaces original Tuya firmware on Beca thermostat with ESP8266 wifi module. The firmware is tested with following devices:
* BHT-002-GBLW, BHT-6000 (floor heating)
* BAC-002-ALW (heater, cooling, ventilation)
* BHT-002-GCLW (Water/Gas Boiler)
## Features
* Enables thermostat to communicate via MQTT and/or Mozilla Webthings
* Configuration of connection and devixe parameters via web interface
* NTP and time zone synchronisation to set the clock of thermostat
* Reading and setting of all parameters via MQTT
* Reading and setting of main parameters vie Webthing
* Only BHT-002-GBLW: actualFloorTemperature (external temperature sensor)
* Only BAC-002-ALW: fanSpeed:auto|low|medium|high; systemMode:cooling|heating|ventilation
* Reading and setting of time schedules via MQTT
## Installation
To install the firmware, follow instructions here:  
https://github.com/klausahrenberg/WThermostatBeca/blob/master/Flashing.md
## Integration in Webthings
Since version 0.96 this firmware supports Mozilla Webthings directly. With webthings you can control the thermostat via the Gateway - inside and also outside of your home network. No clunky VPN, dynDNS solutions needed to access your home devices. I recommend to run the gateway in parallel to an MQTT server and for example Node-Red. Via MQTT you can control the thermostat completely and logic can be done by Node-Red. Webthings is used for outside control of main parameters.  
Add the device to the gateway via '+' icon. After that you have the new nice and shiny icon in the dashboard:  
![webthing_icon](https://github.com/klausahrenberg/WThermostatBeca/blob/master/docs/Webthing_Icon.png)  
The icon shows the actual temperature and heating state.  
There is also a detailed view available:  
<img src="https://github.com/klausahrenberg/WThermostatBeca/blob/master/docs/Webthing_Complete.png" width="400">

## Json structure
Firmware provides 3 different json messages:
1. State report  
2. Schedules
3. Device (at start of device to let you know the topics and ip)
### 1. State report 
**MQTT:** State report is provided every 5 minutes, at change of a parameter or at request via message with empty payload to `<your_topic>/things/thermostat/properties`  
**Webthing:** State report can be requested by: `http://<device_ip>/things/thermostat/properties`  
```json
{
  "idx":"thermostat_beca",
  "ip":"192.168.0.xxx",
  "firmware":"x.xx",
  "temperature":21.5,
  "targetTemperature":23,
  "deviceOn":true,
  "schedulesMode":"off|auto",
  "ecoMode":false,
  "locked":false,
  "state":"off|heating", //only_available,_if_hardware_modified
  "floorTemperature":20, //only_BHT-002-GBLW
  "fanMode":"auto|low|medium|high", //only_BAC-002-ALW
  "systemMode":"cool|heat|fan_only" //only_BAC-002-ALW
}
```
### 2. Schedules
**MQTT:** Request actual schedules via message with empty payload to `<your_topic>/things/thermostat/schedules`
**Webthing:** State report can be requested by: `http://<device_ip>/things/thermostat/schedules`  
```json
{
  "w1h":"06:00",
  "w1t":20,
  "w2h":"08:00",
  "w2t":15,
  ...
  "w6h":"22:00",
  "w6t":15,
  "a1h":"06:00",
  ...
  "a6t":15,
  "u1h":"06:00",
  ...
  "u6t":15
}
```
### 3. Device
**MQTT:** At start of device to let you know the topics and ip to `devices/thermostat`  
**Webthing:** n.a.
```json
{
  "url":"http://192.168.0.xxx/things/thermostat",
  "ip":"192.168.0.xxx",
  "topic":"thermostat_beca/things/thermostat"
}
```
## Modifying parameters via MQTT
Send a json with changed parameters to `<your_topic>/things/thermostat/properties`.  
Send a json with changed schedules to `<your_topic>/things/thermostat/schedules`.
### Don't like or it doesn't work?
Flash the original firmware (see installation). Write me a message with your exact model and which parameter was not correct. Maybe your MQTT-server received some unknown messages - this would be also helpful for me. Again: I have tested this only with model BHT-002-GBLW. If you have another device, don't expect that this is working directly.
### Build this firmware from source
For build from sources you can use the Arduino-IDE, Sloeber or other. All sources needed are inside the folder 'WThermostat' and my other library: https://github.com/klausahrenberg/WAdapter. Additionally you will need some other libraries: DNSServer, EEPROM (for esp8266), ESP8266HTTPClient, ESP8266mDNS, ESP8266WebServer, ESP8266WiFi, Hash, NTPClient, Time - It's all available via board and library manager inside of ArduinoIDE
