#ifndef __DEVICE_SYSTEM_H__
#define __DEVICE_SYSTEM_H__

// Include main Arduino header file.
#include <Arduino.h>

class Inkplate;
class WiFiServer;
class LinkedList;
class Logging;
class IPAdderss;
class MDNSResponder;

void buzzer(Inkplate *_display, uint8_t n, int _ms);
int deviceInit(Inkplate *_display, char *_ssid, char *_password, WiFiServer *_server, LinkedList *_myList, Logging *_logger, IPAddress _localIP, IPAddress _gateway, IPAddress _subnet, IPAddress _primaryDNS, IPAddress _secondaryDNS);
int fetchTime(Inkplate *_display);
bool updateTime(Inkplate *_display, uint8_t _retries, uint16_t _delay);

#endif