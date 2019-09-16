#include <ESP8266WiFi.h> 
#include <sys/time.h>
#include <NTPClientLib.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

#include "C17GH3.h"
#include "Log.h"

extern Log logger;

void C17GH3State::processRx()
{
	if (Serial.available())
	{
		while (Serial.available())
		{
			processRx(Serial.read());
		}
	}
}

void C17GH3State::processRx(int byte)
{
	bool hasMsg = msgBuffer.addbyte(byte);
	if (hasMsg)
	{
		processRx(C17GH3MessageBase(msgBuffer.getBytes()));
	}
}

void C17GH3State::processRx(const C17GH3MessageBase& msg)
{
	logger.addBytes("RX:", msg.getBytes(), 16);

	if (!msg.isValid())
	{
		logger.addLine("ERROR: Invalid MSG");
		return;
	}
	switch(msg.type)
	{
		case 0xC1:
		{
			C17GH3MessageSettings1 s1msg;
			s1msg.setBytes(msg.getBytes());
			isChanged = true;
			if (C17GH3MessageSettings1::WIFI_STATE_CONFIG == s1msg.getWiFiState())
			{
				// wifi config request
				logger.addLine("WIFI CONFIG REQUEST");
				if (wifiConfigCallback)
				{
					wifiConfigCallback();
				}
			}
			else
			{	
				settings1.setBytes(msg.getBytes());
				logger.addLine(settings1.toString());
			}
		}
		break;
		case 0xC2:
			settings2.setBytes(msg.getBytes());
			isChanged = true;
			logger.addLine(settings2.toString());
		break;
		case 0xC3:
		case 0xC4:
		case 0xC5:
		case 0xC6:
		case 0xC7:
		case 0xC8:
		case 0xC9:
			schedule[msg.type - 0xC3].setBytes(msg.getBytes());
			logger.addLine(schedule[msg.type - 0xC3].toString());
			isChanged = true;
		break;
		default:
			logger.addLine("MSG Not handled");
		break;
	}
}

bool C17GH3State::isValidState(const C17GH3MessageBase::C17GH3MessageType &msgType) const
{
	if (msgType == settings1.type)
		return settings1.isValid();
	else if (msgType == settings2.type)
		return settings2.isValid();
	else
		for (int i = 0 ; i < 7; ++i)
			if (msgType == schedule[i].type)
				return schedule[i].isValid();
	return false;
}

void C17GH3State::setTime()
{
	doTimeSend = true;
}

void C17GH3State::sendSettings1() 
{
	static uint32_t nextSend = 0;
	uint32_t millisNow = millis();
	if (millisNow < nextSend)
		return;

	if (!settings1.isValid())
		return;

	nextSend = millisNow + 1000;

	C17GH3MessageSettings1::WiFiState newWifiState = C17GH3MessageSettings1::WIFI_STATE_DISCONNECTED;
	wl_status_t wifiStatus = WiFi.status();
	switch(wifiStatus)
	{
		case WL_NO_SHIELD:
		case WL_IDLE_STATUS:
		case WL_NO_SSID_AVAIL:
		case WL_DISCONNECTED:
		case WL_CONNECTION_LOST:
		case WL_CONNECT_FAILED:
			newWifiState = C17GH3MessageSettings1::WIFI_STATE_DISCONNECTED;
			break;
		case WL_SCAN_COMPLETED:
			newWifiState = settings1.getWiFiState();
			break;
		case WL_CONNECTED:
			newWifiState = C17GH3MessageSettings1::WIFI_STATE_CONNECTED;
			break;
	}

	if ((newWifiState != settings1.getWiFiState()) || (doTimeSend == true))
	{
		if (newWifiState != settings1.getWiFiState())
			logger.addLine(String("Wifi state: ") + String(newWifiState) + String(" Old State:") + String(settings1.getWiFiState()));
			
		doTimeSend = 0;
		C17GH3MessageSettings1 msg;
		msg.setBytes(settings1.getBytes());
		msg.setWiFiState(newWifiState);
		msg.setTxFields(true);
		// wday = 1-7 = mon - sun
		msg.setDayOfWeek(weekday() == 1 ? 7 : weekday() - 1);
		msg.setHour(hour());
		msg.setMinute(minute());
		msg.pack();
		sendMessage(msg);
		logger.addLine(String("Setting Time: Day ") + String(msg.getDayOfWeek()) +  " Time " + String(msg.getHour()) + ":" + String(msg.getMinute()));
	}
}

void C17GH3State::sendSettings2() const
{
	C17GH3MessageSettings2 msg;
	msg.setBytes(settings2.getBytes());
	msg.pack();
	sendMessage(msg);
}

void C17GH3State::processTx()
{
	static uint32_t timeNextSend = 0;
	uint32_t timeNow = millis();
	static C17GH3MessageBase::C17GH3MessageType msgType = C17GH3MessageBase::MSG_TYPE_SETTINGS1;
 
	if (timeNextSend < timeNow)
	{
		if (firstQueriesDone)
			timeNextSend = timeNow + 12000;
		else
			timeNextSend = timeNow + 300;
	
		C17GH3MessageQuery queryMsg(msgType);
		queryMsg.pack();
		sendMessage(queryMsg);
		
		if (C17GH3MessageBase::MSG_TYPE_SCHEDULE_DAY7 == msgType)
		{
			msgType = C17GH3MessageBase::MSG_TYPE_SETTINGS1;
			firstQueriesDone = true;
		}
		else
			msgType = (C17GH3MessageBase::C17GH3MessageType)(msgType + 1);
	}
	
	sendSettings1();
}

void C17GH3State::sendMessage(const C17GH3MessageBase& msg) const
{
	Serial.write(msg.getBytes(), 16);
	logger.addBytes("TX:", msg.getBytes(), 16);
}


C17GH3MessageSettings1::WiFiState C17GH3State::getWiFiState() const
{
	return settings1.getWiFiState();
}

bool C17GH3State::getIsHeating() const
{
	return isHeating;
}
void C17GH3State::setIsHeating(bool heating)
{
	if (isHeating != heating)
	{
		isHeating = heating; 
	}
}

bool C17GH3State::getLock() const
{
	return settings1.getLock();
}

void C17GH3State::setLock(bool locked)
{
	C17GH3MessageSettings1 msg;
	msg.setBytes(settings1.getBytes());
	msg.setTxFields(false);
	msg.setLock(locked);
	msg.pack();
	sendMessage(msg);
}
 
bool C17GH3State::getMode() const
{
	return settings1.getMode();
}

void C17GH3State::setMode(bool mode_)
{
	C17GH3MessageSettings1 msg;
	msg.setBytes(settings1.getBytes());
	msg.setTxFields(false);
	msg.setMode(mode_);
	msg.pack();
	sendMessage(msg);
}
 
bool C17GH3State::getPower() const
{
	return settings1.getPower();
}

void C17GH3State::setPower(bool pow) 
{
	C17GH3MessageSettings1 msg;
	msg.setBytes(settings1.getBytes());
	msg.setTxFields(false);
	msg.setPower(pow);
	msg.pack();
	sendMessage(msg);
}

float C17GH3State::getSetPointTemp() const
{
	return settings1.getSetPointTemp();
}

void C17GH3State::setSetPointTemp(float temperature)
{
	C17GH3MessageSettings1 msg;
	msg.setBytes(settings1.getBytes());
	msg.setTxFields(false);
	msg.setSetPointTemp(temperature);
	msg.pack();
	sendMessage(msg);
}

float C17GH3State::getInternalTemperature() const
{
	return settings1.getInternalTemperature();
}

float C17GH3State::getExternalTemperature() const
{
	return settings1.getExternalTemperature();
}


bool C17GH3State::getBacklightMode() const
{
	return settings2.getBacklightMode();
}

void C17GH3State::setBacklightMode(bool bl)
{
	C17GH3MessageSettings2 msg;
	msg.setBytes(settings2.getBytes());
	msg.setBacklightMode(bl);
	msg.pack();
	sendMessage(msg);
}

bool C17GH3State::getPowerMode() const
{
	return settings2.getPowerMode();
}

void C17GH3State::setPowerMode(bool pm)
{
	C17GH3MessageSettings2 msg;
	msg.setBytes(settings2.getBytes());
	msg.setPowerMode(pm);
	msg.pack();
	sendMessage(msg);
}

bool C17GH3State::getAntifreezeMode() const
{
	return settings2.getAntifreezeMode();
}

void C17GH3State::setAntifreezeMode(bool am)
{
	C17GH3MessageSettings2 msg;
	msg.setBytes(settings2.getBytes());
	msg.setAntifreezeMode(am);
	msg.pack();
	sendMessage(msg);
}

C17GH3MessageSettings2::SensorMode C17GH3State::getSensorMode() const
{
	return settings2.getSensorMode();
}

void C17GH3State::setSensorMode(C17GH3MessageSettings2::SensorMode sm)
{
	C17GH3MessageSettings2 msg;
	msg.setBytes(settings2.getBytes());
	msg.setSensorMode(sm);
	msg.pack();
	sendMessage(msg);
}

float C17GH3State::getTempCorrect() const
{
	return settings2.getTemperatureCorrection();
}

void C17GH3State::setTempCorrect(float temperature)
{
	C17GH3MessageSettings2 msg;
	msg.setBytes(settings2.getBytes());
	msg.setTemperatureCorrection(temperature);
	msg.pack();
	sendMessage(msg);
}

float C17GH3State::getInternalHysteresis() const
{
	return settings2.getInternalHysteresis();
}

void C17GH3State::setInternalHysteresis(float temperature)
{
	C17GH3MessageSettings2 msg;
	msg.setBytes(settings2.getBytes());
	msg.setInternalHysteresis(temperature);
	msg.pack();
	sendMessage(msg);
}

float C17GH3State::getExternalHysteresis() const
{
	return settings2.getExternalHysteresis();
}

void C17GH3State::setExternalHysteresis(float temperature)
{
	C17GH3MessageSettings2 msg;
	msg.setBytes(settings2.getBytes());
	msg.setExternalHysteresis(temperature);
	msg.pack();
	sendMessage(msg);
}

float C17GH3State::getTemperatureLimit() const
{
	return settings2.getExternalSensorLimit();
}

void C17GH3State::setTemperatureLimit(float temperature)
{
	C17GH3MessageSettings2 msg;
	msg.setBytes(settings2.getBytes());
	msg.setExternalSensorLimit(temperature);
	msg.pack();
	sendMessage(msg);
}

String C17GH3State::getSchedule(int day) const
{
	if ((day < 1) || (day > 7))
	  return String("Error");
	C17GH3MessageSchedule s = schedule[day - 1];
	String ret = s.toJson();
    return ret;
}

void C17GH3State::setSchedule(int day, String json)
{
	StaticJsonDocument<1024> jsonDoc;
	
    if ((day < 1) || (day > 7))
	  return; 
	C17GH3MessageSchedule s = schedule[day - 1];
	DeserializationError error = deserializeJson(jsonDoc, json);	
	if (error)
	  return;
	for (int i = 0 ; i < 6; ++i)
	{
		String time = jsonDoc[String("time" + String(i + 1)).c_str()];
		float temp = jsonDoc[String("temp" + String(i + 1)).c_str()];
		int h,m;
		scanf(time.c_str(),"%d:%d", &h, &m);
		s.setTime(i, h, m);
		s.setTemperature(i, temp);
	}
	s.pack();
	sendMessage(s);
}
