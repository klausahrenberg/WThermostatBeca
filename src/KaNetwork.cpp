#include "KaNetwork.h"
#include <EEPROM.h>

const String CONFIG_PASSWORD = "12345678";
const byte STORED_FLAG = 0x27;

WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;
WiFiClient wifiClient;
KaPubSubClient *mqttClient;

KaNetwork::KaNetwork(String applicationName, String firmwareVersion, bool debug,
bool startConfigIfNoSettingsFound) {
	WiFi.mode(WIFI_STA);
	this->applicationName = applicationName;
	this->firmwareVersion = firmwareVersion;
	this->mqttClient = new KaPubSubClient(debug, wifiClient);
	this->dnsApServer = nullptr;
	this->apServer = nullptr;
	this->debug = debug;
	lastMqttConnect = lastWifiConnect = 0;
	networkState = NETWORK_NOT_CONNECTED;
	gotIpEventHandler = WiFi.onStationModeGotIP(
			[this](const WiFiEventStationModeGotIP& event) {
				log("Station connected, IP: " + WiFi.localIP());
				this->networkState = NETWORK_CONNECTED;
				this->notify();
			});
	disconnectedEventHandler = WiFi.onStationModeDisconnected(
			[this](const WiFiEventStationModeDisconnected& event) {
				log("Station disconnected");
				this->networkState = NETWORK_NOT_CONNECTED;
				this->notify();
			});
	mqttClient->setCallback(
			[this] (char* topic, byte* payload, unsigned int length) {
				this->mqttCallback(topic, payload, length);
			});
	if (loadSettings()) {
		mqttClient->setServer(mqttServer, 1883);
	} else if (startConfigIfNoSettingsFound) {
		this->startConfiguration();
	}
}

KaNetwork::~KaNetwork() {

}

String readString(int address, int length) {
	char data[length]; //Max 100 Bytes
	for (int i = 0; i < length; i++) {
		byte k = EEPROM.read(address + i);
		data[i] = k;
		if (k == '\0') {
			break;
		}
	}
	return String(data);
}

void writeString(int address, int length, String value) {
	int size = value.length();
	if (size + 1 >= length) {
		size = length - 1;
	}
	for (int i = 0; i < size; i++) {
		EEPROM.write(address + i, value[i]);
	}
	EEPROM.write(address + size, '\0');
}

bool KaNetwork::loadSettings() {
	EEPROM.begin(512);
	//1 Byte - settingsStored flag
	settingsStored = (EEPROM.read(0) == STORED_FLAG);
	if (settingsStored) {
		this->ssid = readString(1, 33);
		this->password = readString(34, 65);
		this->mqttServer = readString(99, 33);
		this->mqttTopic = readString(132, 65);
		this->mqttUser = readString(197, 33);
		this->mqttPassword = readString(230, 65);
		log(
				"Settings loaded successfully. SSID '" + ssid
						+ "'; MQTT server '" + mqttServer + "'");
	} else {
		log("No stored settings found");
	}
	EEPROM.end();
	return settingsStored;
}

void KaNetwork::saveSettings() {
	EEPROM.begin(512);
	writeString(1, 33, this->ssid);
	writeString(34, 65, this->password);
	writeString(99, 33, this->mqttServer);
	writeString(132, 65, this->mqttTopic);
	writeString(197, 33, this->mqttUser);
	writeString(230, 65, this->mqttPassword);
	//settingsStored 0x25
	EEPROM.write(0, STORED_FLAG);
	EEPROM.commit();
	EEPROM.end();
}

bool KaNetwork::loop(bool waitForWifiConnection) {
	if (!isInConfigurationMode()) {
		if ((ssid != "") && (mqttServer != "")) {
			long now = millis();
			//WiFi connection
			if ((WiFi.status() != WL_CONNECTED)
					&& ((lastWifiConnect == 0)
							|| (now - lastWifiConnect > 300000))) {
				log("Connecting to '" + ssid + "'");
				//Workaround: if disconnect is not called, WIFI connection fails after first startup
				WiFi.disconnect();
				WiFi.begin(ssid.c_str(), password.c_str());
				while ((waitForWifiConnection)
						&& (WiFi.status() != WL_CONNECTED)) {
					delay(500);
					if (millis() - now >= 5000) {
						break;
					}
				}
				//WiFi.waitForConnectResult();
				lastWifiConnect = now;
			}
			//MQTT connection
			if ((WiFi.status() == WL_CONNECTED) && (!mqttClient->connected())
					&& ((lastMqttConnect == 0)
							|| (now - lastMqttConnect > 300000))) {
				if (mqttReconnect()) {
					lastMqttConnect = now;
				}
			}
			if (mqttClient->connected()) {
				mqttClient->loop();
			}
		}
		return true;
	} else {
		dnsApServer->processNextRequest();
		apServer->handleClient();
		return false;
	}
}

void KaNetwork::setOnNotify(THandlerFunction onNotify) {
	this->onNotify = onNotify;
}

void KaNetwork::setOnConfigurationFinished(
		THandlerFunction onConfigurationFinished) {
	this->onConfigurationFinished = onConfigurationFinished;
}

void KaNetwork::notify() {
	if (onNotify) {
		onNotify();
	}
}

int KaNetwork::getNetworkState() {
	return networkState;
}

bool KaNetwork::publishMqtt(String topic, JsonObject& json) {
	char payloadBuffer[MQTT_MAX_PACKET_SIZE];
	json.printTo(payloadBuffer, sizeof(payloadBuffer));
	topic = mqttTopic + (topic != "" ? "/" + topic : "");
	int mSize = 5 + 2 + topic.length() + strlen(payloadBuffer);
	if (mqttClient->publish(topic.c_str(), payloadBuffer)) {
		return true;
	} else {
		log("Sending MQTT message failed, rc=" + String(mqttClient->state()));
		return false;
	}
}

void KaNetwork::onCallbackMqtt(TCallBackMqttHandler cbmh) {
	_mqtt_callback = cbmh;
}

void KaNetwork::mqttCallback(char* ptopic, byte* payload, unsigned int length) {
	//create character buffer with ending null terminator (string)
	char message_buff[MQTT_MAX_PACKET_SIZE];
	for (int i = 0; i < length; i++) {
		message_buff[i] = payload[i];
	}
	message_buff[length] = '\0';
	//forward to serial port
	log(
			"Received MQTT callback: " + String(ptopic) + "/{"
					+ String(message_buff) + "}");
	String topic = String(ptopic).substring(mqttTopic.length() + 1);
	if (_mqtt_callback) {
		_mqtt_callback(topic, String(message_buff));
	}
	//relayControl->processCommand(topic, String(message_buff));
}

bool KaNetwork::mqttReconnect() {
	log("Attempting MQTT connection... ");
	// Attempt to connect
	if (mqttClient->connect(getClientName().c_str(),
			mqttUser.c_str(), mqttPassword.c_str())) {
		log("Connected to MQTT server.");
		mqttClient->subscribe(String(mqttTopic + "/#").c_str());
		return true;
	} else {
		log(
				"Connection to MQTT server failed, rc="
						+ String(mqttClient->state()));
		return false;
	}
}

void KaNetwork::log(String debugMessage) {
	if (debug) {
		Serial.println(debugMessage);
	}
}

void KaNetwork::startConfiguration() {
	if (!isInConfigurationMode()) {
		//Close existing connections
		if (mqttClient->connected()) {
			mqttClient->disconnect();
		}
		if (WiFi.status() == WL_CONNECTED) {
			WiFi.disconnect(true);
		}
		//Create AP
		String apSsid = getClientName();
		log("Start AccessPoint for configuration. SSID '" + apSsid + "'; password '" + CONFIG_PASSWORD + "'");
		dnsApServer = new DNSServer();
		apServer = new ESP8266WebServer(80);
		WiFi.softAP(apSsid.c_str(), CONFIG_PASSWORD.c_str());
		dnsApServer->setErrorReplyCode(DNSReplyCode::NoError);
		dnsApServer->start(53, "*", WiFi.softAPIP());
		apServer->on("/", std::bind(&KaNetwork::handleHttpRootRequest, this));
		apServer->on("/wifi", std::bind(&KaNetwork::handleHttpWifi, this));
		apServer->on("/saveConfiguration",
				std::bind(&KaNetwork::handleHttpSaveConfiguration, this));
		apServer->on("/info", std::bind(&KaNetwork::handleHttpInfo, this));
		apServer->on("/reset", std::bind(&KaNetwork::handleHttpReset, this));
		apServer->on("/firmware", HTTP_GET,
				std::bind(&KaNetwork::handleHttpFirmwareUpdate, this));
		apServer->on("/firmware", HTTP_POST,
				[&]() {
					if (Update.hasError()) {
						apServer->send(200, "text/html", String(F("Update error: ")) + firmwareUpdateError);
					} else {
						apServer->client().setNoDelay(true);
						String page = HTTP_HEAD + HTTP_SCRIPT + HTTP_STYLE + HTTP_HEAD_END + HTTP_SAVED + HTTP_END;
						page.replace("{v}", "Update successful.");
						apServer->send(200, "text/html", page);
					}
					this->restart();
				}, [&]() {
					// handler for the file upload, get's the sketch bytes, and writes
					// them through the Update object
				HTTPUpload& upload = apServer->upload();

				if(upload.status == UPLOAD_FILE_START) {
					firmwareUpdateError = "";
					if (debug) {
						Serial.setDebugOutput(true);
					}
					WiFiUDP::stopAll();
					log("Update: " + upload.filename);
					uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
					if(!Update.begin(maxSketchSpace)) { //start with max available size
						setFirmwareUpdateError();
					}
				} else if(upload.status == UPLOAD_FILE_WRITE && !firmwareUpdateError.length()) {
					if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
						setFirmwareUpdateError();
					}
				} else if(upload.status == UPLOAD_FILE_END && !firmwareUpdateError.length()) {
					if(Update.end(true)) { //true to set the size to the current progress
						log("Update Success: " + String(upload.totalSize) + ". Rebooting...");
					} else {
						setFirmwareUpdateError();
					}
					if (debug) {
						Serial.setDebugOutput(false);
					}
				} else if(upload.status == UPLOAD_FILE_ABORTED) {
					Update.end();
					log("Update was aborted.");
				}
				delay(0);
			});
		//Start ap http server
		apServer->begin();
	}
}

bool KaNetwork::isInConfigurationMode() {
	return (apServer != nullptr);
}

void KaNetwork::handleHttpRootRequest() {
	if (isInConfigurationMode()) {
		String page = HTTP_HEAD;
		page.replace("{v}", applicationName);
		page += HTTP_SCRIPT;
		page += HTTP_STYLE;
		//page += _customHeadElement;
		page += HTTP_HEAD_END;
		page += getHttpCaption();
		page += HTTP_BUTTON_CONFIGURE;
		page += HTTP_BUTTON_FIRMWARE;
		page += HTTP_BUTTON_INFO;
		page += HTTP_BUTTON_RESET;
		page += HTTP_END;
		apServer->sendHeader("Content-Length", String(page.length()));
		apServer->send(200, "text/html", page);
	}
}

void KaNetwork::handleHttpWifi() {
	if (isInConfigurationMode()) {
		String page = HTTP_HEAD;
		page.replace("{v}", "Device Configuration");
		page += HTTP_SCRIPT;
		page += HTTP_STYLE;
		page += HTTP_HEAD_END;
		page += HTTP_FORM_WIFI_PARAM;
		page += HTTP_FORM_MQTT_PARAM;
		page += HTTP_FORM_END;
		page += HTTP_END;

		page.replace("{s}", this->ssid);
		page.replace("{p}", this->password);
		page.replace("{ms}", this->mqttServer);
		page.replace("{mt}", this->mqttTopic);
		page.replace("{mu}", this->mqttUser);
		page.replace("{mp}", this->mqttPassword);

		apServer->sendHeader("Content-Length", String(page.length()));
		apServer->send(200, "text/html", page);
		log("Sent Wifi configuration page.");
	}
}

void KaNetwork::handleHttpSaveConfiguration() {
	if (isInConfigurationMode()) {
		this->ssid = apServer->arg("s");
		this->password = apServer->arg("p");
		this->mqttServer = apServer->arg("ms");
		this->mqttTopic = apServer->arg("mt");
		this->mqttUser = apServer->arg("mu");
		this->mqttPassword = apServer->arg("mp");
		this->saveSettings();

		String page = HTTP_HEAD;
		page += HTTP_SCRIPT;
		page += HTTP_STYLE;
		page += HTTP_HEAD_END;
		page += HTTP_SAVED;
		page += HTTP_END;
		page.replace("{v}", "Settings saved.");

		apServer->sendHeader("Content-Length", String(page.length()));
		apServer->send(200, "text/html", page);

		this->restart();
	}
}

void KaNetwork::handleHttpInfo() {
	if (isInConfigurationMode()) {
		String page = HTTP_HEAD;
		page.replace("{v}", "Info");
		page += HTTP_SCRIPT;
		page += HTTP_STYLE;
		page += HTTP_HEAD_END;
		page += getHttpCaption();
		page += "<table>";
		page += "<tr><th>Chip ID:</th><td>";
		page += ESP.getChipId();
		page += "</td></tr>";
		page += "<tr><th>Flash Chip ID:</th><td>";
		page += ESP.getFlashChipId();
		page += "</td></tr>";
		page += "<tr><th>IDE Flash Size:</th><td>";
		page += ESP.getFlashChipSize();
		page += "</td></tr>";
		page += "<tr><th>Real Flash Size:</th><td>";
		page += ESP.getFlashChipRealSize();
		page += "</td></tr>";
		page += "<tr><th>Soft AP IP:</th><td>";
		page += WiFi.softAPIP().toString();
		page += "</td></tr>";
		page += "<tr><th>Soft AP MAC:</th><td>";
		page += WiFi.softAPmacAddress();
		page += "</td></tr>";
		page += "<tr><th>Station MAC:</th><td>";
		page += WiFi.macAddress();
		page += "</td></tr>";
		page += "<tr><th>Current sketch size:</th><td>";
		page += ESP.getSketchSize();
		page += "</td></tr>";
		page += "<tr><th>Available sketch size:</th><td>";
		page += ESP.getFreeSketchSpace();
		page += "</td></tr>";
		page += "<tr><th>EEPROM size:</th><td>";
		page += EEPROM.length();
		page += "</td></tr>";
		page += "</table>";
		page += HTTP_END;
		apServer->sendHeader("Content-Length", String(page.length()));
		apServer->send(200, "text/html", page);
	}
}

/** Handle the reset page */
void KaNetwork::handleHttpReset() {
	if (isInConfigurationMode()) {
		String page = HTTP_HEAD;
		page.replace("{v}", "Info");
		page += HTTP_SCRIPT;
		page += HTTP_STYLE;
		page += HTTP_HEAD_END;
		page += "Module will reset in a few seconds.";
		page += HTTP_END;
		apServer->sendHeader("Content-Length", String(page.length()));
		apServer->send(200, "text/html", page);

		this->restart();
	}
}

String KaNetwork::getHttpCaption() {
	return "<h2>" + applicationName + "</h2><h3>Revision " + firmwareVersion + "</h3>";
}

String KaNetwork::getClientName() {
	String result = (applicationName.equals("") ? "ESP_" : applicationName);
	result.replace(" ", "_");
	//result += "_";
	String chipId = String(ESP.getChipId());
	int resLength = result.length() + chipId.length() + 1 - 32;
	if (resLength > 0) {
		result.substring(0, 32 - resLength);
	}
	return result + "_" + chipId;
}

void KaNetwork::handleHttpFirmwareUpdate() {
	if (isInConfigurationMode()) {
		String page = HTTP_HEAD;
		page.replace("{v}", "Firmware update");
		page += HTTP_SCRIPT;
		page += HTTP_STYLE;
		page += HTTP_HEAD_END;
		page += getHttpCaption();
		page += HTTP_FORM_FIRMWARE;
		page += HTTP_END;

		apServer->sendHeader("Content-Length", String(page.length()));
		apServer->send(200, "text/html", page);
		log("Sent Firmware update page.");
	}
}

String KaNetwork::getFirmwareUpdateErrorMessage() {
	switch (Update.getError()) {
	case UPDATE_ERROR_OK:
		return "No Error";
	case UPDATE_ERROR_WRITE:
		return "Flash Write Failed";
	case UPDATE_ERROR_ERASE:
		return "Flash Erase Failed";
	case UPDATE_ERROR_READ:
		return "Flash Read Failed";
	case UPDATE_ERROR_SPACE:
		return "Not Enough Space";
	case UPDATE_ERROR_SIZE:
		return "Bad Size Given";
	case UPDATE_ERROR_STREAM:
		return "Stream Read Timeout";
	case UPDATE_ERROR_MD5:
		return "MD5 Failed: ";
	case UPDATE_ERROR_SIGN:
		return "Signature verification failed";
	case UPDATE_ERROR_FLASH_CONFIG:
		return "Flash config wrong.";
	case UPDATE_ERROR_NEW_FLASH_CONFIG:
		return "New Flash config wrong.";
	case UPDATE_ERROR_MAGIC_BYTE:
		return "Magic byte is wrong, not 0xE9";
	case UPDATE_ERROR_BOOTSTRAP:
		return "Invalid bootstrapping state, reset ESP8266 before updating";
	default:
		return "UNKNOWN";
	}
}

void KaNetwork::setFirmwareUpdateError() {
	firmwareUpdateError = getFirmwareUpdateErrorMessage();
	log(firmwareUpdateError);
}

void KaNetwork::stopConfiguration() {
	if (isInConfigurationMode()) {
		delay(100);
		apServer->client().stop();
		apServer = nullptr;
		if (onConfigurationFinished) {
			onConfigurationFinished();
		}
	}
}

void KaNetwork::restart() {
	stopConfiguration();
	delay(1000);
	ESP.restart();
	delay(2000);
}
