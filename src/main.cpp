#include <Arduino.h>

#include <ESP8266WiFi.h> 
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <FS.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>

// time includes
#include <time.h>
#include <sys/time.h>
#include <coredecls.h>                  // settimeofday_cb()

#include <set>

#include "C17GH3.h"
#include "Webserver.h"
#include "Log.h"

WiFiManager wifiManager;
C17GH3State state;
Webserver webserver;
Log logger;
bool relayOn = false;
StaticJsonDocument<1024> jsonDoc;

#define TIMEZONE 	"CET-1CEST,M3.5.0,M10.5.0/3" // FROM https://github.com/nayarsystems/posix_tz_db/blob/master/zones.json

static void setupOTA();
static void initTime();
static void ICACHE_RAM_ATTR handleRelayMonitorInterrupt();
static void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttPublish();

#define PIN_RELAY_MONITOR D1
#define PIN_RELAY2        D2

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String devName;
String mqttServer = "192.168.71.12";
uint16_t   mqttPort = 1883;
String mqttUser = "herter";
String mqttPassword = "****";
String mqttPrefix = "herter/thermostat";

void setup()
{
	devName = String("Thermostat-") + String(ESP.getChipId(),HEX);
	mqttPrefix += String("/") + String(ESP.getChipId(),HEX);
	ArduinoOTA.setHostname(devName.c_str());
	WiFi.hostname(devName);
	WiFi.enableAP(false);
	WiFi.enableSTA(true);
	WiFi.begin();
	wifiManager.setConfigPortalTimeout(120);
	wifiManager.setDebugOutput(false);

	Serial.begin(9600);

	String name = devName;
	state.setWifiConfigCallback([name]() {
		logger.addLine("Configuration portal opened");
		webserver.stop();
        wifiManager.startConfigPortal(devName.c_str());
		webserver.start();
		WiFi.enableAP(false);
		logger.addLine("Configuration portal closed");
    });

	webserver.init(&state);
	setupOTA();

	MDNS.begin(devName);
	MDNS.addService("http", "tcp", 80);

	initTime();
#ifdef PIN_RELAY_MONITOR
	pinMode(PIN_RELAY_MONITOR, INPUT);
	attachInterrupt(digitalPinToInterrupt(PIN_RELAY_MONITOR), handleRelayMonitorInterrupt, CHANGE);
	relayOn = digitalRead(PIN_RELAY_MONITOR);
	state.setIsHeating(relayOn);
#endif
#ifdef PIN_RELAY2
	pinMode(PIN_RELAY2, OUTPUT);
	digitalWrite(PIN_RELAY2, LOW); 	
#endif

	mqttClient.setServer(mqttServer.c_str(), mqttPort);
	mqttClient.setCallback(mqttCallback);
;}

timeval cbtime;			// when time set callback was called
int cbtime_set = 0;
static void timeSet()
{
	gettimeofday(&cbtime, NULL);
	cbtime_set++;
}



void mqttCallback(char* topic, byte* payload, unsigned int length) 
{
	char buffer[64] = {0};

	if (length >= 64 || length == 0)
		return; // message too big or null
	memcpy(buffer, payload, length);
	String pay(buffer);
	if(pay.length() == 0)
	  return;
	if(strcmp(&topic[mqttPrefix.length()], "/on/set") == 0)
		state.setPower(pay.toInt());
	else if(strcmp(&topic[mqttPrefix.length()], "/lock/set") == 0)
		state.setLock(pay.toInt());
	else if(strcmp(&topic[mqttPrefix.length()], "/manual/set") == 0)
		state.setMode(pay.toInt());
	else if(strcmp(&topic[mqttPrefix.length()], "/temperature_setpoint/set") == 0)
		state.setSetPointTemp(pay.toFloat());
	else if(strcmp(&topic[mqttPrefix.length()], "/backlight_always_on/set") == 0)
		state.setBacklightMode(pay.toInt());
	else if(strcmp(&topic[mqttPrefix.length()], "/on_after_powerloss/set") == 0)
		state.setPowerMode(pay.toInt());	
	else if(strcmp(&topic[mqttPrefix.length()], "/antifreeze/set") == 0)
		state.setAntifreezeMode(pay.toInt());	
	else if(strcmp(&topic[mqttPrefix.length()], "/sensor_mode/set") == 0)
		state.setSensorMode((C17GH3MessageSettings2::SensorMode)pay.toInt());	
	else if(strcmp(&topic[mqttPrefix.length()], "/temperature_correction/set") == 0)
		state.setTempCorrect(pay.toFloat());
	else if(strcmp(&topic[mqttPrefix.length()], "/hysteresis_internal/set") == 0)
		state.setInternalHysteresis(pay.toFloat());
	else if(strcmp(&topic[mqttPrefix.length()], "/hysteresis_external/set") == 0)
		state.setExternalHysteresis(pay.toFloat());
	else if(strcmp(&topic[mqttPrefix.length()], "/temperature_limit_external/set") == 0)
		state.setTemperatureLimit(pay.toFloat());
	else if(strcmp(&topic[mqttPrefix.length()], "/schedule1/set") == 0)
		state.setSchedule(1, pay);
	else if(strcmp(&topic[mqttPrefix.length()], "/schedule2/set") == 0)
		state.setSchedule(2, pay);
	else if(strcmp(&topic[mqttPrefix.length()], "/schedule3/set") == 0)
		state.setSchedule(3, pay);
	else if(strcmp(&topic[mqttPrefix.length()], "/schedule4/set") == 0)
		state.setSchedule(4, pay);
	else if(strcmp(&topic[mqttPrefix.length()], "/schedule5/set") == 0)
		state.setSchedule(5, pay);
	else if(strcmp(&topic[mqttPrefix.length()], "/schedule6/set") == 0)
		state.setSchedule(6, pay);
	else if(strcmp(&topic[mqttPrefix.length()], "/schedule7/set") == 0)
		state.setSchedule(7, pay);

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
			// Attempt to connect
			String lastWill = mqttPrefix + "/online";
			if (mqttClient.connect(devName.c_str(), mqttUser.c_str(), mqttPassword.c_str(), lastWill.c_str(), 1, true, "offline")) 
			{
				logger.addLine("MQTT connected");
				mqttClient.publish(lastWill.c_str(), "online");
				mqttClient.subscribe(String(mqttPrefix + "/+/set").c_str());
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
		mqttClient.publish(String(mqttPrefix + "/online").c_str(), "online");
		mqttClient.publish(String(mqttPrefix + "/wifi").c_str(), String(state.getWiFiState()).c_str());
		mqttClient.publish(String(mqttPrefix + "/temperature_setpoint").c_str(), String(state.getSetPointTemp()).c_str());
		mqttClient.publish(String(mqttPrefix + "/lock").c_str(), String(state.getLock()).c_str());
		mqttClient.publish(String(mqttPrefix + "/manual").c_str(), String(state.getMode()).c_str());
		mqttClient.publish(String(mqttPrefix + "/on").c_str(), String(state.getPower()).c_str());
		mqttClient.publish(String(mqttPrefix + "/temperatur_internal").c_str(), String(state.getInternalTemperature()).c_str());
		mqttClient.publish(String(mqttPrefix + "/temperatur_external").c_str(), String(state.getExternalTemperature()).c_str());
		mqttClient.publish(String(mqttPrefix + "/backlight_always_on").c_str(), String(state.getBacklightMode()).c_str());
		mqttClient.publish(String(mqttPrefix + "/on_after_powerloss").c_str(), String(state.getPowerMode()).c_str());
		mqttClient.publish(String(mqttPrefix + "/antifreeze").c_str(), String(state.getAntifreezeMode()).c_str());
		mqttClient.publish(String(mqttPrefix + "/sensor_mode").c_str(), String(state.getSensorMode()).c_str());
		mqttClient.publish(String(mqttPrefix + "/temperature_correction").c_str(), String(state.getTempCorrect()).c_str());
		mqttClient.publish(String(mqttPrefix + "/hysteresis_internal").c_str(), String(state.getInternalHysteresis()).c_str());
		mqttClient.publish(String(mqttPrefix + "/hysteresis_external").c_str(), String(state.getExternalHysteresis()).c_str());
		mqttClient.publish(String(mqttPrefix + "/temperature_limit_external").c_str(), String(state.getTemperatureLimit()).c_str());
		
		mqttClient.publish(String(mqttPrefix + "/schedule1").c_str(), state.getSchedule(1).c_str());
		mqttClient.publish(String(mqttPrefix + "/schedule2").c_str(), state.getSchedule(2).c_str());
		mqttClient.publish(String(mqttPrefix + "/schedule3").c_str(), state.getSchedule(3).c_str());
		mqttClient.publish(String(mqttPrefix + "/schedule4").c_str(), state.getSchedule(4).c_str());
		mqttClient.publish(String(mqttPrefix + "/schedule5").c_str(), state.getSchedule(5).c_str());
		mqttClient.publish(String(mqttPrefix + "/schedule6").c_str(), state.getSchedule(6).c_str());
		mqttClient.publish(String(mqttPrefix + "/schedule7").c_str(), state.getSchedule(7).c_str());

		state.isChanged = false;
	}
}

void loop()
{
	if (!mqttClient.connected()) {
		mqttReconnect();
	}
	state.setIsHeating(relayOn);
	state.processRx();
	if(state.isChanged)
	{
	   mqttPublish();
	}
	webserver.process();
	ArduinoOTA.handle();
	state.processTx(cbtime_set > 1);
	MDNS.update();
	mqttClient.loop();
}

static void setupOTA()
{
	ArduinoOTA.onStart([]() 
	{
	});
	ArduinoOTA.onEnd([]()
	{
	});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
	{
	});
	
	ArduinoOTA.onError([](ota_error_t error)
	{

	});
	ArduinoOTA.begin();
}

static void initTime()
{
	settimeofday_cb(timeSet);

	time_t rtc_time_t = 1541267183; // fake RTC time for now

	timezone tz = { 0, 0};
	timeval tv = { rtc_time_t, 0};

	settimeofday(&tv, &tz);
	setenv("TZ", TIMEZONE, 1);

	tzset(); // save the TZ variable
	configTime(0, 0, "de.pool.ntp.org");
}

static void ICACHE_RAM_ATTR handleRelayMonitorInterrupt()
{
	relayOn = digitalRead(PIN_RELAY_MONITOR);
}
