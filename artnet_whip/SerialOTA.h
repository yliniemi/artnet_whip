#ifndef SERIALOTA_H
#define SERIALOTA_H

#include "settings.h"

#include <WiFi.h>

extern WiFiServer telnetServer;
extern WiFiClient SerialOTA;

void SerialOTAhandle();
void setupSerialOTA(char* setHostname);

#endif
