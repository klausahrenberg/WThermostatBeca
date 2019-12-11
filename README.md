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
## Json structure
Firmware provides 3 different json messages:
1. State report  
MQTT: State report is provided every 5 minutes or at change of a parameter  
Webthing: State report can be requested by: http://<device_ip>/things/thermostat/properties  
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
2. Schedules
3. Device
### 1. State report 
### 2. Schedules
### 3. Device

## First steps
* Remove all connections from flashing
* Power on the device. The device will operate normally because this is untouched by the ESP module.
* Switch the device off with the power button. The device is not really off, only the display.
* Hold the 'Temp down' button for 5 to 8 seconds, until the Wifi logo and display is blinking. This is the same procedure like connecting to the Tuya-Android-App
* The ESP will start a Wifi Access Point. SSID is 'Thermostat_Beca-Wifi_ChipID', the password is '12345678'. Connect to this AP
* Open the web browser and go to '192.168.4.1', the configuration page will prompt up. Go to 'Configure device'. There you can type in the parameters for your router(2G), password and also the connection to your MQTT broker, topic(<YOUR_TOPIC>) and user.

![Setup](https://raw.githubusercontent.com/klausahrenberg/ThermostatBecaWifi/master/docs/Setup_Wifi_MQTT.png)
* Save settings, the ESP will restart, the thermostat is switched off
* Switch on the device. The clock should update automaticly within 30 seconds via NTP. If not, the wireless connection failed. Restart the configuration.
## MQTT structure
Your MQTT broker receive all messages to <YOUR_TOPIC>. The following commands will be send from the device:
1. <YOUR_TOPIC>/state at every change on the device and/or every 5 minutes
2. <YOUR_TOPIC>/schedules at the start of the device, at every change or request
3. <YOUR_TOPIC>/mcucommand can contain 2 strings: 'unknown' for unknown commands received from MCU or 'mcu: <0x0.>' for logged messages from MCU, if command 'logMcu' enabled this before (default: disabled)

You can send the following commands to the device:
1. <YOUR_TOPIC>/schedules/0 to request for the schedules. The schedules come back in 3 separate messages for workday, saturday and sunday
2. <YOUR_TOPIC>/state/0 to request the state record of the device.  
3. <YOUR_TOPIC>/clock/0 to request detail info about time synchronization results.  
4. <YOUR_TOPIC>/webServer/true|false can be called with an bool to switch the web service on or off
5. <YOUR_TOPIC>/mcucommand sends directly serial mcu commands to the device. The has to be a string in hexa-form without the checksum byte at the end, e.g. to set the desired temperature to 24.5C: "55 aa 01 07 00 08 02 02 00 04 00 00 00 31". This command is only for testing of unknown Tuya MCU commands and will not be required for regular work with the device.
6. <YOUR_TOPIC>/logMcu/true|false enables or disables forwarding of all MCU messages to the MQTT broker. Only for testing.

### State record - json structure
The state record of the device is send in follwing json structure:
```json
{
  "deviceOn":false,
  "desiredTemperature":21.5,
  "actualTemperature":20.5,
  "actualFloorTemperature":20, //only_BHT-002-GBLW
  "manualMode":false,
  "ecoMode":false,
  "locked":false,
  "fanSpeed":"auto|low|medium|high", //only_BAC-002-ALW
  "systemMode":"cooling|heating|ventilation", //only_BAC-002-ALW
  "thermostatModel": "BHT-002-GBLW",
  "logMcu": false,
  "schedulesDayOffset": 0,
  "weekend": false, //true_indicates_that_weekend_schedule_is_running_at_device
  "clockTime": "2019-05-03 12:04:26",
  "validTime": true,
  "timeZone": "Asia/[...]",
  "lastNtpSync": "2019-05-03 12:03:30",
  "firmware": "0.91",  
  "ip":"192.168.0.174",
  "webServerRunning":false
 }
```
You can set the parameters via MQTT with the parameter name and the direct value as payload: <YOUR_TOPIC>/<parameter> and value in payload. e.g.: <YOUR_TOPIC>/desiredTemperature; payload: 22.5
### Schedules - json structure
The schedules are only sent once at the start, at every change or on request. The structure of json (for workday): 
```json
{
  "workday":{
    "0":{"h":"06:00","t":20},
    "1":{"h":"08:00","t":15},
    "2":{"h":"11:30","t":15},
    "3":{"h":"13:30","t":15},
    "4":{"h":"17:00","t":22},
    "5":{"h":"22:00","t":15}
  }
}  
```  
Same 2 other messages are send for "saturday" and "sunday".
The schedules can be set/modified with same structure or only parts of it. To set for example the schedule 0 for workday, send <YOUR_TOPIC>/schedules with following json:
```json
{
  "workday":{
    "0":{"h":"6:00","t":22}
  }
} 
```  
Only the specified parts of schedules will be changed.
### Don't like or it doesn't work?
Flash the original firmware (see installation). Write me a message with your exact model and which parameter was not correct. Maybe your MQTT-server received some unknown messages - this would be also helpful for me. Again: I have tested this only with model BHT-002-GBLW. If you have another device, don't expect that this is working directly.
### Build this firmware from source
For build from sources you can use the Arduino-IDE or Sloeber. All sources needed are inside the folder 'src'. You will need some libraries: esp8266, ArduinoJson, DNSServer, EEPROM, NTPClient, TimeLib - It's all available via board and library manager inside of ArduinoIDE
