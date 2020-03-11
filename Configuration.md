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
* Fill out 'Things IDX' (unique id of your choice), 'SSID' (only 2G network), 'Password' for Wifi
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
  "topic":"<your_topic>/[cmnd|stat]/things/thermostat"
}
```
## 4. Configure clock settings
Normally you don't need to change options here.
* Open configuration page at `http://<device_ip>/config`
* Goto 'Configure clock'
* Modify 'NTP server' for time synchronisation
* Modify 'Time zone request' for time offset synchronisation depending on your location
