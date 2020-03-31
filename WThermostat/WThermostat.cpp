#include <Arduino.h>

#include "../lib/WAdapter/WAdapter/WNetwork.h"
#include "WBecaDevice.h"
#include "WClock.h"
#include "WLogDevice.h"

#define APPLICATION "Thermostat Beca"
#define VERSION "1.05b"

#ifdef DEBUG // use platform.io environment to activate/deactive 
#define SERIALDEBUG true  // enables logging to serial console
#else
#define SERIALDEBUG false
#endif


WNetwork *network;
WLogDevice *logDevice;
WBecaDevice *becaDevice;
WClock *wClock;

void setup() {
    Serial.begin(9600);
    // Wifi and Mqtt connection
    network = new WNetwork(SERIALDEBUG, APPLICATION, VERSION, true, NO_LED);
    network->setOnNotify([]() {
        if (network->isWifiConnected()) {
        }
        if (network->isMqttConnected()) {
            becaDevice->queryState();
            if (becaDevice->isDeviceStateComplete()) {
                // sendMqttStatus();
            }
        }
    });
    network->setOnConfigurationFinished([]() {
        // Switch blinking thermostat in normal operating mode back
        becaDevice->cancelConfiguration();
    });

    // KaClock - time sync
    wClock = new WClock(network, APPLICATION);
    network->addDevice(wClock);
    wClock->setOnTimeUpdate([]() { becaDevice->sendActualTimeToBeca(); });
    wClock->setOnError([](const char *error) {
        network->log()->error(F("Clock Error: %s"), error);
    });
    // Communication between ESP and Beca-Mcu
    becaDevice = new WBecaDevice(network, wClock);
    network->addDevice(becaDevice);

    becaDevice->setOnConfigurationRequest([]() {
        network->startWebServer();
        return true;
    });

    // add MQTTLog
    network->log()->notice(F("Loading LogDevice"));
    logDevice = new WLogDevice(network);
    network->addDevice(logDevice);
    network->log()->notice(F("Loading LogDevice Done"));
}

void loop() {
    network->loop(millis());
    delay(50);
}
