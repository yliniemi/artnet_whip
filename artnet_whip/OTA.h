#ifndef OTA_H
#define OTA_H

#include "settings.h"
#include <myCredentials.h>

#include <ArduinoOTA.h>

void setupOTA(char* hostname, int OTArounds);

void setupOTA(char* hostname);

#endif
