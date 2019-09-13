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
};

extern strConfig config;
extern void WriteConfig();
extern boolean ReadConfig();
extern void configLoadDefaults(uint16_t ChipId);
#endif