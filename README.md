# ThermostatBecaWifi
Replaces original Tuya firmware on Beca thermostat with ESP8266 wifi module. It's tested with model bac-002-wifi and should work with similar thermostats based on Tuya firmware. The model bac-002-wifi has 2 colors and can be purchased via Ali.

![image of thermostat](https://raw.githubusercontent.com/klausahrenberg/ThermostatBecaWifi/master/docs/bac-002-wifi.jpg)

And inside this device looks like this. On the right you can see the ESP8266 module (TYWE3S)

![thermostat inside](https://raw.githubusercontent.com/klausahrenberg/ThermostatBecaWifi/master/docs/bac-002-wifi-inside.png)

## Notice
Modifying and flashing of devices is at your own risk. I'm not responsible for bricked or damaged devices. I strongly recommend a backup of original firmware before installing any other software.
## Features
The firmware is not finished yet, actual only for testing purposes
### Working
* Configuration of AccessPoint and MQTT connection via Web-Interface
* NTP and time zone synchronisation to set the clock of thermostat
* Reading of parameters via MQTT: desiredTemperature, actualTemperature, actualFloorTemperature, deviceOn, manualMode, ecoMode, locked
* Setting of parameters via MQTT: desiredTemperature, deviceOn
### Still to do
* Setting of all other parameters
* Reading and Setting of time schedules
## Limitations
The thermostat is working independent from the Wifi-Module. That means, functionality of the thermostat itself will not and can't be changed. This firmware replaces only the communication part of the thermostat, which is handled by the ESP module.
I have tested this only with model bac-002-wifi - I assume, it should work with other devices too. The Tuya devices has a serial communication standard (MCU commands) which is only different in parameters. So if your Thermostat has not only a heating relay, for example an additional cooling circuit, it will not work actually. But there should arrive some unknown commands at the MQTT server.
## Installation
### 1. Connection to device for flashing
There are many ways to get the physical connection to ESP module. I soldered the connections on the device for flashing. Maybe there is a more elegant way to do that. It's quite the same, if you try to flash any other Sonoff devices to Tasmota. So get the inspiration for flashing there: https://github.com/arendst/Sonoff-Tasmota/wiki
### 2. Remove the power supply from thermostat during all flashing steps
Flasing will fail, if the thermostat is still powered during this operation.
### 3. Backup the original firmware
Don't skip this. In case of malfunction you need the original firmware. Tasmota has also a great tutorial for the right esptool commands: https://github.com/arendst/Sonoff-Tasmota/wiki/Esptool. So the backup command is:

```esptool -p <yourCOMport> -b 460800 read_flash 0x00000 0x100000 originalFirmware1M.bin```

for example:

```esptool -p /dev/ttyUSB0 -b 460800 read_flash 0x00000 0x100000 originalFirmware1M.bin```

### 4. Upload new firmware
Erase flash:

```esptool -p /dev/ttyUSB0 erase_flash```

Write firmware (1MB)

```esptool -p /dev/ttyUSB0 write_flash -fs 1MB 0x0 ThermostatBecaWifi.bin```

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
* From now on your MQTT broker should receive a <YOUR_TOPIC>/state json update at every 5 minutes or at every change on the device (if the device (or better the display) is on or off, it doesn't matter). The json state looks actual like:
```json
{
  "deviceOn":false,
  "desiredTemperature":21.5,
  "actualTemperature":20.5,
  "actualFloorTemperature":20,
  "manualMode":false,
  "ecoMode":false,
  "locked":false,
  "clockTime":"2019-02-21 10:40:55",
  "clockTimeRaw":1550745655,
  "validTime":true,
  "lastNtpSync": "2019-02-21 10:41:03",
  "lastTimeZoneSync":"2019-02-21 10:41:04",
  "dstOffset":0,
  "rawOffset":32400,
  "timeZone":"Asia/[..]",
  "firmware":"0.5"
 }
```
* You can set the parameters via MQTT with the parameter name and the direct value as payload: <YOUR_TOPIC>/<parameter> and value in payload. e.g.: <YOUR_TOPIC>/desiredTemperature; payload: 22.5
### Don't like or it doesn't work?
Flash the original firmware (see installation). Instead of flashing ```ThermostatBecaWifi.bin```, use your saved ```originalFirmware1M.bin``` to restore your original firmware. Write me a message with your exact model and which parameter was not correct. Maybe your MQTT-server received some messages mcucommand/unknown - this would be also helpful for me. Again: I have tested this only with model bac-002-wifi. If you have another device, don't expect that this is working directly.
### Build this firmware from source
For build from sources you can use the Arduino-IDE or Sloeber. All sources needed are inside the folder 'src'. You will need some libraries: esp8266, ArduinoJson, DNSServer, EEPROM, NTPClient, TimeLib - It's all available via board and library manager inside of ArduinoIDE
