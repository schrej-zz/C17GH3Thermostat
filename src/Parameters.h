#ifndef PARAMETERS_H
#define PARAMETERS_H

struct strConfig {
  boolean dhcp;                         // 1 Byte - EEPROM 16
  boolean isDayLightSaving;             // 1 Byte - EEPROM 17
  long Update_Time_Via_NTP_Every;       // 4 Byte - EEPROM 18
  long timeZone;                        // 4 Byte - EEPROM 22
  byte  IP[4];                          // 4 Byte - EEPROM 32
  byte  Netmask[4];                     // 4 Byte - EEPROM 36
  byte  Gateway[4];                     // 4 Byte - EEPROM 40
  String ssid;                          // up to 32 Byte - EEPROM 64
  String password;                      // up to 32 Byte - EEPROM 96
  String ntpServerName;                 // up to 32 Byte - EEPROM 128
  String DeviceName;                    // up to 32 Byte - EEPROM 160
  String OTApwd;                        // up to 32 Byte - EEPROM 192
  // Application Settings here... from EEPROM 224 up to 511 (0 - 511)
  //mqtt data
  String mqtt_server;                  	// up to 32 Byte - EEPROM 224
  String mqtt_port;                     // up to 32 Byte - EEPROM 256
  String mqtt_username;							    // up to 32 Byte - EEPROM 288
  String mqtt_password;							    // up to 32 Byte - EEPROM 320
  String mqtt_prefix;							      // up to 32 Byte - EEPROM 352
} config;

//  Auxiliar function to handle EEPROM
// EEPROM-parameters

void EEPROMWritelong(int address, long value) {
    byte four = (value & 0xFF);
    byte three = ((value >> 8) & 0xFF);
    byte two = ((value >> 16) & 0xFF);
    byte one = ((value >> 24) & 0xFF);

    //Write the 4 bytes into the eeprom memory.
    EEPROM.write(address, four);
    EEPROM.write(address + 1, three);
    EEPROM.write(address + 2, two);
    EEPROM.write(address + 3, one);
}

long EEPROMReadlong(long address){
    //Read the 4 bytes from the eeprom memory.
    long four = EEPROM.read(address);
    long three = EEPROM.read(address + 1);
    long two = EEPROM.read(address + 2);
    long one = EEPROM.read(address + 3);

    //Return the recomposed long by using bitshift.
    return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

void WriteStringToEEPROM(int beginaddress, String string){
    char  charBuf[string.length() + 1];
    string.toCharArray(charBuf, string.length() + 1);
    for (unsigned int t =  0; t < sizeof(charBuf); t++)
    {
      EEPROM.write(beginaddress + t, charBuf[t]);
    }
}

String  ReadStringFromEEPROM(int beginaddress){
    volatile byte counter = 0;
    char rChar;
    String retString = "";
    while (1)
    {
      rChar = EEPROM.read(beginaddress + counter);
      if (rChar == 0) break;
      if (counter > 31) break;
      counter++;
      retString.concat(rChar);
    }
    return retString;
}

  void WriteConfig()
  {
    EEPROM.write(0, 'C');
    EEPROM.write(1, 'F');
    EEPROM.write(2, 'G');

    EEPROM.write(16, config.dhcp);
    EEPROM.write(17, config.isDayLightSaving);

    EEPROMWritelong(18, config.Update_Time_Via_NTP_Every); // 4 Byte
    EEPROMWritelong(22, config.timeZone); // 4 Byte

    EEPROM.write(32, config.IP[0]);
    EEPROM.write(33, config.IP[1]);
    EEPROM.write(34, config.IP[2]);
    EEPROM.write(35, config.IP[3]);

    EEPROM.write(36, config.Netmask[0]);
    EEPROM.write(37, config.Netmask[1]);
    EEPROM.write(38, config.Netmask[2]);
    EEPROM.write(39, config.Netmask[3]);

    EEPROM.write(40, config.Gateway[0]);
    EEPROM.write(41, config.Gateway[1]);
    EEPROM.write(42, config.Gateway[2]);
    EEPROM.write(43, config.Gateway[3]);

    WriteStringToEEPROM(64, config.ssid);
    WriteStringToEEPROM(96, config.password);
    WriteStringToEEPROM(128, config.ntpServerName);
    WriteStringToEEPROM(160, config.DeviceName);
    WriteStringToEEPROM(192, config.OTApwd);
    WriteStringToEEPROM(224, config.mqtt_server);
    WriteStringToEEPROM(256, config.mqtt_port);
    WriteStringToEEPROM(288, config.mqtt_username);
    WriteStringToEEPROM(320, config.mqtt_password);
    WriteStringToEEPROM(352, config.mqtt_prefix);
    EEPROM.commit();

  }

  boolean ReadConfig(){
    if (EEPROM.read(0) == 'C' && EEPROM.read(1) == 'F'  && EEPROM.read(2) == 'G' )
    {
      config.dhcp = 	EEPROM.read(16);
      config.isDayLightSaving = EEPROM.read(17);
      config.Update_Time_Via_NTP_Every = EEPROMReadlong(18); // 4 Byte
      config.timeZone = EEPROMReadlong(22); // 4 Byte
      config.IP[0] = EEPROM.read(32);
      config.IP[1] = EEPROM.read(33);
      config.IP[2] = EEPROM.read(34);
      config.IP[3] = EEPROM.read(35);
      config.Netmask[0] = EEPROM.read(36);
      config.Netmask[1] = EEPROM.read(37);
      config.Netmask[2] = EEPROM.read(38);
      config.Netmask[3] = EEPROM.read(39);
      config.Gateway[0] = EEPROM.read(40);
      config.Gateway[1] = EEPROM.read(41);
      config.Gateway[2] = EEPROM.read(42);
      config.Gateway[3] = EEPROM.read(43);
      config.ssid = ReadStringFromEEPROM(64);
      config.password = ReadStringFromEEPROM(96);
      config.ntpServerName = ReadStringFromEEPROM(128);
      config.DeviceName = ReadStringFromEEPROM(160);
      config.OTApwd = ReadStringFromEEPROM(192);
      config.mqtt_server = ReadStringFromEEPROM(224);
      config.mqtt_port = ReadStringFromEEPROM(256);
      config.mqtt_username = ReadStringFromEEPROM(288);
      config.mqtt_password = ReadStringFromEEPROM(320);
      config.mqtt_prefix = ReadStringFromEEPROM(352);
      return true;
    }
    else
    {
      return false;
    }
  }

void configLoadDefaults(uint16_t ChipId)
{

  config.ssid = "Thermostat-" + String(ChipId,HEX);       // SSID of access point
  config.password = "" ;                                  // password of access point
  config.dhcp = true;
  config.IP[0] = 192; config.IP[1] = 168; config.IP[2] = 1; config.IP[3] = 100;
  config.Netmask[0] = 255; config.Netmask[1] = 255; config.Netmask[2] = 255; config.Netmask[3] = 0;
  config.Gateway[0] = 192; config.Gateway[1] = 168; config.Gateway[2] = 1; config.Gateway[3] = 254;
  config.ntpServerName = "pool.ntp.org"; 
  config.Update_Time_Via_NTP_Every =  5;
  config.timeZone = 1;
  config.isDayLightSaving = true;
  config.DeviceName = "Thermostat-" + String(ChipId,HEX);;
  config.OTApwd = "";
  config.mqtt_server = "";
  config.mqtt_port = "";
  config.mqtt_username = "";
  config.mqtt_password = "";
  config.mqtt_prefix = "";
  return;
}

#endif