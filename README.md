# ThermostatBecaWifi

This is a fork off https://github.com/klausahrenberg/WThermostatBeca/ to support my Tuya compatible MH-1823 wifi thermostats. CAVE: those originally use a WBR3 Module (RTL8720) which is incompatible with the ESP8266 firmware, so I replaced it with an ESP-07 (pin compatible). Unsoldering worked reasonably well with a hot air gun.

# Getting Started

Resolder MCU board, connect to its access point thermostat_f00bad with password 12345678, set Wifi and MQTT

For more info on the device and the fork see https://github.com/aoe1/WThermostatBeca/blob/master/MH-1823.md
