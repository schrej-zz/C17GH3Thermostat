#ifndef WEBSERVER_H
#define WEBSERVER_H
//#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>


class Webserver
{
public:
	Webserver() {}
	void init(class C17GH3State* state);
	void stop();
	void start();

	void handleRoot();
	void handleLogin();
	void handleNotFound();
	void handleConsole();
	void process();
	void setPassword(const char *pass)
	{
		password = pass;
	}

private:
	class ESP8266WebServer* server = nullptr;
	class C17GH3State* state = nullptr;
	class ESP8266HTTPUpdateServer* httpUpdater = nullptr;
	String password;
};
#endif