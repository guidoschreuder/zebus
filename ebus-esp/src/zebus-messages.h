#ifndef _ZEBUS_MESSAGES
#define _ZEBUS_MESSAGES

#include "Ebus.h"

struct identification_t {
   char device[6];
   char sw_version[6];
   char hw_version[6];
};

void handleMessage(Ebus::Telegram telegram);

uint8_t sendIdentificationResponse(Ebus::Telegram telegram, uint8_t *buffer);

#endif
