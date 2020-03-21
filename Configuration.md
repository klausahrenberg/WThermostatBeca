# Configuration of Thermostat
This document describes the first configuration steps after flashing of firmware. The firmware supports MQTT messaging and 
Mozilla Webthings. Both can be running parallel.

Steps are in general:
1. Configure thermostat device (model selection)
2. Configure network access
3. Configure MQTT (optional)
4. Configure clock settings


## 1. Configure thermostat device (model selection)
* The thermostat opens an Access Point when it's started first time after flashing. 
* The AccessPoint is named `Thermostat-Beca_xxxxxx`. Default password is `12345678`
* After connection open `http://192.168.4.1` in a web browser
* Goto 'Configure device'
* Choose your thermostat model
* Choose, if heating relay monitor is supported, hw modification need to work, see https://github.com/klausahrenberg/WThermostatBeca/issues/17#issuecomment-552078026
* Choose work day and weekend start in your region
* Press 'Save Configuration' and wait for reboot of device.

## 2. Configure Network access
* Goto 'Configure network'
* Fill out 'Hostname/ IDX' (unique id of your choice), 'SSID' (only 2G network), 'Password' for Wifi
* Leave 'Support Mozilla WebThings' checked (recommended). If this is checked, the thermostat will always run the web interface. 
You don't need to use Webthings itself.
* If you don't want to use MQTT, press 'Save Configuration' and wait for reboot of device.

## 3. Configure MQTT (optional)
* Stay at page 'Network configuration'
* Select checkbox 'Support MQTT', web page will extend
* Fill out 'MQTT Server', 'MQTT User' (optional), 'MQTT password' (optional) and 'MQTT topic'
* Press 'Save Configuration' and wait for reboot of device.
* After restart the thermostat sends 2 MQTT messages to topics 'devices/thermostat' and 'devices/clock' to let you know the IP and MQTT topic of the device. The json message looks like:
```json
{
  "url":"http://192.168.0.xxx/things/thermostat",
  "ip":"192.168.0.xxx",
  "topic":"<your_topic>/[cmnd|stat|tele]/things/thermostat"
}
```
## 4. Configure clock settings
Normally you don't need to change options here.
* Open configuration page at `http://<device_ip>/`
* Goto 'Configure clock'
* Modify 'NTP server' for time synchronisation
* Modify 'Time zone' for time offset synchronisation depending on your location
  * Set Timezone to values -13:30 .. 13:30 for fixed timezone
  * Set Timezone to 99 for Daylight Saving
  * DST/STD: W,M,D,h,T
  W = week (0 = last week of month, 1..4 = first .. fourth)
  M = month (1..12)
  D = day of week (1..7 1 = sunday 7 = saturday)
  h = hour (0..23)
  T = timezone (-810..810) (offset from UTC in MINUTES - 810min / 60min=13:30)
  * It's adopted from http://tasmota.github.io/ - but you don't need the Hemisphere Setting (first number), its calculated
* Examples:

  | Region | Timezone | DST | STD |
  | ----------|----------|-----|----- |
  | UTC | 0 |  |  |
  | Pacific/Honolulu | 10 |  |  |
  | Asia/Beijing | -8 |  |  |
  | Europe/Berlin | 99 |  0,3,0,2,120 | 0,10,0,3,60 |
  | Europe/London | 99 |  0,3,0,2,60 | 0,10,0,3,0 |
  | America/New_York | 99 | 2,3,0,2,-240 | 1,11,0,2,-300
