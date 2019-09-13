#ifndef NTP_H
#define NTP_H
#include <Ticker.h>

struct  strDateTime
{
  byte hour;
  byte minute;
  byte second;
  int year;
  byte month;
  byte day;
  byte wday;
  unsigned long NTPtime;
} ;

extern strDateTime DateTime;                      // Global DateTime structure, will be refreshed every Second

extern void getNTPtime();
extern void ISRsecondTick();
extern Ticker tkSecond;
extern long customWatchdog; 
#endif