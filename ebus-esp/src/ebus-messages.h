#pragma once

#include "Ebus.h"

struct identification_t {
   char device[6];
   char sw_version[6];
   char hw_version[6];
};

void handleMessage(Ebus::Telegram telegram);
