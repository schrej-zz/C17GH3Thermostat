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

static void setupOTA();
static void initTime();
static void ICACHE_RAM_ATTR handleRelayMonitorInterrupt();
static void mqttCallback(char* topic, byte* payload, unsigned int length);
static void mqttPublish();

#define PIN_RELAY_MONITOR D1
#define PIN_RELAY2        D2

WiFiClient espClient;
PubSubClient mqttClient(espClient);


//define your default values here, if there are different values in config.json, they are overwritten.
char devName[100] = "";
char passWord[100] = "";
char mqttServer[256] = "192.168.71.12";
char mqttPort[6] = "1883";
char mqttUser[50] = "";
char mqttPassword[50] = "";
char mqttPrefix[256] = "";
char timeZone[100] = "CET-1CEST,M3.5.0,M10.5.0/3";  // FROM https://github.com/nayarsystems/posix_tz_db/blob/master/zones.json

WiFiManagerParameter* custom_devName;
WiFiManagerParameter* custom_passWord;
WiFiManagerParameter* custom_mqttServer;
WiFiManagerParameter* custom_mqttPort;
WiFiManagerParameter* custom_mqttUser;
WiFiManagerParameter* custom_mqttPassword;
WiFiManagerParameter* custom_mqttPrefix;
WiFiManagerParameter* custom_timeZone;

void setup()
{
	SPIFFS.begin();  

	//Read Config if available
	File configFile = SPIFFS.open("/config.json", "r");
	StaticJsonDocument<512> doc;
	DeserializationError error = deserializeJson(jsonDoc, configFile);	
	if(!error)
	{
		logger.addLine("Configuration read.");
		strcpy(devName, jsonDoc["devName"]);
		strcpy(passWord, jsonDoc["passWord"]);
    	strcpy(mqttServer, jsonDoc["mqttServer"]);
    	strcpy(mqttPort, jsonDoc["mqttPort"]);
    	strcpy(mqttUser, jsonDoc["mqttUser"]);
    	strcpy(mqttPassword, jsonDoc["mqttPassword"]);
    	strcpy(mqttPrefix, jsonDoc["mqttPrefix"]);
    	strcpy(timeZone, jsonDoc["timeZone"]);
	}
	else
	{
		logger.addLine("No configuration found. Using defaults.");
	}
    configFile.close();

	//If no Devicename use Chip-ID
	if(devName[0] == 0)
	{
	  strcpy(devName, "Thermostat-");
	  strcat(devName, String(ESP.getChipId(),HEX).c_str());
	}

	//If Prefix is null, use Devicename
    if(mqttPrefix[0] == 0)
	{
	   strcpy(mqttPrefix, devName);
	}

	//Init Config Parameter
	custom_devName = new WiFiManagerParameter("devName", "Device Name", devName, sizeof(devName));
	custom_passWord = new WiFiManagerParameter("passWord", "Password", passWord, sizeof(passWord));
	custom_mqttServer = new WiFiManagerParameter("mqttServer", "MQTT Server", mqttServer, sizeof(mqttServer));
	custom_mqttPort = new WiFiManagerParameter("mqttPort", "MQTT Port", mqttPort, sizeof(mqttPort));
	custom_mqttUser = new WiFiManagerParameter("mqttUser", "MQTT User", mqttUser, sizeof(mqttUser));
	custom_mqttPassword = new WiFiManagerParameter("mqttPassword", "MQTT Password", mqttPassword, sizeof(mqttPassword));
	custom_mqttPrefix = new WiFiManagerParameter("mqttPrefix", "MQTT Prefix", mqttPrefix, sizeof(mqttPrefix));
	custom_timeZone = new WiFiManagerParameter("timeZone", "TimeZone", timeZone, sizeof(timeZone));
	
	WiFiManagerParameter custom_header("<h2>Thermostat Settings:</h2>");
	WiFiManagerParameter custom_name("<h3>Thermostat Name/Password:</h3>");
	WiFiManagerParameter custom_mqtt("<h3>MQTT Server:</h3>");
	WiFiManagerParameter custom_tz("<h3>Timezone:</h3>");

	//Adding Config to WifiManager
	wifiManager.addParameter(&custom_header);
	wifiManager.addParameter(&custom_name);
	wifiManager.addParameter(custom_devName);
	wifiManager.addParameter(custom_passWord);
	wifiManager.addParameter(&custom_mqtt);
  	wifiManager.addParameter(custom_mqttServer);
  	wifiManager.addParameter(custom_mqttPort);
  	wifiManager.addParameter(custom_mqttUser);
  	wifiManager.addParameter(custom_mqttPassword);
  	wifiManager.addParameter(custom_mqttPrefix);
	wifiManager.addParameter(&custom_tz);
  	wifiManager.addParameter(custom_timeZone);

	ArduinoOTA.setHostname(devName);
	
	//Setting Password if set
	if(passWord[0] != 0)
	{
	  ArduinoOTA.setPassword(passWord);
	}
	
	WiFi.hostname(devName);
	WiFi.enableAP(false);
	WiFi.enableSTA(true);
	WiFi.begin();
	wifiManager.setConfigPortalTimeout(120);
	wifiManager.setDebugOutput(false);
	Serial.begin(9600);

	//Test the Config-Portal
	//wifiManager.startConfigPortal(devName);

	String name = devName;

	state.setWifiConfigCallback([name]() {
		logger.addLine("Configuration portal opened");
		webserver.stop();
        wifiManager.startConfigPortal(devName);
		webserver.start();
		WiFi.enableAP(false);
		logger.addLine("Configuration portal closed");

		//Get Config Parameter
		strcpy(devName, custom_devName->getValue());
		strcpy(passWord, custom_passWord->getValue());
		strcpy(mqttServer, custom_mqttServer->getValue());
		strcpy(mqttPort, custom_mqttPort->getValue());
		strcpy(mqttUser, custom_mqttUser->getValue());
		strcpy(mqttPassword, custom_mqttPassword->getValue());
		strcpy(mqttPrefix, custom_mqttPrefix->getValue());
		strcpy(timeZone, custom_timeZone->getValue());

		//First Remove File
		SPIFFS.remove("/config.json");
		//Then create Config-File
  		File configFile = SPIFFS.open("/config.json", "w");
		if(configFile)
		{
			//Building JSON
	  		StaticJsonDocument<512> doc;	
	  		doc["devName"] = devName;
	  		doc["passWord"] = passWord;
      		doc["mqttServer"] = mqttServer;
      		doc["mqttPort"] = mqttPort;
      		doc["mqttUser"] = mqttUser;
      		doc["mqttPassword"]= mqttPassword;
      		doc["mqttPrefix"] = mqttPrefix;
      		doc["timeZone"] = timeZone;  

	  		if (serializeJson(doc, configFile) == 0) 
	  		{
    			logger.addLine("Configuration not saved!");
	  		}
	  		configFile.close();
			logger.addLine("Configuration saved.");
  		}
		else
		{
			logger.addLine("Could not open Configuration!");
		}
    });

	webserver.init(&state);
	webserver.setPassword(passWord);
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
	//Starting MQTT Client
	mqttClient.setServer(mqttServer, atoi(mqttPort));
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
	if(strcmp(&topic[strlen(mqttPrefix)], "/on/set") == 0)
		state.setPower(pay.toInt());
	else if(strcmp(&topic[strlen(mqttPrefix)], "/lock/set") == 0)
		state.setLock(pay.toInt());
	else if(strcmp(&topic[strlen(mqttPrefix)], "/manual/set") == 0)
		state.setMode(pay.toInt());
	else if(strcmp(&topic[strlen(mqttPrefix)], "/temperature_setpoint/set") == 0)
		state.setSetPointTemp(pay.toFloat());
	else if(strcmp(&topic[strlen(mqttPrefix)], "/backlight_always_on/set") == 0)
		state.setBacklightMode(pay.toInt());
	else if(strcmp(&topic[strlen(mqttPrefix)], "/on_after_powerloss/set") == 0)
		state.setPowerMode(pay.toInt());	
	else if(strcmp(&topic[strlen(mqttPrefix)], "/antifreeze/set") == 0)
		state.setAntifreezeMode(pay.toInt());	
	else if(strcmp(&topic[strlen(mqttPrefix)], "/sensor_mode/set") == 0)
		state.setSensorMode((C17GH3MessageSettings2::SensorMode)pay.toInt());	
	else if(strcmp(&topic[strlen(mqttPrefix)], "/temperature_correction/set") == 0)
		state.setTempCorrect(pay.toFloat());
	else if(strcmp(&topic[strlen(mqttPrefix)], "/hysteresis_internal/set") == 0)
		state.setInternalHysteresis(pay.toFloat());
	else if(strcmp(&topic[strlen(mqttPrefix)], "/hysteresis_external/set") == 0)
		state.setExternalHysteresis(pay.toFloat());
	else if(strcmp(&topic[strlen(mqttPrefix)], "/temperature_limit_external/set") == 0)
		state.setTemperatureLimit(pay.toFloat());
	else if(strcmp(&topic[strlen(mqttPrefix)], "/schedule1/set") == 0)
		state.setSchedule(1, pay);
	else if(strcmp(&topic[strlen(mqttPrefix)], "/schedule2/set") == 0)
		state.setSchedule(2, pay);
	else if(strcmp(&topic[strlen(mqttPrefix)], "/schedule3/set") == 0)
		state.setSchedule(3, pay);
	else if(strcmp(&topic[strlen(mqttPrefix)], "/schedule4/set") == 0)
		state.setSchedule(4, pay);
	else if(strcmp(&topic[strlen(mqttPrefix)], "/schedule5/set") == 0)
		state.setSchedule(5, pay);
	else if(strcmp(&topic[strlen(mqttPrefix)], "/schedule6/set") == 0)
		state.setSchedule(6, pay);
	else if(strcmp(&topic[strlen(mqttPrefix)], "/schedule7/set") == 0)
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
			String lastWill = String(mqttPrefix) + "/online";
			if (mqttClient.connect(devName, mqttUser, mqttPassword, lastWill.c_str(), 1, true, "offline")) 
			{
				logger.addLine("MQTT connected");
				mqttClient.publish(lastWill.c_str(), "online");
				String topic = String(mqttPrefix) + "/+/set";
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
		String prefix(mqttPrefix);

		mqttClient.publish(String(prefix + "/online").c_str(), "online");
		mqttClient.publish(String(prefix + "/wifi").c_str(), String(state.getWiFiState()).c_str());
		mqttClient.publish(String(prefix + "/temperature_setpoint").c_str(), String(state.getSetPointTemp()).c_str());
		mqttClient.publish(String(prefix + "/lock").c_str(), String(state.getLock()).c_str());
		mqttClient.publish(String(prefix + "/manual").c_str(), String(state.getMode()).c_str());
		mqttClient.publish(String(prefix + "/on").c_str(), String(state.getPower()).c_str());
		mqttClient.publish(String(prefix + "/temperatur_internal").c_str(), String(state.getInternalTemperature()).c_str());
		mqttClient.publish(String(prefix + "/temperatur_external").c_str(), String(state.getExternalTemperature()).c_str());
		mqttClient.publish(String(prefix + "/backlight_always_on").c_str(), String(state.getBacklightMode()).c_str());
		mqttClient.publish(String(prefix + "/on_after_powerloss").c_str(), String(state.getPowerMode()).c_str());
		mqttClient.publish(String(prefix + "/antifreeze").c_str(), String(state.getAntifreezeMode()).c_str());
		mqttClient.publish(String(prefix + "/sensor_mode").c_str(), String(state.getSensorMode()).c_str());
		mqttClient.publish(String(prefix + "/temperature_correction").c_str(), String(state.getTempCorrect()).c_str());
		mqttClient.publish(String(prefix + "/hysteresis_internal").c_str(), String(state.getInternalHysteresis()).c_str());
		mqttClient.publish(String(prefix + "/hysteresis_external").c_str(), String(state.getExternalHysteresis()).c_str());
		mqttClient.publish(String(prefix + "/temperature_limit_external").c_str(), String(state.getTemperatureLimit()).c_str());
		
		mqttClient.publish(String(prefix + "/schedule1").c_str(), state.getSchedule(1).c_str());
		mqttClient.publish(String(prefix + "/schedule2").c_str(), state.getSchedule(2).c_str());
		mqttClient.publish(String(prefix + "/schedule3").c_str(), state.getSchedule(3).c_str());
		mqttClient.publish(String(prefix + "/schedule4").c_str(), state.getSchedule(4).c_str());
		mqttClient.publish(String(prefix + "/schedule5").c_str(), state.getSchedule(5).c_str());
		mqttClient.publish(String(prefix + "/schedule6").c_str(), state.getSchedule(6).c_str());
		mqttClient.publish(String(prefix + "/schedule7").c_str(), state.getSchedule(7).c_str());

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
	setenv("TZ", timeZone, 1);

	tzset(); // save the TZ variable
	configTime(0, 0, "pool.ntp.org");
}

static void ICACHE_RAM_ATTR handleRelayMonitorInterrupt()
{
	relayOn = digitalRead(PIN_RELAY_MONITOR);
}
