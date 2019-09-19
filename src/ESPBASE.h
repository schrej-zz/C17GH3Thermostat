#ifndef ESPBASE_H
#define ESPBASE_H

#include <Arduino.h>
#include <string.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <EEPROM.h>

#include "C17GH3.h"
#include "Log.h"

extern Log logger;

class ESPBASE
{
public:
    bool WIFI_connected, CFG_saved;
    void initialize(C17GH3State* s);
    void httpSetup();
    void OTASetup();
private:
	  void handleConsole();
    void handleStatus();

    ESP8266HTTPUpdateServer httpUpdater; 
    class C17GH3State* state = nullptr;
};

#include "Parameters.h"
#include "WifiTools.h"

#include "C17GH3.h"
// Include the HTML, STYLE and Script "Pages"
#include "Page_Admin.h"
#include "Page_Script.js.h"
#include "Page_Style.css.h"
#include "Page_NTPsettings.h"
#include "Page_Information.h"
#include "Page_General.h"
#include "PAGE_NetworkConfiguration.h"

void ESPBASE::initialize(C17GH3State* s)
{

  CFG_saved = false;
  WIFI_connected = false;
  uint8_t timeoutClick = 50;
  
  state = s;

  String chipID;

  EEPROM.begin(512); // define an EEPROM space of 512Bytes to store data

  //**** Network Config load
  CFG_saved = ReadConfig();

  //  Connect to WiFi access point or start as Access point
  if (CFG_saved) 
  { //if config saved use saved values

      // Connect the ESP8266 to local WIFI network in Station mode
      // using SSID and password saved in parameters (config object)
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(true);
      WiFi.hostname(config.DeviceName);
      WiFi.begin(config.ssid.c_str(), config.password.c_str());
      while((WiFi.status()!= WL_CONNECTED) and --timeoutClick > 0) {
        delay(500);
      }
      if(WiFi.status()!= WL_CONNECTED )
      {
          WIFI_connected = false;
      }
      else
      {
        WIFI_connected = true;
      }
  }

  if ( !WIFI_connected or !CFG_saved )
  { // if no values saved or not good use defaults
    //load config with default values
    configLoadDefaults(getChipId());
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.ssid.c_str());
  }

  //  Http Setup
  httpSetup();

  // ***********  OTA SETUP
  OTASetup();

}

void ESPBASE::httpSetup()
{
  // Start HTTP Server for configuration
  server.on ( "/", []() {
    if(config.OTApwd.length() > 0)
	  {
  	  if(!server.authenticate("admin", config.OTApwd.c_str()))
        return server.requestAuthentication();
	  }
    server.send_P ( 200, "text/html", PAGE_AdminMainPage);  // const char top of page
  }  );
  server.on ( "/favicon.ico",   []() {
    if(config.OTApwd.length() > 0)
	  {
  	  if(!server.authenticate("admin", config.OTApwd.c_str()))
        return server.requestAuthentication();
	  }
    server.send( 200, "text/html", "" );
  }  );
  // Network config
  server.on ( "/config.html", send_network_configuration_html );
  // Info Page
  server.on ( "/info.html", []() {
    if(config.OTApwd.length() > 0)
	  {
  	  if(!server.authenticate("admin", config.OTApwd.c_str()))
        return server.requestAuthentication();
	  }
    server.send_P ( 200, "text/html", PAGE_Information );
  }  );
  server.on ( "/ntp.html", send_NTP_configuration_html  );

  //server.on ( "/appl.html", send_application_configuration_html  );
  server.on ( "/general.html", send_general_html  );
  //  server.on ( "/example.html", []() { server.send_P ( 200, "text/html", PAGE_EXAMPLE );  } );
  server.on ( "/style.css", []() {
    if(config.OTApwd.length() > 0)
	  {
  	  if(!server.authenticate("admin", config.OTApwd.c_str()))
        return server.requestAuthentication();
	  }
    server.send_P ( 200, "text/plain", PAGE_Style_css );
  } );
  server.on ( "/microajax.js", []() {
    if(config.OTApwd.length() > 0)
	  {
  	  if(!server.authenticate("admin", config.OTApwd.c_str()))
        return server.requestAuthentication();
	  }
    server.send_P ( 200, "text/plain", PAGE_microajax_js );
  } );
  server.on ( "/admin/values", send_network_configuration_values_html );
  server.on ( "/admin/connectionstate", send_connection_state_values_html );
  server.on ( "/admin/infovalues", send_information_values_html );
  server.on ( "/admin/ntpvalues", send_NTP_configuration_values_html );
  server.on ( "/admin/generalvalues", send_general_configuration_values_html);
  server.on ( "/admin/devicename",     send_devicename_value_html);
	server.on("/status", HTTP_GET,std::bind(&ESPBASE::handleStatus, this));
	server.on("/console", HTTP_GET,std::bind(&ESPBASE::handleConsole, this));
	server.on("/console", HTTP_POST,std::bind(&ESPBASE::handleConsole, this));
  server.onNotFound ( []() {
    server.send ( 400, "text/html", "Page not Found" );
  }  );

  if(config.OTApwd.length() > 0)
  {
    httpUpdater.setup(&server, "/update", "admin", config.OTApwd);
  }
  else
  {
    httpUpdater.setup(&server, "/update");
  }

  server.begin();
  return;
}

void ESPBASE::handleStatus()
{
	String header;
	if(config.OTApwd.length() > 0)
	{
  	  if(!server.authenticate("admin", config.OTApwd.c_str()))
        return server.requestAuthentication();
	}

	String msg( R"=====(
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <a href="/"  class="btn btn--s"><</a>&nbsp;&nbsp;<strong>State</strong>
  <hr>
  )=====");

  msg += "<p>Day: " + String(weekday() == 1 ? 7 : weekday() - 1) + " Hour: " + String(hour()) + " Minute: " + String(minute()) + "</p>";
	msg += "<pre>";
	msg += state->toString();
	msg += "</pre>";
	
	msg += String(R"=====(
  <script>
  window.onload = function ()
  {
	  load("style.css","css", function() 
	  {
		  load("microajax.js","js", function() 
		  {
				setValues("/admin/generalvalues");
		  });
	  });
  }
  function load(e,t,n){if("js"==t){var a=document.createElement("script");a.src=e,a.type="text/javascript",a.async=!1,a.onload=function(){n()},document.getElementsByTagName("head")[0].appendChild(a)}else if("css"==t){var a=document.createElement("link");a.href=e,a.rel="stylesheet",a.type="text/css",a.async=!1,a.onload=function(){n()},document.getElementsByTagName("head")[0].appendChild(a)}}
  </script>
)=====" );
	server.send(200, "text/html", msg);
}


void ESPBASE::handleConsole()
{
	if(config.OTApwd.length() > 0)
	{
  	  if(!server.authenticate("admin", config.OTApwd.c_str()))
        return server.requestAuthentication();
	}
	
	if (server.hasArg("cmd"))
    {
		String cmd = server.arg("cmd");
		cmd.trim();
		cmd.replace(" ","");
		logger.addLine("Got a post cmd: " + cmd);
		
		if (cmd.length() == 35)
		{
			if (cmd.startsWith("RX:"))
			{
				cmd = cmd.substring(3);
				while (cmd.length() > 0)
				{
					String v = cmd.substring(0,2);
					cmd = cmd.substring(2);
					state->processRx(strtol(v.c_str(), nullptr, 16));
				}
			}
			else if (cmd.startsWith("TX:"))
			{
				cmd = cmd.substring(3);
				C17GH3MessageBuffer buffer;
				
				while (cmd.length() > 0)
				{
					String v = cmd.substring(0,2);
					cmd = cmd.substring(2);
					bool hasMsg = buffer.addbyte(strtol(v.c_str(), nullptr, 16));
					if (hasMsg)
					{
						C17GH3MessageBase msg(buffer.getBytes());
						msg.pack();
						state->sendMessage(msg);
					}
				}

			}
		}
    }
	String msg( R"=====(
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <a href="/"  class="btn btn--s"><</a>&nbsp;&nbsp;<strong>Console</strong>
  <hr>
  )=====");
	msg += "<script>";
	msg += "function sb(){ var c = document.getElementById('console');c.scrollTop = c.scrollHeight;}";
	msg += "</script></head>";
	msg += "<body onload='sb();'>";
	msg += "<form method='POST'>";
	msg += "<textarea id='console' style='width:100%;height:calc(100% - 50px);'>";
	msg += logger.getLines();
	msg += "</textarea>";
	msg += "<br/>";
	msg += "<input type='text' name='cmd' style='width:calc(100% - 100px);'></input><input type=submit value='send' style='width:100px;'></input>";
	msg += "</form>";
	msg += String(R"=====(
  <script>
  window.onload = function ()
  {
	  load("style.css","css", function() 
	  {
		  load("microajax.js","js", function() 
		  {
				setValues("/admin/generalvalues");
		  });
	  });
  }
  function load(e,t,n){if("js"==t){var a=document.createElement("script");a.src=e,a.type="text/javascript",a.async=!1,a.onload=function(){n()},document.getElementsByTagName("head")[0].appendChild(a)}else if("css"==t){var a=document.createElement("link");a.href=e,a.rel="stylesheet",a.type="text/css",a.async=!1,a.onload=function(){n()},document.getElementsByTagName("head")[0].appendChild(a)}}
  </script>
)=====" );
	server.send(200, "text/html", msg);
}


void ESPBASE::OTASetup()
{
      //ArduinoOTA.setHostname(host);
      ArduinoOTA.onStart([]() { // what to do before OTA download insert code here
        });
      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      });
      ArduinoOTA.onEnd([]() { // do a fancy thing with our board led at end
          ESP.restart();
        });

      ArduinoOTA.onError([](ota_error_t error) {
          ESP.restart();
        });

       /* setup the OTA server */
      if(config.OTApwd.length() > 0)
      {
        ArduinoOTA.setPassword(config.OTApwd.c_str());   
      }
      ArduinoOTA.begin();
}
#endif