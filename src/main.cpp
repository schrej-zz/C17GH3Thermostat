#define MQTT_MAX_PACKET_SIZE 1024  //Setting for JSON-MQTT

#include <Arduino.h>
#include "ESPBase.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NTPClientLib.h>

#include "C17GH3.h"

ESPBASE Esp;
C17GH3State state;
Log logger;

static void mqttCallback(char* top, byte* pay, unsigned int length);
static void mqttPublish();

WiFiClient espClient;
PubSubClient mqttClient(espClient);
NTPSyncEvent_t ntpEvent; 				// Last triggered event
int reconnect = 0;

void setup()
{
	Esp.initialize(&state);

 	Serial.begin(9600);

	String name = config.DeviceName;

	state.setWifiConfigCallback([name]() {
		logger.addLine("Configuration portal opened");
    	WiFi.mode(WIFI_AP);
    	WiFi.softAP(name);
		logger.addLine("Configuration portal closed");
    });

	MDNS.begin(config.DeviceName);
	MDNS.addService("http", "tcp", 80);

	NTP.onNTPSyncEvent ([](NTPSyncEvent_t event) {
 		if (event == timeSyncd) {
			 state.setTime();
        }
    });

    NTP.setInterval (63);
    NTP.setNTPTimeout (1500);
    NTP.begin (config.ntpServerName.c_str(), config.timeZone / 10, config.isDayLightSaving,  0);

	//Starting MQTT Client
	mqttClient.setServer(config.mqtt_server.c_str(), config.mqtt_port.toInt());
	mqttClient.setCallback(mqttCallback);
}

void mqttCallback(char* top, byte* pay, unsigned int length) 
{
	char buffer[1024] = {0};

	if (length >= 1024 || length == 0)
		return; // message too big or null
	memcpy(buffer, pay, length);
	String payload(buffer);
	String topic(top);

	if(payload.length() == 0)
	  return;
    
	if(topic.endsWith("/on/set"))
		state.setPower(payload.toInt());
	else if(topic.endsWith("/lock/set"))
		state.setLock(payload.toInt());
	else if(topic.endsWith("/manual/set"))
		state.setMode(payload.toInt());
	else if(topic.endsWith("/temperature_setpoint/set"))
		state.setSetPointTemp(payload.toFloat());
	else if(topic.endsWith("/backlight_always_on/set"))
		state.setBacklightMode(payload.toInt());
	else if(topic.endsWith("/on_after_powerloss/set"))
		state.setPowerMode(payload.toInt());	
	else if(topic.endsWith("/antifreeze/set"))
		state.setAntifreezeMode(payload.toInt());	
	else if(topic.endsWith("/sensor_mode/set"))
		state.setSensorMode((C17GH3MessageSettings2::SensorMode)payload.toInt());	
	else if(topic.endsWith("/temperature_correction/set"))
		state.setTempCorrect(payload.toFloat());
	else if(topic.endsWith("/hysteresis_internal/set"))
		state.setInternalHysteresis(payload.toFloat());
	else if(topic.endsWith("/hysteresis_external/set"))
		state.setExternalHysteresis(payload.toFloat());
	else if(topic.endsWith("/temperature_limit_external/set"))
		state.setTemperatureLimit(payload.toFloat());
	else if(topic.endsWith("/schedule1/set"))
		state.setSchedule(1, payload);
	else if(topic.endsWith("/schedule2/set"))
		state.setSchedule(2, payload);
	else if(topic.endsWith("/schedule3/set"))
		state.setSchedule(3, payload);
	else if(topic.endsWith("/schedule4/set"))
		state.setSchedule(4, payload);
	else if(topic.endsWith("/schedule5/set"))
		state.setSchedule(5, payload);
	else if(topic.endsWith("/schedule6/set"))
		state.setSchedule(6, payload);
	else if(topic.endsWith("/schedule7/set"))
		state.setSchedule(7, payload);
}

uint32_t mqttNextConnectAttempt = 0;

void mqttReconnect() 
{
	uint32_t now = millis();
	if (now > mqttNextConnectAttempt)
	{
		// Loop until we're reconnected
		if (!mqttClient.connected())
		{
			logger.addLine("Attempting MQTT connection...");
			String prefix = config.mqtt_prefix + "/" + config.DeviceName;
			// Attempt to connect
			String lastWill = prefix + "/online";
			if (mqttClient.connect(config.DeviceName.c_str(), config.mqtt_username.c_str(), config.mqtt_password.c_str(), lastWill.c_str(), 1, true, "offline")) 
			{
				logger.addLine("MQTT connected");
				mqttClient.publish(lastWill.c_str(), "online", true);
				String topic = prefix + "/+/set";
				mqttClient.subscribe(topic.c_str());
			} 
			else 
			{
				logger.addLine(String("MQTT Connection failed: " + mqttClient.state()));
				mqttNextConnectAttempt = now + 2000;
			}
		}
	}
}

void mqttPublish()
{
	if(state.isChanged)
	{
		String prefix = config.mqtt_prefix + "/" + config.DeviceName;

		mqttClient.publish(String(prefix + "/online").c_str(), "online", true);
		mqttClient.publish(String(prefix + "/wifi").c_str(), String(state.getWiFiState()).c_str());
		mqttClient.publish(String(prefix + "/temperature_setpoint").c_str(), String(state.getSetPointTemp()).c_str(), true);
		mqttClient.publish(String(prefix + "/lock").c_str(), String(state.getLock()).c_str(), true);
		mqttClient.publish(String(prefix + "/manual").c_str(), String(state.getMode()).c_str(), true);
		mqttClient.publish(String(prefix + "/on").c_str(), String(state.getPower()).c_str(), true);
		mqttClient.publish(String(prefix + "/temperatur_internal").c_str(), String(state.getInternalTemperature()).c_str());
		mqttClient.publish(String(prefix + "/temperatur_external").c_str(), String(state.getExternalTemperature()).c_str());
		mqttClient.publish(String(prefix + "/backlight_always_on").c_str(), String(state.getBacklightMode()).c_str(), true);
		mqttClient.publish(String(prefix + "/on_after_powerloss").c_str(), String(state.getPowerMode()).c_str(), true);
		mqttClient.publish(String(prefix + "/antifreeze").c_str(), String(state.getAntifreezeMode()).c_str(), true);
		mqttClient.publish(String(prefix + "/sensor_mode").c_str(), String(state.getSensorMode()).c_str(), true);
		mqttClient.publish(String(prefix + "/temperature_correction").c_str(), String(state.getTempCorrect()).c_str(), true); 
		mqttClient.publish(String(prefix + "/hysteresis_internal").c_str(), String(state.getInternalHysteresis()).c_str(), true);
		mqttClient.publish(String(prefix + "/hysteresis_external").c_str(), String(state.getExternalHysteresis()).c_str(), true);
		mqttClient.publish(String(prefix + "/temperature_limit_external").c_str(), String(state.getTemperatureLimit()).c_str(), true);
		
		mqttClient.publish(String(prefix + "/schedule1").c_str(), state.getSchedule(1).c_str(), true);
		mqttClient.publish(String(prefix + "/schedule2").c_str(), state.getSchedule(2).c_str(), true);
		mqttClient.publish(String(prefix + "/schedule3").c_str(), state.getSchedule(3).c_str(), true);
		mqttClient.publish(String(prefix + "/schedule4").c_str(), state.getSchedule(4).c_str(), true);
		mqttClient.publish(String(prefix + "/schedule5").c_str(), state.getSchedule(5).c_str(), true);
		mqttClient.publish(String(prefix + "/schedule6").c_str(), state.getSchedule(6).c_str(), true);
		mqttClient.publish(String(prefix + "/schedule7").c_str(), state.getSchedule(7).c_str(), true);

		state.isChanged = false;
	}
}

void loop()
{
	ArduinoOTA.handle();
	server.handleClient();

	if (!mqttClient.connected()) {
		mqttReconnect();
	}
	
	NTP.getTimeDateString();
	state.processRx();
	if(WiFi.status() != WL_CONNECTED)
	{
		WiFi.reconnect();
		reconnect++;
	}
	else
	{
		reconnect = 0;
	}
	
	if(reconnect > 100)
	{
		ESP.restart();
	}

	if(state.isChanged && state.isFirstQueryDone())
	{
	   mqttPublish();
	}

	state.processTx();
	MDNS.update();
	mqttClient.loop();
}