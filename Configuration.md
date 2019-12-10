# Configuration of Thermostat
This document describes the first configuration steps after flashing of firmware. The firmware supports MQTT messaging and 
Mozilla Webthings. Both can be running parallel.

Steps are in general:
* Configure network access
* Configure MQTT (optional)
* Configure thermostat device (model selection)
* Add Thermostat to Webthings IOT Gateway (optional)

## Configure Network access
* The thermostat opens an Access Point when it's started first time after flashing. 
* The AccessPoint is named 'Thermostat-Beca_xxxxxx'. Default password is '12345678'
* After connection open 'http://192.168.4.1' in a web browser
* Goto 'Network configuration'
* Fill out 'Things IDX' (unique id of your choice), 'SSID' (only 2G network), 'Password' for Wifi
* Leave 'Support Mozilla WebThings' checked (recommended). If this is checked, the thermostat will always run the web interface. 
You don't need to use Webthings itself.
* If you don't want to use MQTT, press 'Save Configuration' and wait for reboot of device.

##Configure MQTT (optional)
* Stay at page 'Network configuration'
* Select checkbox 'Support MQTT', web page will extend
* Fill out 'MQTT Server', 'MQTT User' (optional), 'MQTT password' (optional) and 'MQTT topic'
* Press 'Save Configuration' and wait for reboot of device.

Configure thermostat device (model selection)
